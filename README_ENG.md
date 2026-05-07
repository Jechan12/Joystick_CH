# Joystick_CH

This repository provides a C++ library for reading and processing joystick input on Linux (`/dev/input/js*`). It supports:

- Low-pass filtering (Hz-independent Time Constant $\tau$)
- Dead-zone handling  
- Normalization to [–1, +1]  
- Quadratic “ramp-up” scaling  
- Optional slew-rate limiting (Hz-independent)
- Accumulative button counters (Time-based integration)
- **Initialization gating** (Time + START button)
- **Kill Switch (E-Stop)** support
- Thread-safe state access via `std::mutex`

![Joystick axis num](./images/joystickAxisNum.png)

## Features

- **State Processing**  
  A dedicated background thread runs `joy::runJoystickThread(running)` to:
  1. Open the joystick device (`CONFIG_JOYSTICK_DEVICE`)
  2. Handle automatic disconnection and reconnection logic.
  3. Safely update internal states protected by `std::mutex`.

- **Hz-Independent Low-Pass Filter & Slew-Rate**  
  - The filter uses a time constant `CONFIG_FILTER_TAU` rather than a fixed alpha, ensuring the same control feel regardless of loop frequency.
  - Slew-rate is implemented as `RatePerSecond * dt`.

- **Accumulative Button Counters**  
  - L1/R1 and L2/R2 buttons increase/decrease virtual axes using a time-based rate (`CONFIG_ACCUM_RATE`), ensuring smooth and consistent accumulation over time.

- **Initialization & Kill Switch**  
  - **INIT**: The library ignores axis updates until `CONFIG_INIT_DELAY_SEC` has passed AND the `CONFIG_BUTTON_START` is pressed.
  - **KILL**: Pressing `CONFIG_BUTTON_KILL` (e.g., SELECT/SHARE) instantly disables inputs and zeroes all outputs until START is pressed again.

- **Thread-Safe Consumer API**
  - External loops (like the robot controller) do not access variables directly. They call `joy::JoystickState state = joy::getJoystickState();` to get a thread-safe copy of the latest joystick state, avoiding data tearing and race conditions.

## File Structure
```plaintext
.
├── demo/
│   └── main.cpp           # Demo: spawns readJoystickEvents thread and prints state
├── images/
│   └── joystickAxisNum.png
├── joystick.h             # Public API, tunable constants & init gating flags
└── joystick.cpp           # Internal helpers & event-loop implementation
```

### joystick.h

- **Tunable constants (User Configuration Area)**  
  - `CONFIG_JOYSTICK_DEVICE`, `CONFIG_JOYSTICK_HZ`
  - `CONFIG_INIT_DELAY_SEC`, `CONFIG_BUTTON_START`, `CONFIG_BUTTON_KILL`
  - `CONFIG_FILTER_TAU`, `CONFIG_ACCUM_RATE`, `CONFIG_DEFAULT_DEADZONE`
- **Public types & API**  
  - `struct JoystickState { float axes[MAX_AXES]; int buttons[MAX_BUTTONS]; float lr1_accumulated; float lr2_accumulated; }`
  - `JoystickState getJoystickState();` (Thread-safe getter)
  - `void runJoystickThread(bool &continueJoystickThread);`

### joystick.cpp

- Implements the background thread `runJoystickThread(...)`.
- Internal state `static JoystickState head_shared;` is protected by `std::mutex joystick_mutex;`.
- Handles USB/Bluetooth disconnection smoothly without crashing.

### demo/main.cpp

- Spawns a `std::thread` running `joy::runJoystickThread`.
- Continuously calls `joy::getJoystickState()` to print the latest axes, buttons, and accumulated values.

## Building & Running

**Prerequisites**  
- Linux with `/dev/input/js*` joystick support  
- A C++17 compiler (e.g. `g++`)  
- POSIX Threads

```bash
cd demo

# Build the demo using the provided Makefile
make

# Run
./joystick_test
```