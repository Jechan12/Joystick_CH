#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "joystick.h"

// Test main
int main() {
    // Start joystick event thread
    // std::thread joystickThread(readJoystickEvents);
    bool continueJoystickThread = true;


    // 3) 시작 시각 기록
    auto t0 = std::chrono::steady_clock::now();
    bool initDone = false;

    // std::ref를 사용하여 참조로 전달합니다.
    //  - readJoystickEvents가 bool&으로 플래그를 받기 때문에, 
    //    std::ref 없이 값을 전달하면 함수 내부에 복사본이 들어갑니다.
    //  - 복사본으로는 main 스레드에서 continueJoystickThread를 false로 바꿔도
    //    스레드 내부에 반영되지 않아 루프가 끝나지 않습니다.
    //  - std::ref로 전달하면 reference_wrapper를 통해 원본 변수를 참조하게 되어,
    //    main에서 값을 변경하면 즉시 스레드 내부에도 반영됩니다.
    std::thread joystickThread(joy::readJoystickEvents, std::ref(continueJoystickThread));  
    // Main thread periodically prints the shared state (head_shared) and accumulative button values
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - t0).count();

        std::cout << std::setprecision(4);
        std::cout << "----- Shared Joystick State -----" << std::endl;
        
        std::cout << "Axes: ";
        for (int i = 0; i < joy::MAX_AXES; i++) {
            std::cout << joy::head_shared.axes[i] << " ";
        }
        std::cout << std::endl;

        std::cout << "Buttons: ";
        for (int i = 0; i < joy::MAX_BUTTONS; i++) {
            std::cout <<joy:: head_shared.buttons[i] << " ";
        }
        std::cout << std::endl;

        // Print accumulative button values for L1/R1 and L2/R2.
        std::cout << "L1/R1 Accumulated: " << joy::lr1_accumulated << std::endl;
        std::cout << "L2/R2 Accumulated: " << joy::lr2_accumulated << std::endl;
        std::cout << std::endl;

        // Wait for 10 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // (In practice, join is never reached due to infinite loop.)
    joystickThread.join();
    return 0;
}
