# Joystick_CH

This repository contains C++ functions for reading and processing joystick input on Linux systems (using the `/dev/input/js*` interface). It includes features such as low-pass filtering, normalization, dead zone handling, scaling (with gradual ramp-up), slew rate limiting, and accumulative button counts for specific buttons.


![Joystick axis num](./images/joystickAxisNum.png)

## Features

- **Joystick State Processing:**  
  Reads raw joystick events and stores normalized axis values (range: -1 to 1) along with button states.

- **Low-Pass Filtering & Dead Zone:**  
  Applies a low-pass filter to smooth out noisy raw values, then normalizes them and applies a dead zone threshold.

- **Scaling & Slew Rate Limiting:**  
  Scales the normalized values with a quadratic ramp-up for a smooth transition from 0 to full output, and limits the rate of change to prevent abrupt movements.

- **Accumulative Button Counters:**  
  Maintains two global accumulative variables:  
  - **lr1_accumulated:** For L1 (button index 4) and R1 (button index 5) — L1 decrements and R1 increments the value.  
  - **lr2_accumulated:** For L2 (button index 6) and R2 (button index 7) — L2 decrements and R2 increments the value.  
  These values are clamped between -1.0 and 1.0.

- **Real-Time Loop at 1 kHz:**  
  The event loop measures its execution time and sleeps for the remaining time, ensuring each iteration takes exactly 1ms.

## File Structure

- **joystick.h:**  
  Contains the declarations for the `JoystickState` structure, global shared variables, and function prototypes for normalization, filtering, scaling, accumulative button updates, and reading joystick events.

- **joystick.cpp:**  
  Implements the joystick processing functions:
  - Low-pass filter (`lowpassFilter_Joy`)
  - Scaling function (`scaleJoystickOutput`)
  - Slew rate limiter (`applySlewRate`)
  - State update function (`updateSharedState`)
  - Joystick event loop (`readJoystickEvents`), which also updates accumulative button counters

- **main.cpp:**  
  A demo application that launches the joystick event thread and periodically prints the current shared joystick state (axes and buttons) and the accumulative button values.

## Building the Project

### Prerequisites

- A Linux environment with joystick support (e.g., a PS4 DualShock or similar device)
- A C++ compiler supporting C++11 or later (e.g., `g++`)
- POSIX threads support

### Compile from the Command Line

If all files (`main.cpp`, `joystick.cpp`, and `joystick.h`) are in the same directory (or arranged with `joystick.cpp` and `joystick.h` in the parent directory of `demo`), navigate to the appropriate directory and run:

```bash
g++ -std=c++11 -I.. main.cpp ../joystick.cpp -o joystick_test -pthread

or

cd demo

./joytest
