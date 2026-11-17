import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest

from task_engine import TaskEngine


class TaskAcceptance(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.now = 1000.0
        self.engine = TaskEngine(self.root, clock=lambda: self.now)

    def tearDown(self):
        self.engine.close()
        self.temp.cleanup()

    def create(self, key="first"):
        return self.engine.create(key, "local.note", {"text": "明天核对两版原型清单"}, "owner-A", 1)

    def ready(self):
        t = self.create()
        return self.engine.approve(t["id"], t["fingerprint"])

    def test_duplicate_delivery_does_not_duplicate_task_or_extend_expiry(self):
        first = self.create()
        self.now += 20
        second = self.create()
        self.assertEqual(first["id"], second["id"])
        self.assertEqual(first["expires"], second["expires"])
        with self.assertRaises(ValueError):
            self.engine.create("first", "local.note", {"text": "changed"}, "owner-A", 1)

    def test_approval_is_required_and_bound_to_exact_request(self):
        t = self.create()
        with self.assertRaises(ValueError):
            self.engine.execute(t["id"], "owner-A", 1)
        with self.assertRaises(ValueError):
            self.engine.approve(t["id"], "old-or-altered-request")
        self.assertFalse((self.root / "notes").exists())

    def test_permission_or_owner_change_blocks_old_approved_work(self):
        t = self.ready()
        for binding, revision in [("owner-B", 1), ("owner-A", 2)]:
            with self.assertRaises(ValueError):
                self.engine.execute(t["id"], binding, revision)
        self.assertEqual(self.engine.get(t["id"])["status"], "queued")

    def test_expired_work_never_executes(self):
        t = self.ready()
        self.now = t["expires"]
        with self.assertRaises(ValueError):
            self.engine.execute(t["id"], "owner-A", 1)

    def test_cancelled_work_survives_restart_without_side_effect(self):
        t = self.ready()
        self.engine.cancel(t["id"])
        other = TaskEngine(self.root)
        try:
            with self.assertRaises(ValueError):
                other.execute(t["id"], "owner-A", 1)
            self.assertEqual(other.get(t["id"])["status"], "cancelled")
        finally:
            other.close()
        self.assertFalse((self.root / "notes").exists())

    def test_two_workers_cannot_both_execute_one_task(self):
        t = self.ready()
        barrier = threading.Barrier(2)

        def run():
            worker = TaskEngine(self.root, clock=lambda: self.now)
            try:
                barrier.wait()
                try:
                    return worker.execute(t["id"], "owner-A", 1)["status"]
                except ValueError:
                    return "blocked"
            finally:
                worker.close()

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(run) for _ in range(2)]
            result = sorted(f.result(timeout=10) for f in futures)
        self.assertEqual(result, ["blocked", "verified"])
        self.assertEqual(len(list((self.root / "notes").glob("*.txt"))), 1)

    def test_cancel_after_side_effect_is_not_reported_as_undone(self):
        t = self.ready()
        original = self.engine._write_note

        def write_and_cancel(task):
            evidence = original(task)
            second = TaskEngine(self.root, clock=lambda: self.now)
            try:
                second.cancel(task["id"])
            finally:
                second.close()
            return evidence

        self.engine._write_note = write_and_cancel
        result = self.engine.execute(t["id"], "owner-A", 1)
        self.assertEqual(result["status"], "verified")
        self.assertTrue(result["cancel_requested"])

    def test_real_process_exit_after_write_requires_reconciliation_not_retry(self):
        t = self.ready()
        script = """
import os, sys
from task_engine import TaskEngine
e = TaskEngine(sys.argv[1], clock=lambda: 1000)
original = e._write_note
def write_then_exit(task):
    original(task)
    os._exit(9)
e._write_note = write_then_exit
e.execute(sys.argv[2], 'owner-A', 1)
"""
        result = subprocess.run([sys.executable, "-c", script, str(self.root), t["id"]],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 9, result.stderr)
        self.assertEqual(self.engine.get(t["id"])["status"], "executing")
        self.assertEqual(self.engine.recover_stopped_worker(), [t["id"]])
        with self.assertRaises(ValueError):
            self.engine.execute(t["id"], "owner-A", 1)
        result = self.engine.reconcile(t["id"])
        self.assertEqual(result["status"], "verified")
        self.assertEqual(result["result"]["verification_method"], "recovery_readback_sha256")
        self.assertEqual(len(list((self.root / "notes").glob("*.txt"))), 1)

    def test_partial_or_missing_note_is_not_falsely_verified(self):
        t = self.ready()
        self.engine.db.execute("UPDATE tasks SET status='executing' WHERE id=?", (t["id"],))
        self.engine.recover_stopped_worker()
        self.assertEqual(self.engine.reconcile(t["id"])["status"], "outcome_unknown")
        self.engine._note_path(t["id"]).write_text("partial")
        self.assertEqual(self.engine.reconcile(t["id"])["status"], "outcome_unknown")

    def test_unimplemented_external_actions_and_paths_are_rejected(self):
        for tool in ("phone.call", "mijia.set", "shell.run"):
            with self.assertRaises(ValueError):
                self.engine.create("external", tool, {"text": "x"}, "owner-A", 1)
        with self.assertRaises(ValueError):
            self.engine.create("path", "local.note", {"text": "x", "path": "../../outside"}, "owner-A", 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
