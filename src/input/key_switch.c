#include "key_switch.h"
#include "system_config.h"
#include <stdbool.h>

/*
 * Nios II에서 사용할 경우 실제 BSP 확인 후
 * KEY_SWITCH_NIOS_HAL 매크로를 활성화한다.
 */
#if defined(KEY_SWITCH_NIOS_HAL)
#include <sys/alt_irq.h>
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

#if defined(KEY_SWITCH_NIOS_HAL)
    /*
     * 실제 Nios II 환경에서는 읽기/초기화 도중
     * IRQ가 발생해 요청이 유실되지 않도록 보호한다.
     */
    alt_irq_context context = alt_irq_disable_all();
#endif

    /* 현재 누적된 KEY 요청 가져오기 */
    events = s_pending_events;

    /* 처리한 요청 제거 */
    s_pending_events = KEY_EVENT_NONE;

#if defined(KEY_SWITCH_NIOS_HAL)
    alt_irq_enable_all(context);
#endif

    return events;
}