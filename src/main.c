
/*
 * DE1-SoC Sensor Blackbox
 *
 * 1차 통합 테스트
 * - 진동 / 조도 / 소음 센서 입력
 * - Timer IRQ0: 1ms 시간 관리, 10ms 센서 샘플링
 * - KEY IRQ1: KEY0, KEY1 입력
 * - 이벤트 감지 및 LED/HEX 표시
 * - Audio-out 경고음: 코드만 작성, 주석 처리
 *
 * Ring Buffer / Event Record는 아직 연결하지 않음
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../include/interrupt.h"
#include "../include/timer.h"
#include "../include/sensor_manager.h"
#include "../include/project_types.h"
#include "../include/event_detector.h"
#include "../include/key_switch.h"
#include "../include/led_hex.h"

/* Audio-out 실험 시 주석 해제 */
// #include "../include/alarm.h"
// #include "../include/alarm_audio_driver.h"
// #include "../include/event_classifier.h"


/* JTAG UART 하드웨어 주소 */
#define JTAG_UART_BASE 0xFF201000U

#define JTAG_UART_DATA \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x00U))

#define JTAG_UART_CONTROL \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x04U))


/* JTAG UART Terminal에 문자열 출력 */
static void uart_print(const char *str)
{
    while (*str != '\0')
    {
        /* 송신 FIFO에 공간이 생길 때까지 대기 */
        while (((JTAG_UART_CONTROL >> 16) & 0xFFFFU) == 0U)
        {
        }

        JTAG_UART_DATA = (uint32_t)(unsigned char)*str++;
    }
}


int main(void)
{
    SensorSample sample;
    uint32_t sample_count = 0U;
    char msg[128];


    /* =========================================
    1. 각 모듈 초기화
       ========================================= */

    /* KEY0, KEY1 입력 상태 초기화 */
    key_init();

    /* 이벤트 감지 상태 초기화 */
    event_detector_init();

    /* LED OFF, HEX 00으로 초기화 */
    led_hex_init();

    /* 진동, 조도, Audio-in 초기화 */
    sensor_manager_init();

    /* Audio-out 스피커 드라이버 초기화 */
    // alarm_audio_init();

    /* Audio 경고 패턴 초기화 */
    // alarm_init();


    /* =========================================
    2. Timer 및 인터럽트 설정
       ========================================= */

    /* 인터럽트 초기화 */
    if (!interrupt_init())
    {
        uart_print("[ERROR] Interrupt init failed\n");
        return 1;
    }

    /* 1ms 하드웨어 Timer 초기화 */
    if (!timer_init())
    {
        uart_print("[ERROR] Timer init failed\n");
        return 1;
    }

    /* Timer IRQ0 + KEY IRQ1 활성화 */
    interrupt_enable();

    uart_print("=== Sensor Blackbox Stage 1 ===\n");
    uart_print("[INFO] System initialized\n");
    uart_print("[INFO] KEY0: Alarm stop\n");
    uart_print("[INFO] KEY1: LED/HEX clear\n");


    /* =========================================
    3. 메인 반복문
       ========================================= */

    while (1)
    {
        uint32_t now_ms;
        uint8_t keys;

        /* Audio-in FIFO 데이터를 지속적으로 처리 */
        sensor_manager_poll();

        /* Timer 인터럽트로 갱신되는 현재 시간 */
        now_ms = timer_get_ms();


        /* =====================================
        4. KEY0 / KEY1 버튼 이벤트 처리
           ===================================== */

        /* KEY IRQ1에서 전달된 버튼 이벤트 가져오기 */
        keys = key_take_events();

        /* KEY0: 경고음 중지 */
        if ((keys & KEY_EVENT_STOP) != 0U)
        {
            // alarm_stop();

            uart_print("[KEY0] Alarm stop requested\n");
        }

        /* KEY1: 시스템 출력 초기화 */
        if ((keys & KEY_EVENT_CLEAR) != 0U)
        {
            // alarm_stop();

            /* 저장 기능 통합 후 사용 */
            // event_record_clear();

            led_hex_clear();

            uart_print("[KEY1] LED/HEX cleared\n");
        }



        /* =====================================
        5. LED 점멸 및 알람 출력 갱신
           ===================================== */

        /* LED를 250ms 간격으로 점멸 */
        led_hex_update(now_ms);

        /* 경고음의 시간별 출력 패턴 갱신 */
        // alarm_update(now_ms);

        /* Audio-out FIFO에 실제 소리 샘플 공급 */
        // alarm_audio_update();


        /* =====================================
        6. 10ms 센서 샘플링
           ===================================== */

        /* Timer IRQ0에서 생성한 샘플링 요청 확인 */
        if (timer_take_sample_tick())
        {
            uint8_t flags;

            /* 진동, 조도, 소음 센서값 수집 */
            if (!sensor_manager_sample(
                    timer_get_ms(), &sample))
            {
                uart_print("[ERROR] Sensor sampling failed\n");
                continue;
            }

            sample_count++;


            /* =================================
            7. 이벤트 감지
               ================================= */

            /* 센서값으로 충격, 소음, 조도 이벤트 판단 */
                        
            flags = event_detect(&sample);

            if (flags != EVENT_NONE)
            {
                /* LED/HEX에 이벤트 종류 표시 */
                led_hex_show(flags);

                /* Audio-out으로 이벤트별 경고음 출력 */
                // alarm_start(event_classify(flags));

                (void)snprintf(
                    msg,
                    sizeof(msg),
                    "[EVENT] Time:%lu ms | Flags:%u\n",
                    (unsigned long)sample.timestamp_ms,
                    (unsigned)flags
                );

                uart_print(msg);
            }



            /* =================================
            8. 센서값 콘솔 출력
               ================================= */

            /* 100개 샘플마다 약 1초 간격으로 출력 */
            if ((sample_count % 100U) == 0U)
            {
                (void)snprintf(
                    msg,
                    sizeof(msg),
                    "Time:%lu ms | Vibration:%u | "
                    "Light:%u | Sound:%u\n",
                    (unsigned long)sample.timestamp_ms,
                    (unsigned)sample.vibration,
                    (unsigned)sample.light,
                    (unsigned)sample.sound_level
                );

                uart_print(msg);
            }
        }
    }

    return 0;
}
