
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../include/interrupt.h"
#include "../include/sensor_manager.h"
#include "../include/project_types.h"
#include "../include/timer.h"

#define LEDR (*(volatile unsigned int *)0xFF200000)
#define JTAG_UART_BASE 0xFF201000

    static void uart_print(const char *str)
    {
        volatile unsigned int *data =
            (volatile unsigned int *)JTAG_UART_BASE;

        volatile unsigned int *control =
            (volatile unsigned int *)(JTAG_UART_BASE + 4);

        while (*str)
        {
            while (((*control >> 16) & 0xFFFF) == 0)
            {
            }

            *data = (unsigned int)(unsigned char)*str++;
        }
    }


int main(void)
{
    SensorSample sample;
    uint32_t sample_count = 0;

    LEDR = 0x001; // 추가: main 진입 확인

    uart_print("=== Sensor Board Test ===\n");

    fflush(stdout); // 추가: 터미널에 즉시 출력

    sensor_manager_init();

    LEDR = 0x003; // 추가: 센서 초기화 완료 확인

    if (!timer_init())
    {
        uart_print("[ERROR] Timer initialization failed\n");
        return 1;
    }

    interrupt_init();
    interrupt_enable();

    LEDR = 0x007; // 추가: 인터럽트 활성화 확인

    uart_print("[INFO] System initialized\n");

    while (1)
    {
        /* Audio FIFO는 지속적으로 읽기 */
        sensor_manager_poll();

        /* 10ms 샘플링 요청 확인 */
        if (timer_take_sample_tick())
        {
            LEDR = 0x00F; // 추가: 샘플링 시작 확인

            if (!sensor_manager_sample(timer_get_ms(), &sample))
            {
                uart_print("[ERROR] Sensor sampling failed\n");
                continue;
            }

            LEDR = 0x01F; // 추가: 샘플링 완료 확인

            sample_count++;

            /* 100개 샘플마다 출력 = 약 1초 */
            if (sample_count % 100U == 0U)
            {
                char msg[128];

                snprintf(msg, sizeof(msg),
                    "Time: %lu ms | Vibration: %u | "
                    "Light: %u | Sound: %u\n",
                    (unsigned long)sample.timestamp_ms,
                    (unsigned int)sample.vibration,
                    (unsigned int)sample.light,
                    (unsigned int)sample.sound_level);

                uart_print(msg);
            }
        }
    }

    return 0;
}