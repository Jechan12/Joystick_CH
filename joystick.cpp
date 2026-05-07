#include "joystick.h"

#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>   // for std::fabs
#include <chrono>  // for time measurement

namespace joy {

// 초기값은 false (입력 무시)
std::atomic<bool> inputEnabled{false};

// 내부 스레드에서 관리되는 전역 상태 변수 (뮤텍스로 보호됨)
static JoystickState head_shared = {0};
std::mutex joystick_mutex;

JoystickState getJoystickState() {
    std::lock_guard<std::mutex> lock(joystick_mutex);
    return head_shared;
}

// Low-pass filter function (exponential moving average)
float lowpassFilter_Joy(float previous, float current, float alpha) {
    return previous + alpha * (current - previous);
}

/**
 * @brief scaleJoystickOutput
 *
 * 1) Dead zone 처리:
 *    절대값(absVal)이 deadZoneThreshold 이하면 0으로 간주해
 *    손떨림이나 미세한 노이즈로 인한 불필요한 입력을 무시합니다.
 *
 * 2) Ramp-up 스케일링:
 *    [deadZoneThreshold, 1] 구간을 [0,1]로 선형 매핑한 뒤
 *    제곱(curve = x^2)을 적용해 저속 영역에서 더 부드러운 출력을 만듭니다.
 *
 * @param normalized        -1.0 ~ 1.0으로 정규화된 입력 값
 * @param deadZoneThreshold 데드존 임계치 (0.0 ~ 1.0)
 * @return 스케일링 후 출력 값 (-1.0 ~ 1.0)
 */
float scaleJoystickOutput(float normalized, float deadZoneThreshold) {
    float absVal = std::fabs(normalized);
    if (absVal < deadZoneThreshold) {
        return 0.0f;
    }
    // Compute adjusted value: linearly map [deadZoneThreshold, 1] to [0, 1]
    float adjusted = (absVal - deadZoneThreshold) / (1.0f - deadZoneThreshold);
    // Apply a quadratic scaling for a smoother, gradual ramp-up.
    adjusted = std::pow(adjusted, 2.0f); 
    return (normalized >= 0) ? adjusted : -adjusted;
}

// Applies a slew rate limiter to smooth the output changes.
// Limits the change from 'previous' to 'desired' by at most 'maxDelta'.
/**
 * @brief applySlewRate (슬루율 리미터)
 *
 * 출력의 순간 변화량(diff)을 maxDelta로 제한해
 *
 * @param previous  직전 출력 값
 * @param desired   목표 출력 값
 * @param maxDelta  한 스텝당 허용 최대 변화량
 * @return maxDelta 범위 내에서 부드럽게 보정된 출력 값
 */
float applySlewRate(float previous, float desired, float maxDelta) {
    float diff = desired - previous;
    if (diff > maxDelta) {
        diff = maxDelta;
    } else if (diff < -maxDelta) {
        diff = -maxDelta;
    }
    return previous + diff;
}



// raw ∈ [−32767 … +32767] 를 normalized ∈ [−1 … +1] 로 매핑
float normalizeAxisValue(float raw) {
    if (raw < 0.0f) {
        return raw / RAW_AXIS_MAX_NEG;
    } else {
        return raw / RAW_AXIS_MAX_POS;
    }
}

/*
   updateSharedState:
   Updates head_shared by low-pass filtering raw axis values,
   normalizing them, and then applying scaling (dead zone + gradual ramp-up).
   Also applies a slew rate limiter (maxDelta is 0.1 for the first 1 second, then 0.001).
*/
// ────────────────────────────────────────────────────────────────────────────
/**
 * @brief updateSharedState
 *
 * localState.axes[]에 들어온 raw 축 값을 아래 순서로 처리하여
 * 내부 상태(head_shared)에 저장하고, 버튼 상태는 그대로 복사합니다.
 *
 *  1) lowpassFilter_Joy로 노이즈 제거
 *  2) normalizeAxisValue로 –1~1 정규화
 *  3) scaleJoystickOutput으로 dead zone + 부드러운 ramp-up
 *  4) applySlewRate로 슬루율 리미팅
 *
 * @param localState        생(raw) 입력이 담긴 구조체
 * @param alpha             필터 계수
 * @param deadZoneThreshold dead zone 임계치
 */
void updateSharedState(const JoystickState &localState, float dt, float deadZoneThreshold) {
    
    // Time constant to alpha conversion for EMA filter
    float alpha = dt / (CONFIG_FILTER_TAU + dt);

    // Use static variable to record the initial time.
    static auto initTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - initTime).count() / 1000.0f;
    
    // Set maxDelta based on max rate per second * dt
    float maxRate = (elapsed < CONFIG_SLEW_SWITCH_TIME_S) ? CONFIG_SLEW_INITIAL_MAX_RATE : CONFIG_SLEW_RUNNING_MAX_RATE;
    float maxDelta = maxRate * dt;

    // 첫 호출일 때는 filteredRaw를 raw 값으로 채워서 
    // 0→–1 과도 현상을 방지합니다. (특히 L2 R2)
    static bool firstCall = true;
    
    // Local filter state for each axis (persist between calls).
    // We can use a static local array for this purpose.
    static float filteredRaw[joy::MAX_AXES] = {0.0f};

    if (firstCall) {
            for (int i = 0; i < MAX_AXES; ++i) {
                // raw 값 그대로 초기 세팅
                filteredRaw[i] = localState.axes[i];
                // 즉시 head_shared에 반영 (데드존+스케일링만)
                float norm   = normalizeAxisValue(filteredRaw[i]);
                float scaled = scaleJoystickOutput(norm, deadZoneThreshold);
                head_shared.axes[i] = scaled;
            }
            // 버튼도 바로 복사
            for (int i = 0; i < MAX_BUTTONS; ++i) {
                head_shared.buttons[i] = localState.buttons[i];
            }
            firstCall = false;
            return;
    }
    
    for (int i = 0; i < joy::MAX_AXES; i++) {
        // Update the low-pass filtered raw value for this axis.
        filteredRaw[i] = lowpassFilter_Joy(filteredRaw[i], localState.axes[i], alpha);
        
        // Normalize the filtered raw value.
        float normalized = normalizeAxisValue(filteredRaw[i]);
        
        // Apply scaling function: dead zone + gradual ramp-up.
        float scaled = scaleJoystickOutput(normalized, deadZoneThreshold);
#ifdef CONFIG_USE_SLEW
        // Limit the rate of change for smoother transitions.
        float finalOutput = applySlewRate(head_shared.axes[i], scaled, maxDelta);
        head_shared.axes[i] = finalOutput;
#else
        head_shared.axes[i] = scaled;
#endif


    }
    for (int i = 0; i < joy::MAX_BUTTONS; i++) {
        head_shared.buttons[i] = localState.buttons[i];
    }
}

/*
- For button events, in addition to storing the current button state,
    accumulative variables are updated:
  - For button index 4 (L1): if pressed, subtract 1.
  - For button index 5 (R1): if pressed, add 1.
  - For button index 6 (L2): if pressed, subtract 1.
  - For button index 7 (R2): if pressed, add 1.
*/
/**
 * @brief updateAccumulators
 *
 * L1/R1, L2/R2 버튼이 눌린 시간만큼 accumStep 단위로 값을 더하거나 빼
 * lr1_accumulated, lr2_accumulated에 누적합니다.
 * 누적값은 –1.0 ~ +1.0 범위로 클램핑됩니다.
 *
 * @param state      현재 버튼 상태가 담긴 구조체
 * @param accumStep  한 스텝당 누적할 양 (초 또는 임의 단위)
 */
void updateAccumulators(const JoystickState &state, float dt) {
    float accumStep = CONFIG_ACCUM_RATE * dt;

    // L1 (CONFIG_BUTTON_L1) 누르면 감소, R1 누르면 증가
    if (state.buttons[CONFIG_BUTTON_L1]) {
        head_shared.lr1_accumulated = std::clamp(head_shared.lr1_accumulated - accumStep, -1.0f, 1.0f);
    }
    if (state.buttons[CONFIG_BUTTON_R1]) {
        head_shared.lr1_accumulated = std::clamp(head_shared.lr1_accumulated + accumStep, -1.0f, 1.0f);
    }

    // L2 (CONFIG_BUTTON_L2) 누르면 감소, R2 누르면 증가
    if (state.buttons[CONFIG_BUTTON_L2]) {
        head_shared.lr2_accumulated = std::clamp(head_shared.lr2_accumulated - accumStep, -1.0f, 1.0f);
    }
    if (state.buttons[CONFIG_BUTTON_R2]) {
        head_shared.lr2_accumulated = std::clamp(head_shared.lr2_accumulated + accumStep, -1.0f, 1.0f);
    }
}


/*
   runJoystickThread() reads raw joystick events and processes them:
   - Raw event data is stored in a local JoystickState structure.
   - For each event, if it's an axis event, its raw value is stored.
   - Finally, updateSharedState() is called to process axis data.
*/
/**
 * @brief runJoystickThread
 *
 * 논블록킹으로 조이스틱 이벤트(/dev/input/js0)를 읽어
 * 1) 축/버튼 이벤트를 localState에 저장
 * 2) updateAccumulators 호출해 버튼 누적값 갱신
 * 3) updateSharedState 호출해 축 값 필터·정규화·스케일링·슬루 적용
 * 4) CONFIG_JOYSTICK_HZ 주파수로 루프
 *
 * 외부에서 continueJoystickThread를 false로 바꾸면
 * 디바이스를 close하고 함수가 종료됩니다.
 *
 * @param continueJoystickThread  루프 동작 제어 변수
 */
void runJoystickThread(bool &continueJoystickThread) {
    const char* devicePath = CONFIG_JOYSTICK_DEVICE;  
    int fd = open(devicePath, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Unable to open joystick device: " << devicePath << std::endl;
        return;
    }
    
    std::cout << "Joystick device " << devicePath << " connected successfully" << std::endl;
    
    struct js_event event;
    
    // Low-pass filter coefficient and dead zone threshold
    float deadZoneThreshold = CONFIG_DEFAULT_DEADZONE;  // Dead zone threshold (user-defined)
    
    // localState: holds raw axis and button data (as float for axes)
    JoystickState localState = {0};
    
    // 원하는 루프 주기 계산 (마이크로초 단위)
    const long DESIRED_LOOP_US = 1000000 / CONFIG_JOYSTICK_HZ; 

    auto startTime = std::chrono::steady_clock::now();
    bool initDone = false;

    auto last_time = std::chrono::steady_clock::now();

    while (continueJoystickThread) {
        // 루프 시작 시각 기록  
        auto loop_start = std::chrono::steady_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::microseconds>(loop_start - last_time).count() / 1000000.0f;
        last_time = loop_start;

        ssize_t bytes = read(fd, &event, sizeof(event));
        
        // 1. 디스커넥트 처리 (Issue 1)
        if (bytes < 0 && errno != EAGAIN) {
            std::cerr << "[CRITICAL] Joystick disconnected! Stopping robot." << std::endl;
            close(fd);
            fd = -1;

            // 상태 초기화
            localState = {0};
            {
                std::lock_guard<std::mutex> lock(joystick_mutex);
                head_shared = {0};
            }
            inputEnabled.store(false);
            initDone = false;

            // 재연결 대기
            while (continueJoystickThread && fd < 0) {
                std::cout << "Waiting for joystick reconnection..." << std::endl;
                usleep(1000000); // 1초 대기
                fd = open(devicePath, O_RDONLY | O_NONBLOCK);
            }
            if (fd >= 0) {
                std::cout << "[INFO] Joystick reconnected!" << std::endl;
                startTime = std::chrono::steady_clock::now();
                last_time = std::chrono::steady_clock::now();
            }
            continue;
        }

        if (bytes == sizeof(event)) {
            unsigned char type = event.type & ~JS_EVENT_INIT;
            if (type == JS_EVENT_AXIS) {
                int axis_index = event.number;
                if (axis_index < MAX_AXES) {
                    // Store the raw value (as float) from the event.
                    localState.axes[axis_index] = static_cast<float>(event.value);
#ifdef CONFIG_DATA_PRINT
                    std::cout << "Axis " << axis_index 
                              << " raw: " << event.value << std::endl;
#endif
                }
            } else if (type == JS_EVENT_BUTTON) {
                int button_index = event.number;
                if (button_index < MAX_BUTTONS) {
                    localState.buttons[button_index] = event.value;
#ifdef CONFIG_DATA_PRINT
                    std::cout << "Button " << button_index 
                              << " state: " << event.value << std::endl;
#endif
                }
            }
        }

        // 2. Kill Switch (비상 정지) 처리 (Issue 3)
        if (localState.buttons[CONFIG_BUTTON_KILL] == 1) {
            if (inputEnabled.load()) {
                std::cerr << "[WARNING] Kill Switch (SELECT) Pressed! Disabling inputs." << std::endl;
            }
            inputEnabled.store(false);
            initDone = false;
            localState = {0};
            {
                std::lock_guard<std::mutex> lock(joystick_mutex);
                head_shared = {0};
            }
        }
        
        // 2) initDone 전에는 START 버튼만 복사
        if (!initDone) {
            std::lock_guard<std::mutex> lock(joystick_mutex);
            head_shared.buttons[CONFIG_BUTTON_START] = localState.buttons[CONFIG_BUTTON_START];
        }

        // 3) 초기화 완료 조건: CONFIG_INIT_DELAY_SEC 경과 + START 버튼 눌림
        if (!initDone) {
            float elapsed_init = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() - startTime
                             ).count();
            
            bool start_pressed = false;
            {
                std::lock_guard<std::mutex> lock(joystick_mutex);
                start_pressed = (head_shared.buttons[CONFIG_BUTTON_START] == 1);
            }

            if (elapsed_init >= CONFIG_INIT_DELAY_SEC && start_pressed)
            {
                inputEnabled.store(true);
                initDone = true;
                std::cout << "[INFO] Joystick enabled after START pressed.\n";
            }
        }

        // **입력 허용 플래그가 true일 때만 실제 반영**  
        if (inputEnabled.load()) {
            std::lock_guard<std::mutex> lock(joystick_mutex);
            updateAccumulators(localState, dt);
            // Process the raw axis data: apply filtering, normalization, scaling, and slew rate limiting.
            updateSharedState(localState, dt, deadZoneThreshold);
        }

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

}  // namespace joy