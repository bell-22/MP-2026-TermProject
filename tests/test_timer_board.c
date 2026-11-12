
#include <stdint.h>
#include <stdbool.h>

#include "timer.h"
#include "interrupt.h"

#define LEDR_BASE 0xFF200000u

#define LEDR \
    (*(volatile uint32_t *)(uintptr_t)LEDR_BASE)

int main(void)
{
    uint32_t last_blink_ms = 0;
    uint32_t sample_count = 0;
    uint32_t led_value = 0;

    /* 초기 상태 */
    LEDR = 0;

    /* IRQ 비활성화 및 설정 초기화 */
    if (!interrupt_init())
    {
        while (1)
        {
        }
    }

    /* 하드웨어 Timer 초기화 */
    if (!timer_init())
    {
        while (1)
        {
        }
    }

    /* Timer IRQ0 활성화 */
    interrupt_enable();

    while (1)
    {
        uint32_t now = timer_get_ms();

        /* 500ms마다 LEDR0 반전 */
        if ((uint32_t)(now - last_blink_ms) >= 500u)
        {
            last_blink_ms += 500u;

            led_value ^= 0x1u;
            LEDR = led_value;
        }

        /* 10ms 센서 샘플링 요청 확인 */
        if (timer_take_sample_tick())
        {
            sample_count++;

            /* 샘플 요청 100개마다 LEDR1 반전 */
            if (sample_count % 100u == 0u)
            {
                led_value ^= 0x2u;
                LEDR = led_value;
            }
        }
    }

    return 0;
}
