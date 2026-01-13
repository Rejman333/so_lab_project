## Arguments Parsing and System Limits Test

## Test goal

Verify correct parsing and handling of optional command-line arguments, as well as
proper system behavior under various operating system constraints.

The test focuses on:
- correct interpretation of optional arguments passed to the program
- handling of invalid or missing argument values
- graceful behavior in system-limited scenarios, such as:
    - insufficient number of available semaphores
    - insufficient number of processes allowed by the operating system

---

## Recommended start parameters (clear and readable logs)

These settings allow easy observation of argument parsing results and system error
handling in logs.

```bash
./main_program [optional arguments]