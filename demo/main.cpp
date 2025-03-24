#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "joystick.h"

// Test main
int main() {
    // Start joystick event thread
    std::thread joystickThread(readJoystickEvents);

    // Main thread periodically prints the shared state (head_shared) and accumulative button values
    while (true) {
        std::cout << std::setprecision(4);
        std::cout << "----- Shared Joystick State -----" << std::endl;
        
        std::cout << "Axes: ";
        for (int i = 0; i < MAX_AXES; i++) {
            std::cout << head_shared.axes[i] << " ";
        }
        std::cout << std::endl;

        std::cout << "Buttons: ";
        for (int i = 0; i < MAX_BUTTONS; i++) {
            std::cout << head_shared.buttons[i] << " ";
        }
        std::cout << std::endl;

        // Print accumulative button values for L1/R1 and L2/R2.
        std::cout << "L1/R1 Accumulated: " << lr1_accumulated << std::endl;
        std::cout << "L2/R2 Accumulated: " << lr2_accumulated << std::endl;
        std::cout << std::endl;

        // Wait for 10 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // (In practice, join is never reached due to infinite loop.)
    joystickThread.join();
    return 0;
}
