## Test 1 — Shared Memory and Data Structures

This test covers basic validation of the shared memory implementation and the data
structures stored within it. The focus is on verifying correct memory allocation,
initialization, access, and cleanup.

The test does **not** examine inter-process communication or synchronization logic.
Instead, it serves as a low-level structural test of the shared memory layout and the
correctness of data handling.

Testing this layer is particularly important, as errors in shared memory structures
are often subtle and difficult to detect through logs alone. If data is corrupted or
incorrectly shared, higher-level system behavior may appear inconsistent or unstable,
making such issues hard to trace during later integration tests.

Successful completion of this test provides a reliable foundation for all subsequent
tests involving process interaction and signal-based communication.

How to run:
```bash
./test_1
```