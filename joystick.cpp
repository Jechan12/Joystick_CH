#include "joystick.h"

// Global shared state variable definition (must be defined exactly once)
JoystickState head_shared = {0};

// New accumulative variables for button counts
double lr1_accumulated = 0.0;
double lr2_accumulated = 0.0;

// Low-pass filter function (exponential moving average)
double lowpassFilter_Joy(double previous, double current, double alpha) {
    return previous + alpha * (current - previous);
}

// Processes a normalized joystick value ([-1, 1]).
// Returns 0 if the absolute value is below the dead zone threshold.
// Otherwise, maps the value from [deadZoneThreshold, 1] to [0, 1] and applies quadratic scaling
// for a smooth, gradual ramp-up while preserving the sign.
double scaleJoystickOutput(double normalized, double deadZoneThreshold) {
    double absVal = std::fabs(normalized);
    if (absVal < deadZoneThreshold) {
        return 0.0;
    }
    // Compute adjusted value: linearly map [deadZoneThreshold, 1] to [0, 1]
    double adjusted = (absVal - deadZoneThreshold) / (1.0 - deadZoneThreshold);
    // Apply a quadratic scaling for a smoother, gradual ramp-up.
    adjusted = std::pow(adjusted, 2.0);
    return (normalized >= 0) ? adjusted : -adjusted;
}

// Applies a slew rate limiter to smooth the output changes.
// Limits the change from 'previous' to 'desired' by at most 'maxDelta'.
double applySlewRate(double previous, double desired, double maxDelta) {
    double diff = desired - previous;
    if (diff > maxDelta) {
        diff = maxDelta;
    } else if (diff < -maxDelta) {
        diff = -maxDelta;
    }
    return previous + diff;
}

/*
   updateSharedState:
   Updates head_shared by low-pass filtering raw axis values,
   normalizing them, and then applying scaling (dead zone + gradual ramp-up).
   Also applies a slew rate limiter (maxDelta is 0.1 for the first 1 second, then 0.001).
*/
void updateSharedState(const JoystickState &localState, double alpha, double deadZoneThreshold) {
    // Use static variable to record the initial time.
    static auto initTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - initTime).count() / 1000.0;
    
    // Set maxDelta: 0.1 for first 1 second, then 0.001.
    double maxDelta = (elapsed < 1.0) ? 0.1 : 0.001;
    
    // Local filter state for each axis (persist between calls).
    // We can use a static local array for this purpose.
    static double filteredRaw[MAX_AXES] = {0};
    
    for (int i = 0; i < MAX_AXES; i++) {
        // Update the low-pass filtered raw value for this axis.
        filteredRaw[i] = lowpassFilter_Joy(filteredRaw[i], localState.axes[i], alpha);
        
        // Normalize the filtered raw value.
        double normalized;
        if (filteredRaw[i] < 0)
            normalized = filteredRaw[i] / 32768.0;
        else
            normalized = filteredRaw[i] / 32767.0;
        
        // Apply scaling function: dead zone + gradual ramp-up.
        double scaled = scaleJoystickOutput(normalized, deadZoneThreshold);
        // Limit the rate of change for smoother transitions.
        double finalOutput = applySlewRate(head_shared.axes[i], scaled, maxDelta);
        head_shared.axes[i] = finalOutput;
    }
    for (int i = 0; i < MAX_BUTTONS; i++) {
        head_shared.buttons[i] = localState.buttons[i];
    }
}

/*
   readJoystickEvents() reads raw joystick events and processes them:
   - Raw event data is stored in a local JoystickState structure.
   - For each event, if it's an axis event, its raw value is stored.
   - For button events, in addition to storing the current button state,
     accumulative variables are updated:
       - For button index 4 (L1): if pressed, subtract 1.
       - For button index 5 (R1): if pressed, add 1.
       - For button index 6 (L2): if pressed, subtract 1.
       - For button index 7 (R2): if pressed, add 1.
   - Finally, updateSharedState() is called to process axis data.
*/
void readJoystickEvents() {
    const char* devicePath = "/dev/input/js0";  // Actual device path (check if it's js0, js1, etc.)
    int fd = open(devicePath, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Unable to open joystick device: " << devicePath << std::endl;
        return;
    }
    
    std::cout << "Joystick device " << devicePath << " connected successfully" << std::endl;
    
    struct js_event event;
    
    // Low-pass filter coefficient and dead zone threshold
    double alpha = 0.1;              // Filter coefficient (0.0 ~ 1.0)
    double deadZoneThreshold = 0.1;  // Dead zone threshold (user-defined)
    
    // localState: holds raw axis and button data (as double for axes)
    JoystickState localState = {0};
    
    // 원하는 루프 주기 (마이크로초 단위)
    const long DESIRED_LOOP_US = 1000; // 1ms

    while (true) {
        // 루프 시작 시각 기록  
        auto loop_start = std::chrono::steady_clock::now();

        ssize_t bytes = read(fd, &event, sizeof(event));
        if (bytes == sizeof(event)) {
            unsigned char type = event.type & ~JS_EVENT_INIT;
            if (type == JS_EVENT_AXIS) {
                int axis_index = event.number;
                if (axis_index < MAX_AXES) {
                    // Store the raw value (as double) from the event.
                    localState.axes[axis_index] = static_cast<double>(event.value);
#ifdef Data_print
                    std::cout << "Axis " << axis_index 
                              << " raw: " << event.value << std::endl;
#endif
                }
            } else if (type == JS_EVENT_BUTTON) {
                int button_index = event.number;
                if (button_index < MAX_BUTTONS) {
                    localState.buttons[button_index] = event.value;
#ifdef Data_print
                    std::cout << "Button " << button_index 
                              << " state: " << event.value << std::endl;
#endif
                }
            }
        }
        
        // Update local accumulators based on button states.
        // Button indices:
        // L1 = 4: pressing decreases local_lr1
        // R1 = 5: pressing increases local_lr1
        // L2 = 6: pressing decreases local_lr2
        // R2 = 7: pressing increases local_lr2
        // Local accumulators for button counts (persist between loop iterations)
        static double local_lr1 = 0.0;  // For L1 (button index 4) and R1 (button index 5)
        static double local_lr2 = 0.0;  // For L2 (button index 6) and R2 (button index 7)

        double dt = 0.001;  // 1ms loop interval; adjust as needed.
        if (localState.buttons[4] == 1) {  // L1 pressed
            local_lr1 -= dt;
        }
        if (localState.buttons[5] == 1) {  // R1 pressed
            local_lr1 += dt;
        }
        if (localState.buttons[6] == 1) {  // L2 pressed
            local_lr2 -= dt;
        }
        if (localState.buttons[7] == 1) {  // R2 pressed
            local_lr2 += dt;
        }
        
        // Clamp local accumulators between -1.0 and 1.0.
        if (local_lr1 < -1.0) local_lr1 = -1.0;
        if (local_lr1 > 1.0)  local_lr1 = 1.0;
        if (local_lr2 < -1.0) local_lr2 = -1.0;
        if (local_lr2 > 1.0)  local_lr2 = 1.0;
        
        // Update global shared accumulative variables with the local accumulators.
        lr1_accumulated = local_lr1;
        lr2_accumulated = local_lr2;
        
        // Process the raw axis data: apply filtering, normalization, scaling, and slew rate limiting.
        updateSharedState(localState, alpha, deadZoneThreshold);
        
        // 루프 종료 시각 기록
        auto loop_end = std::chrono::steady_clock::now();
        // 실제 걸린 시간(마이크로초)
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(loop_end - loop_start).count();
        
        // 남은 시간만큼 sleep
        long remaining = DESIRED_LOOP_US - elapsed_us;
        if (remaining > 0) {
            usleep(remaining);
        }
    }
    
    close(fd);
}