/* LED	의미	확인 방법
LEDR0	10ms 샘플링 동작	매우 빠르게 깜빡임
LEDR1	진동 값 변화	Piezo 센서에 가벼운 진동
LEDR2	조도 값 변화	센서를 가리거나 빛 비추기
LEDR3	Audio 입력 존재	마이크에 소리 입력
LEDR9	초기화·센서 읽기 오류	켜지면 점검 필요 */

#include <stdint.h>
#include <stdbool.h>

#include "timer.h"
#include "interrupt.h"
#include "sensor_manager.h"
#include "project_types.h"

#define LEDR_BASE 0xFF200000u

#define LED_SAMPLE (1u << 0)
#define LED_VIB (1u << 1)
#define LED_LIGHT (1u << 2)
#define LED_SOUND (1u << 3)
#define LED_ERROR (1u << 9)

static volatile uint32_t *const ledr =
    (volatile uint32_t *)(uintptr_t)LEDR_BASE;

int main(void)
{
    uint32_t led_state = 0;
    uint16_t last_vibration = 0;
    uint16_t last_light = 0;
    bool first_sample = true;

    *ledr = 0;

    sensor_manager_init();

    if (!timer_init())
    {
        *ledr = LED_ERROR;
        while (1)
        {
        }
    }

    interrupt_init();
    interrupt_enable();

    while (1)
    {
        /*
         * Call frequently.
         * Do not wait 10 ms before draining audio FIFO.
         */
        sensor_manager_poll();

        if (timer_take_sample_tick())
        {
            SensorSample sample = {0};

            if (!sensor_manager_sample(
                    timer_get_ms(), &sample))
            {
                led_state |= LED_ERROR;
                *ledr = led_state;
                continue;
            }

            /* LED0 toggles every 10 ms. */
            led_state ^= LED_SAMPLE;

            /* LED1: vibration ADC value changed. */
            if (!first_sample &&
                sample.vibration != last_vibration)
            {
                led_state |= LED_VIB;
            }
            else
            {
                led_state &= ~LED_VIB;
            }

            /* LED2: light ADC value changed. */
            if (!first_sample &&
                sample.light != last_light)
            {
                led_state |= LED_LIGHT;
            }
            else
            {
                led_state &= ~LED_LIGHT;
            }

            /* LED3: nonzero audio amplitude. */
            if (sample.sound_level != 0)
            {
                led_state |= LED_SOUND;
            }
            else
            {
                led_state &= ~LED_SOUND;
            }

            last_vibration = sample.vibration;
            last_light = sample.light;
            first_sample = false;

            *ledr = led_state;
        }
    }
}
