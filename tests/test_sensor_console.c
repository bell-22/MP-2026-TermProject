
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "timer.h"
#include "interrupt.h"
#include "sensor_manager.h"
#include "project_types.h"

int main(void)
{
    SensorSample sample;
    uint32_t sample_count = 0;

    printf("=== Sensor Board Test ===\n");

    sensor_manager_init();

    if (!timer_init())
    {
        printf("[ERROR] Timer initialization failed\n");
        return 1;
    }

    interrupt_init();
    interrupt_enable();

    printf("[INFO] System initialized\n");

    while (1)
    {
        /* Audio FIFO는 지속적으로 읽기 */
        sensor_manager_poll();

        /* 10ms 샘플링 요청 확인 */
        if (timer_take_sample_tick())
        {
            if (!sensor_manager_sample(timer_get_ms(), &sample))
            {
                printf("[ERROR] Sensor sampling failed\n");
                continue;
            }

            sample_count++;

            /* 100개 샘플마다 출력 = 약 1초 */
            if (sample_count % 100U == 0U)
            {
                printf(
                    "Time: %lu ms | Vibration: %u | "
                    "Light: %u | Sound: %u\n",
                    (unsigned long)sample.timestamp_ms,
                    (unsigned int)sample.vibration,
                    (unsigned int)sample.light,
                    (unsigned int)sample.sound_level);
            }
        }
    }

    return 0;
}