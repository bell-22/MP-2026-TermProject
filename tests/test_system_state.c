#include <stdio.h>
#include "system_state.h"

static unsigned s_pass = 0U;
static unsigned s_fail = 0U;

/* 현재 상태와 예상 상태 비교 */
static void check(const char *name, SystemState expected)
{
    SystemState actual = system_state_get();

    if (actual == expected) {
        printf("[PASS] %s\n", name);
        s_pass++;
    } else {
        printf("[FAIL] %s expected=%d actual=%d\n",
            name, (int)expected, (int)actual);
        s_fail++;
    }
}

int main(void)
{
    printf("=== System State Unit Test ===\n");

    /* 1. 초기 상태 확인 */
    system_state_init();
    check("initial state", STATE_MONITORING);

    /* 2. 잘못된 상태 전환 방지 */
    system_state_set(STATE_SAVED);
    check("MONITORING -> SAVED rejected",
        STATE_MONITORING);

    /* 3. 동일 상태 유지 */
    system_state_set(STATE_MONITORING);
    check("same MONITORING", STATE_MONITORING);

    /* 4. 이벤트 기록 시작 */
    system_state_set(STATE_RECORDING);
    check("MONITORING -> RECORDING",
        STATE_RECORDING);

    /* 5. 동일 상태 유지 */
    system_state_set(STATE_RECORDING);
    check("same RECORDING", STATE_RECORDING);

    /* 6. 기록 완료 */
    system_state_set(STATE_SAVED);
    check("RECORDING -> SAVED", STATE_SAVED);

    /* 7. 잘못된 상태 전환 방지 */
    system_state_set(STATE_RECORDING);
    check("SAVED -> RECORDING rejected",
        STATE_SAVED);

    /* 8. 잘못된 상태값 입력 */
    system_state_set((SystemState)99);
    check("unknown state rejected", STATE_SAVED);

    /* 9. KEY1 초기화 */
    system_state_set(STATE_MONITORING);
    check("SAVED -> MONITORING",
        STATE_MONITORING);

    /* 10. 다시 기록 시작 */
    system_state_set(STATE_RECORDING);
    check("MONITORING -> RECORDING again",
        STATE_RECORDING);

    /* 11. 기록 중 KEY1 초기화 */
    system_state_set(STATE_MONITORING);
    check("RECORDING -> MONITORING",
        STATE_MONITORING);

    /* 12. 전체 시스템 재초기화 */
    system_state_set(STATE_RECORDING);
    system_state_set(STATE_SAVED);

    system_state_init();
    check("reinitialize", STATE_MONITORING);

    printf("\nTest summary: %u PASS, %u FAIL\n",
        s_pass, s_fail);

    return (s_fail == 0U) ? 0 : 1;
}