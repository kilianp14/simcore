# Simcore

[![CI](https://github.com/kilianp14/simcore/actions/workflows/ci.yml/badge.svg)](https://github.com/kilianp14/simcore/actions/workflows/ci.yml)

Simcore is a small deterministic discrete-event simulation kernel written in C11. It provides a C API and Python bindings for scheduling events on a logical timeline and executing callbacks in a predictable order.

## Features

- **Deterministic ordering:** events execute by ascending simulation time, with event ID as the tie-breaker.
- **Flexible scheduling:** schedule at an absolute time or after a delay from the current time.
- **Cancellation:** cancel pending events by ID.
- **Bounded execution:** run until the queue is empty or until an inclusive simulation-time limit.
- **Callback-driven behavior:** callbacks can schedule or cancel events while a simulation is running.
- **Callback-context cleanup:** optionally register a cleanup callback for event data.

## C API

### Requirements

- C11-compatible compiler (Clang or GCC)
- CMake 3.31 or newer
- Ninja, used by the included CMake presets

### Building and Testing
Configure, build, and run the tests from the root directory:
```sh
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

### API Overview

| Function | Description |
| --- | --- |
| `sim_create` | Create a simulator with its clock initialized to `0`. |
| `sim_destroy` | Destroy the simulator and clean up pending event contexts. |
| `sim_now` | Read the current simulation time. |
| `sim_schedule_at` | Schedule an event at an absolute simulation time. |
| `sim_schedule_after` | Schedule an event after a delay from the current simulation time. |
| `sim_cancel` | Cancel a pending event by ID. |
| `sim_run` | Execute events until the queue is empty. |
| `sim_run_until` | Execute events whose times are less than or equal to a specified time. |
| `sim_status_string` | Get a human-readable description of a status code. |

See [`include/sim/sim.h`](include/sim/sim.h) for the full API contract.

## Python API

### Requirements

- Python 3.13 or newer
- uv
- A C compiler and the associated build tools

CFFI is installed automatically as a package dependency.

### Building and Testing
Configure, build, and run the tests from the root directory:
```sh
cd python
uv sync --locked --dev
uv run pytest
```

### API Overview

The `simcore` package exposes the `Simulator` class, which manages a native simulation instance.

| Member | Description |
| --- | --- |
| `Simulator()` | Create a simulator with its clock initialized to `0`. |
| `sim.now` | Read the current simulation time in ticks. |
| `sim.schedule_at(time, callback)` | Schedule a callback at an absolute simulation time. |
| `sim.schedule_after(delay, callback)` | Schedule a callback after a delay from the current simulation time. |
| `sim.cancel(event_id)` | Cancel a pending event by ID. |
| `sim.run(until=None)` | Execute events until the queue is empty, or until the inclusive time limit when `until` is specified. |
| `sim.close()` | Destroy the simulator and release its resources. |
| `with Simulator()` | Manage simulator lifetime through a context manager. |

Callbacks receive the simulator instance, event ID, and event execution time. Exceptions raised by callbacks are propagated to the caller of `run()`.
