
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "adc.h"

/* ADC 채널 설정 */
#define LIGHT_CH 0
#define VIB1_CH 1
#define VIB2_CH 2

/* 출력 주기: 100ms */
#define SAMPLE_INTERVAL_US 100000

int main(void)
{
    uint16_t light = 0;
    uint16_t vib1 = 0;
    uint16_t vib2 = 0;

    printf("================================\n");
    printf(" DE1-SoC Sensor Monitoring Test\n");
    printf("================================\n");

    /* ADC 초기화 */
    adc_init();

    printf("ADC initialized.\n");
    printf("Start sensor monitoring...\n\n");

    while (1)
    {
        /* ADC 변환 갱신 요청 */
        adc_update();

        /*
         * 하드웨어의 변환 완료 시점은
         * ADC IP 사양에 맞게 검증해야 함.
         */
        usleep(1000);

        /* 센서 데이터 읽기 */
        bool light_ok = adc_read(LIGHT_CH, &light);
        bool vib1_ok = adc_read(VIB1_CH, &vib1);
        bool vib2_ok = adc_read(VIB2_CH, &vib2);

        if (light_ok && vib1_ok && vib2_ok)
        {
            printf("LIGHT: %4u | VIB1: %4u | VIB2: %4u\n",
                   (unsigned)light,
                   (unsigned)vib1,
                   (unsigned)vib2);
        }
        else
        {
            printf("[ERROR] ADC read failed!\n");
        }

        /* 100ms 간격으로 출력 */
        usleep(SAMPLE_INTERVAL_US);
    }

    return 0;
}
