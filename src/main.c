
/*
 * DE1-SoC Sensor Blackbox
 * Stage 1 Integration Debug Test
 *
 * - Vibration / Light / Sound input
 * - Timer IRQ0: 1ms tick, 10ms sampling
 * - KEY IRQ1: KEY0 / KEY1
 * - Initial baseline calibration: 1 second
 * - Ready signal: LEDR0~9 blink 3 times
 * - Event detection and classification
 * - Risk level evaluation
 * - LED / HEX output
 * - Audio-out alarm enabled
 * - Direct JTAG UART debug output
 *
 * Ring Buffer / Event Record: not integrated yet
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/interrupt.h"
#include "../include/timer.h"
#include "../include/sensor_manager.h"
#include "../include/project_types.h"
#include "../include/system_config.h"
#include "../include/event_detector.h"
#include "../include/event_classifier.h"
#include "../include/risk_level.h"
#include "../include/key_switch.h"
#include "../include/led_hex.h"
#include "../include/alarm.h"
#include "../include/alarm_audio_driver.h"

/* JTAG UART MMIO */
#define JTAG_UART_BASE 0xFF201000U

#define JTAG_UART_DATA \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x00U))

#define JTAG_UART_CONTROL \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x04U))


/*
 * 프로그램 시작 상태
 */
typedef enum {
    STARTUP_CALIBRATING = 0,
    STARTUP_READY_SIGNAL,
    STARTUP_MONITORING
} StartupState;


/*
 * Direct JTAG UART output
 */
static void uart_print(const char *str)
{
    if (str == NULL)
        return;

    while (*str != '\0')
    {
        uint32_t timeout = 1000000U;

        while (((JTAG_UART_CONTROL >> 16) & 0xFFFFU) == 0U)
        {
            if (--timeout == 0U)
                return;
        }

        JTAG_UART_DATA = (uint32_t)(uint8_t)*str;
        str++;
    }
}


int main(void)
{
    SensorSample sample;

    uint32_t sample_count = 0U;
    char msg[160];

    /*
     * 초기 평균 측정 상태
     */
    StartupState startup_state = STARTUP_CALIBRATING;

    uint32_t calibration_start_ms = 0U;

    /* 진동 / 조도 / 소음 누적 합계 */
    uint32_t vibration_sum = 0U;
    uint32_t light_sum = 0U;
    uint32_t sound_sum = 0U;

    /* 정상적으로 측정한 샘플 개수 */
    uint32_t baseline_count = 0U;

    /* 초기 1초 평균값 */
    uint16_t vibration_average = 0U;
    uint16_t light_average = 0U;
    uint16_t sound_average = 0U;


    /* =====================================
     * 1. Startup Debug
     * ===================================== */

    uart_print("\n=== Sensor Blackbox Stage 1 ===\n");
    uart_print("[BOOT] main entered\n");


    /* =====================================
     * 2. Module Initialization
     * ===================================== */

    key_init();
    uart_print("[INIT] KEY OK\n");

    event_detector_init();
    uart_print("[INIT] Event detector OK\n");

    risk_level_init();
    uart_print("[INIT] Risk level OK\n");

    led_hex_init();
    uart_print("[INIT] LED/HEX OK\n");

    sensor_manager_init();
    uart_print("[INIT] Sensor Manager OK\n");

    /* Audio-out enabled */
    alarm_audio_init();
    uart_print("[INIT] Audio driver init returned\n");

    alarm_init();
    uart_print("[INIT] Alarm OK\n");


    /* =====================================
     * 3. Timer / Interrupt Initialization
     * ===================================== */

    if (!interrupt_init())
    {
        uart_print("[ERROR] Interrupt init failed\n");
        return 1;
    }

    uart_print("[INIT] Interrupt OK\n");

    if (!timer_init())
    {
        uart_print("[ERROR] Timer init failed\n");
        return 1;
    }

    uart_print("[INIT] Timer OK\n");

    interrupt_enable();

    uart_print("[INFO] Interrupts enabled\n");
    uart_print("[INFO] KEY0: Alarm stop\n");
    uart_print("[INFO] KEY1: LED/HEX clear\n");

    /* 초기 1초 평균 측정 시작 */
    calibration_start_ms = timer_get_ms();

    uart_print("[CAL] Baseline calibration started\n");
    uart_print("[CAL] Keep sensors stable for 1 second\n");


    /* =====================================
     * 4. Main Loop
     * ===================================== */

    while (1)
    {
        uint32_t now_ms;
        uint8_t keys;

        /* Audio-in FIFO 처리 */
        sensor_manager_poll();

        now_ms = timer_get_ms();


        /* ---------------------------------
         * KEY Event Handling
         * --------------------------------- */

        keys = key_take_events();

        if ((keys & KEY_EVENT_STOP) != 0U)
        {
            alarm_stop();
            uart_print("[KEY0] Alarm stopped\n");
        }

        if ((keys & KEY_EVENT_CLEAR) != 0U)
        {
            alarm_stop();
            led_hex_clear();

            uart_print("[KEY1] LED/HEX cleared\n");
        }


        /* ---------------------------------
         * LED / Alarm Updates
         * --------------------------------- */

        led_hex_update(now_ms);

        alarm_update(now_ms);

        /* Audio-out PCM 출력 */
        alarm_audio_update();


        /* ---------------------------------
         * Ready Signal Completion
         * --------------------------------- */

        if (startup_state == STARTUP_READY_SIGNAL)
        {
            if (led_hex_ready_is_complete())
            {
                startup_state = STARTUP_MONITORING;

                uart_print("[READY] LED signal completed\n");
                uart_print("[INFO] Start monitoring\n");
            }
        }


        /* ---------------------------------
         * 10ms Sensor Sampling
         * --------------------------------- */

        if (timer_take_sample_tick())
        {
            if (!sensor_manager_sample(
                    timer_get_ms(), &sample))
            {
                uart_print("[ERROR] Sensor sampling failed\n");
                continue;
            }

            sample_count++;


            /* =============================
             * A. Initial Calibration
             * ============================= */

            if (startup_state == STARTUP_CALIBRATING)
            {
                /*
                 * 초기 1초 동안
                 * 세 센서의 측정값 누적
                 */
                vibration_sum +=
                    (uint32_t)sample.vibration;

                light_sum +=
                    (uint32_t)sample.light;

                sound_sum +=
                    (uint32_t)sample.sound_level;

                baseline_count++;
            }


            /* =============================
             * B. Event Monitoring
             * ============================= */

            else if (startup_state == STARTUP_MONITORING)
            {
                uint8_t flags;

                /*
                 * 세 센서 모두
                 * 초기 평균 대비 변화량으로 감지
                 */
                flags = event_detect(&sample);

                /*
                 * 동일한 평균값을 사용한 위험도 판정
                 */
                (void)risk_level_update(&sample);

                if (flags != EVENT_NONE)
                {
                    led_hex_show(flags);

                    /* Audio-out 경고음 시작 */
                    alarm_start(event_classify(flags));

                    (void)snprintf(
                        msg, sizeof(msg),
                        "[EVENT] Time:%lu ms | Flags:%u\n",
                        (unsigned long)sample.timestamp_ms,
                        (unsigned)flags
                    );

                    uart_print(msg);
                }
            }


            /* =============================
             * C. Sensor Debug Output
             * ============================= */

            if ((sample_count % 100U) == 0U)
            {
                (void)snprintf(
                    msg, sizeof(msg),
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


        /* =================================
         * 5. Baseline Calibration Completion
         * ================================= */

        if (startup_state == STARTUP_CALIBRATING)
        {
            uint32_t elapsed_ms =
                (uint32_t)(
                    timer_get_ms() - calibration_start_ms
                );

            if (elapsed_ms >= BASELINE_CALIBRATION_MS)
            {
                /* 정상 샘플이 없는 경우 */
                if (baseline_count == 0U)
                {
                    uart_print(
                        "[ERROR] No baseline samples collected\n"
                    );

                    alarm_stop();
                    led_hex_clear();

                    return 1;
                }


                /*
                 * 초기 1초 평균값 계산
                 */
                vibration_average =
                    (uint16_t)(
                        vibration_sum / baseline_count
                    );

                light_average =
                    (uint16_t)(
                        light_sum / baseline_count
                    );

                sound_average =
                    (uint16_t)(
                        sound_sum / baseline_count
                    );


                /* -------------------------
                 * Baseline Debug Output
                 * ------------------------- */

                (void)snprintf(
                    msg, sizeof(msg),
                    "[CAL] Samples:%lu | "
                    "Vibration Base:%u | "
                    "Light Base:%u | "
                    "Sound Base:%u\n",
                    (unsigned long)baseline_count,
                    (unsigned)vibration_average,
                    (unsigned)light_average,
                    (unsigned)sound_average
                );

                uart_print(msg);


                /* -------------------------
                 * Event Detector Baseline
                 * ------------------------- */

                event_detector_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average
                );


                /* -------------------------
                 * Risk Level Baseline
                 * ------------------------- */

                risk_level_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average
                );


                /* -------------------------
                 * Ready LED Signal
                 * ------------------------- */

                /*
                 * LEDR0~9 전체 3회 점멸
                 * 총 900ms
                 */
                led_hex_ready_start(timer_get_ms());

                /* Audio-out 독립 테스트: 800Hz 경고음 3회 ----- test*/
                alarm_start(DISPLAY_LIGHT);
                uart_print("[AUDIO TEST] Alarm started\n");

                startup_state = STARTUP_READY_SIGNAL;

                uart_print(
                    "[READY] LEDR0~9 blinking 3 times\n"
                );
            }
        }
    }

    return 0;
}
