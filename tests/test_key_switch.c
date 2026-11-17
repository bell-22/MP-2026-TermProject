#include <stdio.h>
#include <stdint.h>
#include "key_switch.h"

static unsigned s_pass = 0U;
static unsigned s_fail = 0U;


/* 반환된 KEY 요청이 예상값과 같은지 검사 */
static void check(const char *name, uint8_t expected)
{
    const uint8_t actual = key_take_events();

    if (actual == expected) {
        printf("[PASS] %-30s expected=%u actual=%u\n",
            name,
            (unsigned)expected,
            (unsigned)actual);
        s_pass++;
    } else {
        printf("[FAIL] %-30s expected=%u actual=%u\n",
            name,
            (unsigned)expected,
            (unsigned)actual);
        s_fail++;
    }
}


int main(void)
{
    printf("=== KEY Switch Unit Test ===\n");

    /* 1. 초기화 테스트 */
    key_init();
    check("initial state", KEY_EVENT_NONE);

    /* 2. 입력이 없는 경우 */
    key_irq_notify(0U, 0U);
    check("empty input", KEY_EVENT_NONE);

    /* 3. KEY0 최초 입력 */
    key_irq_notify(KEY_EVENT_STOP, 0U);
    check("KEY0 first press", KEY_EVENT_STOP);

    /* 4. 이벤트 소비 확인 */
    check("event consumed", KEY_EVENT_NONE);

    /* 5. KEY0 디바운싱 테스트 */
    key_irq_notify(KEY_EVENT_STOP, 10U);
    check("KEY0 bounce blocked", KEY_EVENT_NONE);

    /* 6. KEY1 독립 입력 테스트 */
    key_irq_notify(KEY_EVENT_CLEAR, 10U);
    check("KEY1 independent", KEY_EVENT_CLEAR);

    /* 7. KEY1 디바운싱 테스트 */
    key_irq_notify(KEY_EVENT_CLEAR, 20U);
    check("KEY1 bounce blocked", KEY_EVENT_NONE);

    /* 8. KEY0 50ms 경과 */
    key_irq_notify(KEY_EVENT_STOP, 50U);
    check("KEY0 at 50ms boundary", KEY_EVENT_STOP);

    /* 9. KEY1 50ms 경과 */
    key_irq_notify(KEY_EVENT_CLEAR, 60U);
    check("KEY1 at 50ms boundary", KEY_EVENT_CLEAR);

    /* 10. KEY0과 KEY1 동시 입력 */
    key_irq_notify(
        KEY_EVENT_STOP | KEY_EVENT_CLEAR,
        120U
    );

    check("simultaneous KEY0+KEY1",
        KEY_EVENT_STOP | KEY_EVENT_CLEAR);

    /* 11. 동시 이벤트 소비 */
    check("both consumed", KEY_EVENT_NONE);

    /* 12. 정의되지 않은 KEY 비트 무시 */
    key_irq_notify(0x80U, 200U);
    check("unknown bit ignored", KEY_EVENT_NONE);

    /* 13. 잘못된 비트와 KEY0 동시 입력 */
    key_irq_notify(
        (uint8_t)(0x80U | KEY_EVENT_STOP),
        200U
    );
    check("unknown bit + KEY0", KEY_EVENT_STOP);

    /* 14. 여러 요청 누적 */
    key_irq_notify(KEY_EVENT_STOP, 260U);
    key_irq_notify(KEY_EVENT_CLEAR, 260U);

    check("requests accumulated",
        KEY_EVENT_STOP | KEY_EVENT_CLEAR);

    /* 15. 재초기화 */
    key_init();
    check("reinitialization", KEY_EVENT_NONE);

    /* 16. 타이머 오버플로 직전 입력 */
    key_irq_notify(KEY_EVENT_STOP, UINT32_MAX - 20U);
    check("wraparound first press", KEY_EVENT_STOP);

    /* 17. 오버플로 후 31ms 경과 */
    key_irq_notify(KEY_EVENT_STOP, 10U);
    check("wraparound 31ms blocked", KEY_EVENT_NONE);

    /* 18. 오버플로 후 56ms 경과 */
    key_irq_notify(KEY_EVENT_STOP, 35U);
    check("wraparound 56ms allowed", KEY_EVENT_STOP);

    /* 테스트 결과 */
    printf("\nTest summary: %u PASS, %u FAIL\n",
        s_pass, s_fail);

    return (s_fail == 0U) ? 0 : 1;
}