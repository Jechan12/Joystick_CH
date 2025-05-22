#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <linux/joystick.h>  // js_event 등 조이스틱 타입 정의
#include <atomic>




namespace joy { 
// Debug real-time output control (uncomment to enable output)
// #define Data_print

extern std::atomic<bool> inputEnabled;    // true여야만 readJoystickEvents가 값을 반영합니다.

// ──  Configuration Constants  ────────────────────────────────────────────────────────────────
// #define SLEW                   // 정의하면 lowpass filter 친 이후에 SLEW 까지 적용하여 maxdelta 제한

/////////           [ Adjust as needed , 필요에 따라 조정하세요 ]        ///////////////////////////
// Joystick device path
constexpr char   JOYSTICK_DEVICE[]       = "/dev/input/js0";  // Actual device path (check if it's js0, js1, etc.)

// Loop timing (in microseconds)
constexpr unsigned   JOYSTICK_LOOP_US        = 1000;   // 1 ms

 // Low-pass filter coefficient and dead-zone threshold
constexpr float DEFAULT_ALPHA           = 0.0015f;    // 필터 계수 [0.0 ~ 1.0] 
constexpr float DEFAULT_DEADZONE        = 0.1f;    // normalized units

// Slew-rate limiting
constexpr float SLEW_INITIAL_MAX_DELTA  = 0.1f;    // first SLEW_SWITCH_TIME_S seconds
constexpr float SLEW_RUNNING_MAX_DELTA  = 0.001f;  // 이후
constexpr float SLEW_SWITCH_TIME_S      = 1.0f;    // seconds

// Button indices for accumulators
constexpr int    BUTTON_L1               = 4;
constexpr int    BUTTON_R1               = 5;
constexpr int    BUTTON_L2               = 6;
constexpr int    BUTTON_R2               = 7;

constexpr double accumStep = 0.001; // L1&R1 , L2&R2 누적 값 . 속도조절 

// Raw axis value max (abs). negative 방향과 positive 방향이 약간 다르므로 분리.
constexpr float RAW_AXIS_MAX_NEG        = 32767.0f;  // 음수 측 최대 절대값
constexpr float RAW_AXIS_MAX_POS        = 32767.0f;  // 양수 측 최대 절대값

// 최대 축/버튼 개수
constexpr int MAX_AXES =  8;
constexpr int MAX_BUTTONS =  13;

// 초기화 완료까지 대기할 시간 (초)
constexpr float INIT_DELAY_SEC = 3.0f;
// 시간과 함께 값 받아오기 시작할 버튼
constexpr int BUTTON_START = 11;
// ─────────────────────────────────────────────────────────────────────────────────────────


// Shared state variable: Data to be read by the controller thread
struct JoystickState {
    float axes[MAX_AXES];   // Normalized values after applying low-pass filter
    int buttons[MAX_BUTTONS]; // Button states (0 or 1)
};

// Global shared state variable for axes/buttons
// 전역 공유 상태 변수: low-pass 필터 → 정규화 → 스케일링된 축 값과 버튼 상태를 보관합니다.
extern JoystickState head_shared;

// Shared accumulative variables for button counts:
// For L1 (button index 4) and R1 (button index 5): pressing L1 decrements, R1 increments.
// 누적 버튼 카운터 - lr1_accumulated: L1(버튼 4)을 누르면 값이 감소하고, R1(버튼 5)을 누르면 값이 증가합니다.
extern float lr1_accumulated;

// For L2 (button index 6) and R2 (button index 7): pressing L2 decrements, R2 increments.
// 누적 버튼 카운터 - lr2_accumulated: L2(버튼 6)을 누르면 값이 감소하고, R2(버튼 7)을 누르면 값이 증가합니다
extern float lr2_accumulated;


/**
 * @brief 조이스틱 이벤트를 지속적으로 읽고 처리하는 함수
 *
 * 이 함수는 별도의 스레드에서 실행되며, 아래 과정을 반복합니다:
 * 1. JOYSTICK_DEVICE 경로의 조이스틱 디바이스를 논블록킹 모드로 open
 * 2. js_event 구조체로부터 축(axis) 및 버튼 이벤트를 읽어 localState에 저장
 *    - 축 이벤트: raw 값을 localState.axes[index]에 대입
 *    - 버튼 이벤트: state 값을 localState.buttons[index]에 대입
 * 3. BUTTON_L1/R1, BUTTON_L2/R2 버튼 상태에 따라 lr1_accumulated, lr2_accumulated를
 *    ACCUM_STEP 만큼 감소/증가시켜 누적값을 갱신
 * 4. updateSharedState() 호출을 통해
 *    low-pass 필터 → 정규화 → 데드존+스케일링 → 슬루율 제한 순으로
 *    최종 축 값을 head_shared.axes에 저장, 버튼 상태는 head_shared.buttons에 복사
 * 5. 루프 주기(JOYSTICK_LOOP_US 마이크로초)를 맞춰 usleep으로 대기
 * 6. 외부에서 continueJoystickThread를 false로 설정하면 루프를 빠져나가고 디바이스를 close
 *
 * @param continueJoystickThread  true인 동안 루프 실행, false로 변경 시 루프 종료
 */
void readJoystickEvents(bool &continueJoystickThread);

}  // namespace joy
#endif // JOYSTICK_H
