"""Headless local task ledger. No UI, network, phone or hardware integration.

Python >=3.11, standard library only. CLI is a local development interface,
not a remotely authenticated RPC endpoint.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import time
import uuid


def canonical(value):
    return json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(",", ":"))


class TaskEngine:
    def __init__(self, directory, clock=time.time):
        self.root = Path(directory)
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.clock = clock
        self.db = sqlite3.connect(self.root / "tasks.sqlite3", isolation_level=None)
        self.db.row_factory = sqlite3.Row
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.executescript("""
          CREATE TABLE IF NOT EXISTS tasks (
            id TEXT PRIMARY KEY, request_key TEXT UNIQUE NOT NULL,
            fingerprint TEXT NOT NULL, request TEXT NOT NULL,
            status TEXT NOT NULL, approved INTEGER NOT NULL DEFAULT 0,
            cancel_requested INTEGER NOT NULL DEFAULT 0,
            expires REAL NOT NULL, result TEXT);
          CREATE TABLE IF NOT EXISTS events (
            seq INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id TEXT NOT NULL, at REAL NOT NULL, state TEXT NOT NULL);
        """)

    def close(self):
        self.db.close()

    def _event(self, task_id, state):
        self.db.execute("INSERT INTO events(task_id,at,state) VALUES(?,?,?)",
                        (task_id, self.clock(), state))

    def get(self, task_id):
        r = self.db.execute("SELECT * FROM tasks WHERE id=?", (task_id,)).fetchone()
        if r is None:
            raise ValueError("unknown task")
        out = dict(r)
        out["request"] = json.loads(out["request"])
        out["result"] = json.loads(out["result"]) if out["result"] else None
        return out

    def create(self, key, tool, arguments, binding, permission_revision, ttl=3600):
        if tool != "local.note":
            raise ValueError("unsupported tool; phone, calls and home actions not connected")
        if not isinstance(arguments, dict) or set(arguments) != {"text"}:
            raise ValueError("local.note requires exactly text")
        if not isinstance(arguments["text"], str) or not 0 < len(arguments["text"]) <= 10000:
            raise ValueError("text length must be 1..10000")
        if not isinstance(key, str) or not 0 < len(key) <= 128:
            raise ValueError("invalid idempotency key")
        if not isinstance(binding, str) or not binding:
            raise ValueError("binding required")
        if type(permission_revision) is not int or permission_revision < 0:
            raise ValueError("permission_revision must be a non-negative integer")
        if type(ttl) not in (int, float) or not 0 < ttl <= 86400:
            raise ValueError("ttl must be 0..86400 seconds")
        request = dict(tool=tool, arguments=arguments, binding=binding,
                       permission_revision=permission_revision, ttl=ttl)
        raw = canonical(request)
        fingerprint = hashlib.sha256(raw.encode()).hexdigest()
        self.db.execute("BEGIN IMMEDIATE")
        try:
            existing = self.db.execute("SELECT id,fingerprint FROM tasks WHERE request_key=?",
                                       (key,)).fetchone()
            if existing:
                if existing["fingerprint"] != fingerprint:
                    raise ValueError("idempotency key reused for different request")
                task_id = existing["id"]
            else:
                task_id = uuid.uuid4().hex
                self.db.execute("INSERT INTO tasks(id,request_key,fingerprint,request,status,expires) "
                                "VALUES(?,?,?,?,?,?)", (task_id, key, fingerprint, raw,
                                                      "awaiting_approval", self.clock() + ttl))
                self._event(task_id, "awaiting_approval")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return self.get(task_id)

    def approve(self, task_id, fingerprint):
        self.db.execute("BEGIN IMMEDIATE")
        try:
            task = self.get(task_id)
            if task["status"] != "awaiting_approval" or fingerprint != task["fingerprint"]:
                raise ValueError("approval must match exact pending request")
            if self.clock() >= task["expires"]:
                raise ValueError("approval expired")
            self.db.execute("UPDATE tasks SET approved=1,status='queued' WHERE id=?", (task_id,))
            self._event(task_id, "queued")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return self.get(task_id)

    def cancel(self, task_id):
        self.db.execute("BEGIN IMMEDIATE")
        try:
            task = self.get(task_id)
            if task["status"] in ("awaiting_approval", "queued"):
                self.db.execute("UPDATE tasks SET status='cancelled',cancel_requested=1 WHERE id=?",
                                (task_id,))
                self._event(task_id, "cancelled")
            elif task["status"] in ("executing", "outcome_unknown"):
                self.db.execute("UPDATE tasks SET cancel_requested=1 WHERE id=?", (task_id,))
                self._event(task_id, "cancel_requested")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return self.get(task_id)

    def _note_path(self, task_id):
        # Only database-generated identifiers reach this function, never model paths.
        if len(task_id) != 32 or any(c not in "0123456789abcdef" for c in task_id):
            raise ValueError("invalid task identifier")
        folder = self.root / "notes"
        folder.mkdir(exist_ok=True, mode=0o700)
        return folder / (task_id + ".txt")

    def _write_note(self, task):
        path = self._note_path(task["id"])
        data = task["request"]["arguments"]["text"].encode()
        with path.open("xb") as stream:
            stream.write(data)
            stream.flush()
            import os
            os.fsync(stream.fileno())
        # This local check is the completion evidence, not just a successful write return.
        if path.read_bytes() != data:
            raise RuntimeError("saved content mismatch")
        return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
                "verification_method": "readback_sha256", "scope": "local_note_only"}

    def execute(self, task_id, binding, permission_revision):
        self.db.execute("BEGIN IMMEDIATE")
        try:
            task = self.get(task_id)
            if task["status"] != "queued" or not task["approved"]:
                raise ValueError("only approved queued work may execute; unknown outcomes never retry")
            request = task["request"]
            if request["binding"] != binding or request["permission_revision"] != permission_revision:
                raise ValueError("binding or permission changed; re-authorization required")
            if self.clock() >= task["expires"]:
                raise ValueError("task expired")
            self.db.execute("UPDATE tasks SET status='executing' WHERE id=?", (task_id,))
            self._event(task_id, "executing")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        # Commit intent before side effect; a process crash leaves a recoverable executing row.
        try:
            evidence = self._write_note(task)
            status = "verified"
        except Exception:
            # A failure may occur after writing; do not claim failure implies no side effect.
            status, evidence = "outcome_unknown", {"reason": "executor_or_verification_error"}
        self.db.execute("BEGIN IMMEDIATE")
        try:
            self.db.execute("UPDATE tasks SET status=?,result=? WHERE id=?",
                            (status, canonical(evidence), task_id))
            self._event(task_id, status)
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return self.get(task_id)

    def recover_stopped_worker(self):
        """Explicit maintenance action: caller must have stopped the only executor first."""
        self.db.execute("BEGIN IMMEDIATE")
        try:
            ids = [r[0] for r in self.db.execute("SELECT id FROM tasks WHERE status='executing'")]
            for task_id in ids:
                self.db.execute("UPDATE tasks SET status='outcome_unknown' WHERE id=?", (task_id,))
                self._event(task_id, "outcome_unknown")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return ids

    def reconcile(self, task_id):
        task = self.get(task_id)
        if task["status"] != "outcome_unknown":
            raise ValueError("reconcile only unknown work")
        path = self._note_path(task_id)
        expected = task["request"]["arguments"]["text"].encode()
        if not path.is_file() or path.read_bytes() != expected:
            return task  # Absence or partial content is not permission to execute again.
        evidence = {"path": str(path), "sha256": hashlib.sha256(expected).hexdigest(),
                    "verification_method": "recovery_readback_sha256", "scope": "local_note_only"}
        self.db.execute("BEGIN IMMEDIATE")
        try:
            self.db.execute("UPDATE tasks SET status='verified',result=? WHERE id=?",
                            (canonical(evidence), task_id))
            self._event(task_id, "verified_after_reconciliation")
            self.db.execute("COMMIT")
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        return self.get(task_id)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--state", required=True, help="private local state directory")
    p.add_argument("command", choices=["create", "approve", "execute", "cancel", "get", "recover", "reconcile"])
    p.add_argument("--id")
    p.add_argument("--key")
    p.add_argument("--text")
    p.add_argument("--fingerprint")
    p.add_argument("--binding", default="local-development-owner")
    p.add_argument("--permission-revision", type=int, default=1)
    a = p.parse_args()
    engine = TaskEngine(a.state)
    try:
        if a.command == "create":
            result = engine.create(a.key, "local.note", {"text": a.text}, a.binding, a.permission_revision)
        elif a.command == "approve":
            result = engine.approve(a.id, a.fingerprint)
        elif a.command == "execute":
            result = engine.execute(a.id, a.binding, a.permission_revision)
        elif a.command == "recover":
            result = engine.recover_stopped_worker()
        else:
            result = getattr(engine, a.command)(a.id)
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except ValueError as exc:
        p.exit(2, str(exc) + "\n")
    finally:
        engine.close()


if __name__ == "__main__":
    main()
