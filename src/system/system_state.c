#include "../../include/system_state.h"

/* 현재 시스템 상태를 저장하는 변수 */
static SystemState s_state = STATE_MONITORING;

/* 시스템 상태 초기화 */
void system_state_init(void)
{
    s_state = STATE_MONITORING;
}

/* 현재 시스템 상태 반환 */
SystemState system_state_get(void)
{
    return s_state;
}

/* 시스템 상태 변경 */
void system_state_set(SystemState next)
{
    /* 현재 상태와 같으면 변경하지 않음 */
    if (next == s_state) {
        return;
    }

    switch (s_state) {

        case STATE_MONITORING:
            /* 이벤트가 발생하여 기록 시작 */
            if (next == STATE_RECORDING) {
                s_state = next;
            }
            break;

        case STATE_RECORDING:
            /* 기록 완료 또는 KEY1 초기화 */
            if (next == STATE_SAVED ||
                next == STATE_MONITORING) {
                s_state = next;
            }
            break;

        case STATE_SAVED:
            /* KEY1으로 기록 초기화 후 감시 복귀 */
            if (next == STATE_MONITORING) {
                s_state = next;
            }
            break;

        default:
            /* 잘못된 상태는 무시 */
            break;
    }
}