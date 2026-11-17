Portable task core v0.1 — engineering implementation, not complete firmware.

Host acceptance command (run from this directory):
clang -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined task_core.c acceptance.c -o /tmp/idc-task-core-check
/tmp/idc-task-core-check

The ESP-IDF component deliberately excludes acceptance.c. No ESP toolchain build,
flashing, radio/call/audio/device test has been performed.

All API calls must be serialized. Before dispatching an effect, atomically persist
updated state; a persistence error must prevent dispatch. Restore validated journal
records before idc_on_reboot(). Do not write the raw host struct as a portable file
format: use a versioned CBOR/binary encoding with integrity and field checks.
The storage layer and deserializer are not implemented here.

The coordinator must enforce uniqueness of task_id and the full request digest.
The 64-bit fingerprint in this core is not an authentication mechanism.
The adapter must authenticate receipts and bind them to the token. Nonzero receipt
IDs only prevent absent-result success; they do not verify a signature or actual
physical effect. An IR send receipt must not be described as TV-state confirmation.

The bounded opcode table is a basis for declarative skills, not a complete script
sandbox or installer. Manifest parsing, signatures, parameters, quotas, OTA/rollback,
phone/provider adapters and FreeRTOS integration remain explicit implementation work.
