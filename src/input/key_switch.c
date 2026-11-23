
#include "../../include/key_switch.h"
#include "../../include/system_config.h"

#include <stdbool.h>
#include <stdint.h>

/* Nios II 보드에서만 제어 레지스터 사용 */
#if defined(__nios2_arch__)
#include "../../include/nios2_ctrl_reg_macros.h"
#endif

#define KEY_COUNT 4U

/* ISR에서 저장하고 메인 루프에서 처리하는 KEY 요청 */
static volatile uint8_t s_pending_events;

/* KEY0~KEY3 마지막 입력 시간 */
static uint32_t s_last_key_ms[KEY_COUNT];

/* 각 KEY의 최초 입력 여부 */
static bool s_seen_key[KEY_COUNT];


/* KEY 입력 상태 초기화 */
void key_init(void)
{
    uint32_t i;

    /* 인터럽트 활성화 전에 호출 */
    s_pending_events = KEY_EVENT_NONE;

    for (i = 0U; i < KEY_COUNT; i++)
    {
        s_last_key_ms[i] = 0U;
        s_seen_key[i] = false;
    }
}


/* KEY 인터럽트에서 눌림 이벤트 전달 */
void key_irq_notify(uint8_t pressed_mask,
                    uint32_t timestamp_ms)
{
    uint32_t i;

    const uint8_t valid_mask =
        (uint8_t)(
            KEY_EVENT_STOP |
            KEY_EVENT_CLEAR |
            KEY_EVENT_VIEW_LIVE |
            KEY_EVENT_VIEW_HISTORY
        );

    const uint8_t pressed =
        (uint8_t)(pressed_mask & valid_mask);

    /* KEY0~KEY3 각각 확인 */
    for (i = 0U; i < KEY_COUNT; i++)
    {
        const uint8_t key_bit =
            (uint8_t)(1U << i);

        /* 해당 KEY가 눌리지 않았으면 건너뜀 */
        if ((pressed & key_bit) == 0U)
        {
            continue;
        }

        /*
         * 디바운싱
         *
         * 최초 입력이거나
         * 마지막 입력 후 50ms 이상이면 인정
         */
        if (!s_seen_key[i] ||
            (uint32_t)(
                timestamp_ms - s_last_key_ms[i]
            ) >= KEY_DEBOUNCE_MS)
        {
            /* 메인 루프에 이벤트 전달 */
            s_pending_events =
                (uint8_t)(
                    s_pending_events | key_bit
                );

            s_last_key_ms[i] = timestamp_ms;
            s_seen_key[i] = true;
        }
    }
}


/* 메인 루프에서 KEY 입력 요청 확인 */
uint8_t key_take_events(void)
{
    uint8_t events;

#if defined(__nios2_arch__)
    uint32_t saved_status;

    /* 현재 CPU 인터럽트 상태 저장 */
    NIOS2_READ_STATUS(saved_status);

    /* 이벤트 조회 중 인터럽트 잠시 차단 */
    NIOS2_WRITE_STATUS(saved_status & ~1U);
#endif

    /* 누적된 KEY0~KEY3 입력 가져오기 */
    events = s_pending_events;

    /* 처리할 이벤트를 가져온 후 초기화 */
    s_pending_events = KEY_EVENT_NONE;

#if defined(__nios2_arch__)
    /* 이전 CPU 인터럽트 상태 복원 */
    NIOS2_WRITE_STATUS(saved_status);
#endif

    return events;
}
