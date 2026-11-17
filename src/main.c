
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "adc.h"
#include "system_config.h"

/* 콘솔 출력 간격: 100ms */
#define SAMPLE_INTERVAL_US 100000U

int main(void)
{
    uint16_t vibration = 0;
    uint16_t light = 0;

    printf("================================\n");
    printf(" DE1-SoC Sensor Monitoring Test\n");
    printf("================================\n");

    adc_init();

    printf("ADC initialized.\n");
    printf("Vibration: CH%u | Light: CH%u\n",
           (unsigned)VIBRATION_ADC_CHANNEL,
           (unsigned)LIGHT_ADC_CHANNEL);
    printf("Start sensor monitoring...\n\n");

    while (1)
    {
        /* ADC 변환 갱신 요청 */
        adc_update();

        /* 변환 완료 시점은 실제 보드에서 검증 필요 */
        usleep(1000);

        /* 공통 설정값에 따라 센서 데이터 읽기 */
        bool vibration_ok =
            adc_read(VIBRATION_ADC_CHANNEL, &vibration);

        bool light_ok =
            adc_read(LIGHT_ADC_CHANNEL, &light);

        if (vibration_ok && light_ok)
        {
            printf("VIBRATION: %4u | LIGHT: %4u\n",
                   (unsigned)vibration,
                   (unsigned)light);
        }
        else
        {
            printf("[ERROR] ADC read failed!\n");
        }

        usleep(SAMPLE_INTERVAL_US);
    }

    return 0;
}
