
#include "../../include/key_switch.h"
#include "../../include/system_config.h"
#include <stdbool.h>

/* Nios II 보드에서만 제어 레지스터 사용 */
#if defined(__nios2_arch__)
#include "../../include/nios2_ctrl_reg_macros.h"
#endif


/* ISR에서 저장하고 메인 루프에서 처리하는 KEY 요청 */
static volatile uint8_t s_pending_events;

/* KEY0, KEY1 마지막 입력 시간 */
static uint32_t s_last_key0_ms;
static uint32_t s_last_key1_ms;

/* 각 KEY의 최초 입력 여부 */
static bool s_seen_key0;
static bool s_seen_key1;


/* KEY 입력 상태 초기화 */
void key_init(void)
{
    /* 인터럽트 활성화 전에 호출해야 한다. */
    s_pending_events = KEY_EVENT_NONE;

    s_last_key0_ms = 0U;
    s_last_key1_ms = 0U;

    s_seen_key0 = false;
    s_seen_key1 = false;
}


/* KEY 인터럽트에서 눌림 이벤트 전달 */
void key_irq_notify(uint8_t pressed_mask,
                    uint32_t timestamp_ms)
{
    const uint8_t valid_mask =
        (uint8_t)(KEY_EVENT_STOP | KEY_EVENT_CLEAR);

    /* 정의된 KEY0, KEY1 비트만 사용 */
    const uint8_t pressed =
        (uint8_t)(pressed_mask & valid_mask);

    /* 1. KEY0: 경고음 해제 요청 */
    if ((pressed & KEY_EVENT_STOP) != 0U) {

        /* 최초 입력 또는 50ms 경과 시 인정 */
        if (!s_seen_key0 ||
            (uint32_t)(timestamp_ms - s_last_key0_ms)
                >= KEY_DEBOUNCE_MS) {

            s_pending_events |= KEY_EVENT_STOP;

            s_last_key0_ms = timestamp_ms;
            s_seen_key0 = true;
        }
    }

    /* 2. KEY1: 이벤트 기록 초기화 요청 */
    if ((pressed & KEY_EVENT_CLEAR) != 0U) {

        if (!s_seen_key1 ||
            (uint32_t)(timestamp_ms - s_last_key1_ms)
                >= KEY_DEBOUNCE_MS) {

            s_pending_events |= KEY_EVENT_CLEAR;

            s_last_key1_ms = timestamp_ms;
            s_seen_key1 = true;
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

    /* 이벤트를 읽는 동안 인터럽트 잠시 차단 */
    NIOS2_WRITE_STATUS(saved_status & ~1u);
#endif

    /* 누적된 KEY0, KEY1 입력 가져오기 */
    events = s_pending_events;

    /* 가져온 이벤트만 초기화 */
    s_pending_events = KEY_EVENT_NONE;

#if defined(__nios2_arch__)
    /* 이전 CPU 인터럽트 상태 복원 */
    NIOS2_WRITE_STATUS(saved_status);
#endif

    return events;
}
