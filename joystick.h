#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <linux/joystick.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cstdlib>
#include <cmath>   // for std::fabs
#include <chrono>  // for time measurement

// Debug real-time output control (uncomment to enable output)
// #define Data_print

// Define maximum number of axes and buttons (modify if needed)
#define MAX_AXES 8
#define MAX_BUTTONS 13

// Shared state variable: Data to be read by the controller thread
struct JoystickState {
    double axes[MAX_AXES];   // Normalized values after applying low-pass filter
    int buttons[MAX_BUTTONS]; // Button states (0 or 1)
};

// Global shared state variable for axes/buttons
extern JoystickState head_shared;

// Shared accumulative variables for button counts:
// For L1 (button index 4) and R1 (button index 5): pressing L1 decrements, R1 increments.
extern double lr1_accumulated;

// For L2 (button index 6) and R2 (button index 7): pressing L2 decrements, R2 increments.
extern double lr2_accumulated;

// Normalize the raw value to a double in the range -1.0 ~ 1.0
double normalizeAxisValue(int raw);

/*
   scaleJoystickOutput:
   - Processes a normalized joystick value in the range [-1, 1].
   - Returns 0 if the absolute value is below the dead zone threshold.
   - For values above the threshold, subtracts the dead zone, scales the remaining portion to [0, 1],
     applies a quadratic curve for a gradual ramp-up, and then restores the original sign.
   - This function ensures a smooth transition from zero input to full output.
*/
double scaleJoystickOutput(double normalized, double deadZoneThreshold);

// Low-pass filter function: Interpolates between previous and current values using the alpha coefficient
double lowpassFilter_Joy(double previous, double current, double alpha);

/*
   updateSharedState:
   - For each axis, update the local low-pass filter state using raw data from localState.
   - Then, normalize the filtered value (mapping the raw range to [-1,1]) and apply scaling.
   - The final processed values are stored in head_shared.
   (Button states are simply copied.)
*/
void updateSharedState(const JoystickState &rawState, double alpha, double deadZoneThreshold);

// Continuously reads joystick events, updates the local state, and passes the result to head_shared.
// Also updates the accumulative variables for L1/R1 and L2/R2 based on button events.
void readJoystickEvents();

#endif // JOYSTICK_H
