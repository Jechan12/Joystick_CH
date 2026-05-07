#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "../joystick.h"

// Test main
int main() {
    // Start joystick event thread
    // std::thread joystickThread(readJoystickEvents);
    bool continueJoystickThread = true;


    // 3) 시작 시각 기록
    auto t0 = std::chrono::steady_clock::now();

    // std::ref를 사용하여 참조로 전달합니다.
    //  - readJoystickEvents가 bool&으로 플래그를 받기 때문에, 
    //    std::ref 없이 값을 전달하면 함수 내부에 복사본이 들어갑니다.
    //    // 1) 백그라운드 스레드에서 조이스틱을 처리하는 함수(runJoystickThread)를 실행합니다.
    //  - runJoystickThread가 bool&으로 플래그를 받기 때문에, 
    //    메인 스레드에서 continueJoystickThread를 false로 바꾸면 스레드가 종료됩니다.
    //  - ref(continueJoystickThread)를 사용하여 참조로 넘깁니다.
    std::thread joystickThread(joy::runJoystickThread, std::ref(continueJoystickThread));  
    // Main thread periodically prints the shared state (head_shared) and accumulative button values
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - t0).count();

        joy::JoystickState state = joy::getJoystickState();

        std::cout << std::setprecision(4);
        std::cout << "----- Shared Joystick State -----" << std::endl;
        
        std::cout << "Axes: ";
        for (int i = 0; i < joy::MAX_AXES; i++) {
            std::cout << state.axes[i] << " ";
        }
        std::cout << std::endl;

        std::cout << "Buttons: ";
        for (int i = 0; i < joy::MAX_BUTTONS; i++) {
            std::cout << state.buttons[i] << " ";
        }
        std::cout << std::endl;

        // Print accumulative button values for L1/R1 and L2/R2.
        std::cout << "L1/R1 Accumulated: " << state.lr1_accumulated << std::endl;
        std::cout << "L2/R2 Accumulated: " << state.lr2_accumulated << std::endl;
        std::cout << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // (In practice, join is never reached due to infinite loop.)
    joystickThread.join();
    return 0;
}
