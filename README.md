# Drone Swarm Simulation — Repor


## 1. Project Goal

The goal of this project was to simulate the lifecycle of an autonomous drone swarm with a base, limited docking
capacity,
two one-way doors, battery constraints, operator replenishment,
and commander signals that dynamically change system capacity or trigger a suicide attack for a specific drone.  
The simulation produces a textual log.

---

## 2. Requirements Summary (Specification Mapping)

### Core parameters

- **N** — initial number of drones in the swarm
- **P** — maximum number of drones that can be inside the base at the same time (**P < N/2**)
- **T1i** — maximum allowed time spent charging inside the base before leaving (overheating constraint)
- **T2i** — maximum flight time on one full battery (**T2i = 2.5 · T1i**)
- **Tk** — operator replenishment interval
- **Xi** — maximum number of charging cycles per drone (after that the drone is decommissioned)

### Actors

- **System Commander**
    - `s1`: increase maximum number of drones in base up to **2N** (add platforms)
    - `s2`: decrease maximum number of drones in base by **50%** (remove platforms)
    - `s3`: order a specific drone to perform a **suicide attack** (ignored if battery ≤ 20%)

- **Operator**
    - Every **Tk**, attempts to replenish missing drones if there is space in the base
    - Reacts to commander signals

- **Drone**
    - State: battery level, return threshold, location (Base | Mission), number of charges
    - Leaves base after **T1i**
    - Can fly up to **T2i**
    - Automatically returns when battery < **20%**
    - Destroyed if battery reaches **0%** during flight
    - Decommissioned after exceeding **Xi** charging cycles
    - Reacts to `s3` only if battery > 20%

- **Base Doors**
    - Two doors, each can pass **one drone at a time**
    - At any moment, movement is allowed only in **one direction** (either entering or exiting)

---

## 3. Design Assumptions (Implementation Decisions)

To translate the specification into code, the following implementation-specific assumptions were made due to ambiguities
in the original task description:

1. **Signal handling model**
    - Commander signals (`s1`, `s2`, `s3`) are implemented as **state flags** rather than immediate actions.
    - Setting a signal flag guarantees that the requested action will eventually be executed, but **not necessarily in
      the same simulation step**.
    - **A small execution delay may occur depending on the current simulation state and initial parameter values.**

2. **Drone creation and replenishment policy**
    - The operator attempts to create new drones.
    - The final decision whether a drone can actually be created is deferred to the drone logic.
    - This approach avoids hard blocking on the operator side and simplifies global capacity management.

3. **Gate (door) safety constraint**
    - A drone **cannot be destroyed, decommissioned, or forced to perform a suicide attack** while it is in the process
      of passing through a gate. This is because of the nature of handling signals as flags.
    - Any terminal action (battery depletion, suicide command, or decommissioning) is deferred until the drone has fully
      exited or entered the base.
    - This guarantees consistent state transitions and prevents invalid intermediate states.

4. **Simulation termination conditions**
    - The simulation ends when all of the following conditions are met:
        - The **allowed maximum number of drones in base drops to 0** (e.g. due to repeated `s2` signals),
        - All currently active drones have fully depleted their batteries and are removed from the simulation.

---

## 4. High-Level Code Overview

The project is implemented in C and organized around multiple cooperating processes communicating via IPC mechanisms.
Each major responsibility is separated into a dedicated module or process, allowing clear ownership of logic and
simplified synchronization.

### Main Process (`main.c`)

The `main.c` module is responsible for global system setup and teardown:

- Parsing args.
- Initializes all IPC resources at startup:
    - shared memory segments,
    - semaphores,
    - FIFO
- Initializes a global logging mechanism shared by all components.
- Spawns child processes:
    - the **Operator** process,
    - the **System Commander** process.
- Waits for simulation termination and performs orderly cleanup:
    - terminates remaining processes,
    - releases IPC resources,
    - closes and finalizes logs.

The main process does not participate directly in the simulation logic; it acts purely as a coordinator and resource
manager.

---

### Drone Module (`dron.c`)

The `dron.c` module contains the complete logic of an individual drone and represents the core of the simulation.

Responsibilities include:

- Maintaining drone state:
    - battery level,
    - current location (Base / Map),
    - flight and charging timers,
    - number of completed charging cycles,
- Executing the drone lifecycle:
    - leaving the base after `T1i`,
    - flying up to `T2i`,
    - initiating return when battery < 20%,
    - destruction when battery reaches 0 during flight,
    - decommissioning after exceeding `Xi` charging cycles.
- Reacting to commander signals (e.g. suicide attack), subject to safety constraints.
- Coordinating movement through base gates using IPC FIFO.

Each drone operates autonomously once created and progresses independently through its state machine.

---

### Operator Process (`operator.c`)

The operator process encapsulates logic related to drone replenishment and capacity management.

Its responsibilities include:

- Creates starting drones.
- Periodically attempting to create new drones every `Tk`.
- Reacting to System Commander signals that modify the allowed maximum number of drones.

The operator does not directly control drone behavior after creation and does not forcibly remove active drones when
capacity is reduced.

---

### System Commander Process (`system_commander.c`)

The system commander process executes a predefined **simulation plan** that drives global system behavior.

The plan consists of sequential phases:

1. Increasing the allowed number of drones up to the configured maximum.
2. Sending suicide commands (`s3`) to selected drones during active operation.
3. Gradually reducing the allowed maximum number of drones down to zero.

---

### IPC Library (`ipc/`)

The `ipc/` directory contains a custom wrapper library used throughout the project to abstract low-level inter-process
communication.

It includes:

- Semaphore wrappers for mutual exclusion and synchronization.
- Shared memory abstractions with logic specific to the drone swarm problem.
- FIFO-based communication.

Notably, FIFO mechanisms are extended to function similarly to semaphores and are used to **simulate gate behavior**.

---

### Utility Data Structures (`data_structures/`)

This directory provides supporting data structures required by the simulation logic.

- `stack.c / stack.h` — a simple stack implementation used internally by selected components.

---

### Utilities (`utils/`)

The `utils/` directory contains auxiliary libraries that support debugging and observability rather than core simulation
logic.

- `printer.c / printer.h` — a utility library responsible for colored terminal output and general-purpose logging,
  shared across multiple processes.

## 5. Main Problems Encountered

During the development of the project, several non-trivial technical and design issues were encountered.

1. **Development environment setup**
    - The project was developed on Windows using WSL.
    - Building the entire system and making IPC mechanisms work as intended required a deeper understanding of CMake and
      the Linux IPC model.
    - An unexpected limitation of WSL1 (`mq_open: Function not implemented`) forced a migration to WSL2.

2. **Incorrect early architectural decisions**
    - The initial use of process groups was a poor design choice that significantly complicated the implementation.
    - Process groups did not provide any real benefit and interfered with signal handling and process control.
    - Eventually, this approach was abandoned in favor of direct parent–child signal propagation.

3. **Drone counting and base occupancy management**
    - Designing a reliable counter for drones currently inside the base proved difficult.
    - The counter had to support safe decrementing while allowing the simulation to continue without deadlocks or
      inconsistent states.
    - Several early implementations led to drones becoming unable to enter the base.

4. **Signal handling design**
    - The first signal-handling implementation relied heavily on logic executed directly inside signal handlers.
    - This approach caused subtle and hard-to-debug errors when multiple signals collided or arrived in close
      succession.
    - The solution was to simplify handlers and move logic into the main execution flow using shared flags.

5. **Semaphore and signal interaction**
    - Signals interrupting semaphore waits caused unexpected behavior in multiple parts of the system.
    - Handling interrupted waits correctly required additional synchronization logic and retry strategies.

6. **Refactoring under time pressure**
    - Due to accumulated complexity and unstable behavior, a partial rewrite of the codebase became necessary.
    - The final version relies more heavily on global variables to simplify access to shared state and improve overall
      code clarity.

Overall, many of these problems stemmed from early architectural assumptions that only revealed their drawbacks once
IPC,
signals, and synchronization mechanisms interacted at scale.

## 6. Shared Memory Design for Drone Management

The simulation uses shared memory (SHM) as the primary mechanism for managing drone instances and global simulation
state across multiple processes.

Two shared memory regions are defined:

1. **Index Stack (SHM Stack)**
    - This region implements a stack structure that stores **free drone indices**.
    - Before a new drone can be created, a free index must be obtained by popping it from the stack.
    - When a drone is destroyed or decommissioned, its index is pushed back onto the stack, making it available for
      reuse.
    - This approach prevents uncontrolled growth of drone identifiers and allows efficient recycling of resources.

2. **Drone and Simulation State (SHM AllDronesData)**
    - This region stores:
        - per-drone data (id, pid, location),
        - global simulation parameters and counters related to the swarm.
    - Each drone accesses and updates only its own assigned slot, identified by the index obtained from the stack.

Both shared memory regions are protected by a **single semaphore**, which guarantees mutual exclusion and ensures
consistency across the stack and drone state data.
Treating both structures as one synchronized unit prevents race conditions such as assigning the same index to multiple
drones or accessing partially updated state.

---

### Build Commands and Environments details

```
Distributor ID: Debian
Description:    Debian GNU/Linux 11 (bullseye)
Release:        11
Codename:       bullseye

CMAKE_VERSION 3.18.4
CMAKE_C_STANDARD 11
gcc version 8.5.0 (GCC)
```

```bash
# Example:
cmake -S . -B build
cmake --build build -j
cd build/ #log.txt will be created in here

# Execute with default params
./main_program
# Or if you who'd like to see supported args
./main_program -h
```

The **-h** option displays a list of supported command-line parameters and their descriptions.
Running the program without any arguments starts the simulation using default parameter values.

---

## Code References (GitHub Permalinks)

### a) File creation and I/O

**Required functions:** `creat()`, `open()`, `close()`, `read()`, `write()`, `unlink()`

- `printer.h` and `printer.h` is a library for loging and printing messages. To understand how working with files
  and I/O works it is best to take a look at the library.
    - [printer.h](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/utils/printer.h#L1-L48)
    - [printer.c](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/utils/printer.c#L1-L200)
    - Only main process cals `global_logger_initialize()`, others just use `logger_initialize()`

---

### b) Process creation and management

**Required functions:** `fork()`, `exec*()`, `exit()`, `wait*()`

- `main.c: parse_positive_int()`
    - [exit()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L65-L93)
- `main.c: process_argv()`
    - [exit()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L158-L164)
- `main.c: creat_operator()`
    - [Creation of operator process](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L198-L224)
    - We must reset the signal mask because child processes inherit ignored signals from the parent.
- `main.c: creat_system_commander()`
    - [Creation of system commander process](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L226-L265)
    - We must reset the signal mask because child processes inherit ignored signals from the parent.
- `main.c: close_main()`
    - [Joining system commander](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L433-L448)
    - [Joining operator](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L449-L465)
    - [exit()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L534)
- `operator.c creat_dron()`
    - [Creation of drone process](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L49-L77)
- `operator.c close_main()`
    - [This function waits for children, and exits](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L112-L139)
- `operator.c main()`
    - [To prevent zombie processes we set this](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L246-L255)
    - I know its signals, but i fell like it feets more this category.

In my project, new processes are created by the main process and by operator processes. Process termination occurs
either when `fork()` fails or inside a function usually `called close_main()`.
`_exit()` is used when the process must terminate immediately without running cleanup handlers.

---

### c) Threads and thread synchronization (POSIX threads)

**Required functions:** `pthread_create()`, `pthread_join()`, `pthread_detach()`, `pthread_exit()`,  
`pthread_mutex_lock()`, `pthread_mutex_unlock()`, `pthread_mutex_trylock()`,  
`pthread_cond_wait()`, `pthread_cond_signal()`, `pthread_cond_broadcast()`

- `dron.c: main()`
    - [creating thread](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L635-L638)
- `dron,c battery()`
    - [This is a function running in a battery thread](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L271-L289)
- `dron.c: close_main()`
    - [pthread_join()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L98-L103)
- `dron.c mutex uses`
    - [process_sigusr1()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L332-L334)
    - [battery_state_check()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L360-L362)
    - [force_base_return()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L385-L387)
    - [force_leave_base()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L466-L468)
    - [describe_self()](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L500-L502)

There is only one thread in the drone process, responsible for updating the battery state. Communication between
processes is synchronized using a mutex.


---

### d) Signal handling

**Required functions:** `kill()`, `raise()`, `signal()`, `sigaction()`

- `main.c: main()`
    - [Ignoring SIGINT](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L539)
    - This process should not be terminated, as it is responsible for releasing critical resources.

All signal handlers are set in the `main()` function of each process. I will link these sections as larger code blocks,
one for each process.

Signals are handled using the *flag approach*, where a signal handler only sets a flag and the signal is processed later
during normal program execution.

- `system_commander.c: main()`
    - [Setting up signal handlers for SIGINT and SIGTERM](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/system_commander/system_commander.c#L307-L322)
- `operator.c: main()`
    - [Setting up signal handlers for SIGINT and SIGTERM, SIGUSR1, SIGUSR2. And system for not creating zombies](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L246-L293)
- `dron.c: main()`
    - [Setting up signal handlers for SIGINT and SIGTERM, SIGUSR1](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L575-L601)

---

### e) Process / thread synchronization (System V semaphores)

**Required functions:** `ftok()`, `semget()`, `semctl()`, `semop()`

Working with `ftok()` is part of the `ipc.h` library and is implemented in the `key.c` file.

- [Implementation of
  `ftok()` and related file operations](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/ipc/key.c#L9-L33)

Semaphores are discussed in more detail in the **Shared Memory** section, as they are exclusively used for synchronizing
access to shared data.

The only exception is a semaphore used for the gate mechanism, which is currently deprecated and remains in the codebase
solely for testing and historical reasons.

---

### f) Named and unnamed pipes

**Required functions:** `mkfifo()`, `pipe()`, `dup()`, `dup2()`, `popen()`
**and pipes specific File I/O operations**

- FIFOs in my project are part of the `ipc.h` library. Below, I provide links to the implementation of the relevant
  system
  functions, along with examples showing where the FIFO responsible for the gate is created, destroyed, and used.
- `ipc.h`
    - [Function declarations](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/ipc/ipc.h#L106-L124)
- `fifo.c`
    - [Implementation](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/ipc/fifo.c#L1-L263)
    - I decided to provide a link to all functions, as evry one of them has something to do with FIFOs
- `main.c: creat_gate_fifo_sem()`
    - [Example of creating fifo, using my library](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L384-L390)
    - [Example of deleting fifo, using my library](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L523-L530)
- `dron.c pass_the_gate_fifo()`
    - [Using FIFO to pass the gate inside drone](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L308-L328)

As can be seen FIFOs in my project are just working like a semaphore with starting value of 2.

---

### g) Shared memory segments (System V SHM), and semaphores

**Required functions:** `ftok()`, `shmget()`, `shmat()`, `shmdt()`, `shmctl()`

All shared memory operations and semaphores are part of the `ipc.h` library.
For this reason, I provide links to the `ipc.h`, `shm.c`, and `semaphore.c` files.
The remaining links serve as examples of how the library is used in practice.

Only the most relevant and interesting cases are included; otherwise, the number of links would be excessive and would
reduce the overall clarity and usefulness of this section.

[ipc.h](src/ipc/ipc.h), [semaphore.c](src/ipc/semaphore.c), [shm.c](src/ipc/shm.c)

- `main.c: creat_shm_config(), creat_shm_all_drones_data()`
    - [This functions creates shared memory used in the project and the semaphore guarding it.](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L267-L367)
- `main.c: close_main()`
    - [Hear shared memory, and semaphores are being deleted](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/main.c#L466-L515)

“The system commander, operator, and drone have similar mechanisms. Each has its own
`get_configuration_from_shm_config()`
and get_shm_all_drones_data(), which are cleaned up in `close_main()`. The difference is that they use get functions,
which cannot create shared memory or semaphores. They also do not delete these resources on exit—only detach from them.”

Using operator as an example:

- `operator.c: get_configuration_from_shm_config(), get_shm_all_drones_data()`
    - [Opening shm and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L141-L236)
- `operator.c: close_main()`
    - [Closing shm, and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L112-L129)
- `operator.c: selected examples of shm usage`
    - [Changing maximum_dron_in_base 1](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L315-L330)
    - [Changing maximum_dron_in_base 2](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L338-L349)
    - [Checking if calling creat_drone() makes sense](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L360-L379)
    - [Describing current simulation state](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/operator/operator.c#L91-L110)

---

- `system_commander.c: get_shm_all_drones_data()`
    - [Opening shm and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/system_commander/system_commander.c#L69-L110)
- `system_commander.c: cleanup_ipc_attachments()`
    - [Closing shm, and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/system_commander/system_commander.c#L270-L288)
- `system_commander.c: selected examples of shm usage`
    - [Checking value of maximum_dron_in_base_count](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/system_commander/system_commander.c#L219-L257)
    - [Using SHM_AllDronesData_get_dron_pid() to get dron pid from shm](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/system_commander/system_commander.c#L142-L156)

---

-
`dron.c: get_configuration_from_shm_config(), get_shm_all_drones_data(), get_gate_semaphore() is a legacy code and its not being used`
    - [Opening shm and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L136-L260)
- `dron.c: close_main()`
    - [Closing shm, and semaphores](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L70-L96)
- `dron.c: selected examples of shm usage`
    - [Deleting drone from shm 1](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L337-L348)
    - [Deleting drone from shm 2](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L365-L376)
    - [Deleting drone from shm 3](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L391-L402)
    - [Process of entering the base, this is good to have a look at](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L410-L455)
    - [Process of leaving the base](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L482-L492)
    - [This function is responsible for adding new drones to shm](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/dron/dron.c#L538-L563)

As can be seen, working with shared memory is a major part of the project.  
The crucial functions from `ipc,h` that are necessary to understand the project are shown here:  
[link](https://github.com/Rejman333/so_lab_project/blob/39b2a275b233227a295992a58f00718cbf9010a3/src/ipc/shm.c#L66-L148)

They focus on operating on my data structures stored inside shared memory.


# Tests

All test can be found in the `src/tests` directory. All of them have descriptions inside.

- [test_1.md](src/tests/test_1.md)
- [test_2.md](src/tests/test_2.md)
- [test_3.md](src/tests/test_3.md)
- [test_4.md](src/tests/test_4.md)

---

## Known Problems

Depending on the starting conditions and the number of drones operating simultaneously,
issues may occur such as **delayed signal handling** due to the operating system not allocating
sufficient time to each drone. This can result in the need to press **Ctrl + Z** two or three times to close the
application.

If the gate time is set to a long duration, the program may take longer to terminate. Drones cannot be stopped once they
enter a gate; they will complete their gate activity before shutting down.


