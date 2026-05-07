#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <linux/joystick.h>  // js_event 등 조이스틱 타입 정의
#include <atomic>
#include <mutex>



// =========================================================================================
// ──  User Configuration Area (사용자 설정 영역)  ───────────────────────────────────────────
// [설명] 필요에 따라 아래 값들을 자유롭게 변경하세요. (컴파일 시 적용됩니다)
// 플레이스테이션 패드 기준 
// =========================================================================================

// 디버깅 출력을 켜려면 아래 주석을 해제하세요.
// #define CONFIG_DATA_PRINT

// 1. 조이스틱 장치 경로 (실제 연결된 장치가 js0, js1 인지 확인)
#define CONFIG_JOYSTICK_DEVICE       "/dev/input/js0"

// 2. 조이스틱 읽기 루프 주파수 (Hz 단위, 1000 = 1ms 주기)
#define CONFIG_JOYSTICK_HZ           100

// 3. 초기화 조건 (안전 장치)
// 프로그램 시작 후 일정 시간(초)이 지나야 하며, 특정 시작 버튼을 눌러야 제어 입력이 들어갑니다.
#define CONFIG_INIT_DELAY_SEC        3.0f  // 대기 시간 (초)
#define CONFIG_BUTTON_START          11    // 시작 트리거 버튼 인덱스
#define CONFIG_BUTTON_KILL           8     // 비상 정지(Kill Switch) 버튼 (PS 패드의 SHARE)

// 4. 필터 및 조작감 설정
#define CONFIG_FILTER_TAU            0.66f   // Low-pass 필터 시정수(초). 값이 클수록 묵직하고 느리게 반응.
#define CONFIG_DEFAULT_DEADZONE      0.1f    // 데드존 (이하의 미세한 스틱 움직임 무시)

// 5. 버튼 누적기 (가상 축) 속도 조절
// L1/R1, L2/R2 버튼을 누르고 있을 때 초당 얼마나 증감할지 결정 (1.0 = 초당 1.0 누적)
#define CONFIG_ACCUM_RATE            1.0f

// 6. Slew-rate 제한 기능 (스틱의 급격한 조작 방지)
// 활성화하려면 아래 주석을 해제하세요.
// #define CONFIG_USE_SLEW
#define CONFIG_SLEW_INITIAL_MAX_RATE   100.0f  // 처음 스위치 타임 동안의 초당 최대 변화량 (100.0 = 0.01초만에 0->1 도달)
#define CONFIG_SLEW_RUNNING_MAX_RATE   1.0f    // 이후 안정화 상태에서의 초당 최대 변화량 (1.0 = 1초만에 0->1 도달)
#define CONFIG_SLEW_SWITCH_TIME_S      1.0f    // 스위치 타임 (초)

// 7. 시스템 버튼 인덱스 매핑 (패드 종류에 따라 다를 수 있음)
#define CONFIG_BUTTON_L1             4
#define CONFIG_BUTTON_R1             5
#define CONFIG_BUTTON_L2             6
#define CONFIG_BUTTON_R2             7

// =========================================================================================

namespace joy { 

extern std::atomic<bool> inputEnabled;    // true여야만 runJoystickThread가 조작을 반영합니다.

// Raw axis value max (abs). negative 방향과 positive 방향이 약간 다르므로 분리.
constexpr float RAW_AXIS_MAX_NEG        = 32767.0f;  // 음수 측 최대 절대값
constexpr float RAW_AXIS_MAX_POS        = 32767.0f;  // 양수 측 최대 절대값

// 최대 축/버튼 개수
constexpr int MAX_AXES =  8;
constexpr int MAX_BUTTONS =  13;


// Shared state variable: Data to be read by the controller thread
struct JoystickState {
    float axes[MAX_AXES];   // Normalized values after applying low-pass filter
    int buttons[MAX_BUTTONS]; // Button states (0 or 1)
    float lr1_accumulated;  // 누적기 1 (L1/R1)
    float lr2_accumulated;  // 누적기 2 (L2/R2)
};

// 스레드 안전하게 최신 조이스틱 상태를 가져오는 함수 (외부에서 호출)
JoystickState getJoystickState();


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
 *    최종 축 값을 내부 상태에 안전하게 갱신
 * 5. 루프 주파수(CONFIG_JOYSTICK_HZ)를 맞춰 usleep으로 대기
 * 6. 외부에서 continueJoystickThread를 false로 설정하면 루프를 빠져나가고 디바이스를 close
 *
 * @param continueJoystickThread  true인 동안 루프 실행, false로 변경 시 루프 종료
 */
void runJoystickThread(bool &continueJoystickThread);

}  // namespace joy
#endif // JOYSTICK_H
