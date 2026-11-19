
/*
 * DE1-SoC Sensor Blackbox
 * Stage 1 + Storage Integration Test
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
 * - Ring Buffer: latest 500 samples
 * - Event Record: pre/post event samples
 * - Event History: latest 10 events
 * - Direct JTAG UART debug output
 *
 * KEY0: Alarm stop
 * KEY1: LED/HEX clear
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

/* Storage */
#include "../include/ring_buffer.h"
#include "../include/event_record.h"
#include "../include/event_history.h"

/* JTAG UART MMIO */
#define JTAG_UART_BASE 0xFF201000U

#define JTAG_UART_DATA \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x00U))

#define JTAG_UART_CONTROL \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x04U))

/* History 자동 출력 간격 */
#define HISTORY_PRINT_INTERVAL_MS 10000U

/* 프로그램 시작 상태 */
typedef enum
{
    STARTUP_CALIBRATING = 0,
    STARTUP_READY_SIGNAL,
    STARTUP_MONITORING
} StartupState;

/* =====================================
 * Direct JTAG UART Output
 * ===================================== */

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

/* =====================================
 * Event Flags Name
 * ===================================== */

static const char *event_flags_name(uint8_t flags)
{
    switch (flags)
    {
    case EVENT_SHOCK:
        return "SHOCK";

    case EVENT_SOUND:
        return "SOUND";

    case EVENT_LIGHT:
        return "LIGHT";

    case (EVENT_SHOCK | EVENT_SOUND):
        return "SHOCK+SOUND";

    case (EVENT_SHOCK | EVENT_LIGHT):
        return "SHOCK+LIGHT";

    case (EVENT_SOUND | EVENT_LIGHT):
        return "SOUND+LIGHT";

    case (EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT):
        return "ALL";

    default:
        return "UNKNOWN";
    }
}

/* =====================================
 * Event History Console Output
 * ===================================== */

static void print_event_history(void)
{
    char msg[160];
    uint32_t count = event_history_count();

    uart_print("\n========== EVENT HISTORY ==========\n");

    (void)snprintf(
        msg, sizeof(msg),
        "Stored Events: %lu / 10\n",
        (unsigned long)count);

    uart_print(msg);

    if (count == 0U)
    {
        uart_print("[HISTORY] No events recorded\n");
    }

    for (uint32_t i = 0U; i < count; i++)
    {
        EventHistoryEntry entry;

        if (event_history_get(i, &entry))
        {
            (void)snprintf(
                msg, sizeof(msg),
                "[%02lu] Time:%lu ms | Flags:%u | %s\n",
                (unsigned long)(i + 1U),
                (unsigned long)entry.timestamp_ms,
                (unsigned)entry.event_flags,
                event_flags_name(entry.event_flags));

            uart_print(msg);
        }
    }

    uart_print("===================================\n");
}

/* =====================================
 * Event Record Console Output
 * ===================================== */

static void print_event_record(void)
{
    char msg[160];
    EventInfo info;

    if (!event_record_get_info(&info))
    {
        uart_print("[RECORD] Empty\n");
        return;
    }

    (void)snprintf(
        msg, sizeof(msg),
        "[RECORD] Start:%lu ms | Flags:%u | "
        "Type:%s | Samples:%lu\n",
        (unsigned long)info.timestamp_ms,
        (unsigned)info.event_flags,
        event_flags_name(info.event_flags),
        (unsigned long)event_record_get_count());

    uart_print(msg);
}

/* =====================================
 * Main
 * ===================================== */

int main(void)
{
    SensorSample sample;

    uint32_t sample_count = 0U;
    char msg[160];

    StartupState startup_state = STARTUP_CALIBRATING;

    uint32_t calibration_start_ms = 0U;

    /* History 출력 기준 시각 */
    uint32_t last_history_print_ms = 0U;

    /* Record 저장 완료 로그 중복 방지 */
    bool record_saved_logged = false;

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

    /* ---------------------------------
     * Storage Initialization
     * --------------------------------- */

    ring_buffer_init();
    uart_print("[INIT] Ring Buffer OK\n");

    event_record_init();
    uart_print("[INIT] Event Record OK\n");

    event_history_init();
    uart_print("[INIT] Event History OK\n");

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

                /* 모니터링 시작 시각 기준 */
                last_history_print_ms = now_ms;

                uart_print("[READY] LED signal completed\n");
                uart_print("[INFO] Start monitoring\n");
                uart_print("[INFO] Storage monitoring enabled\n");
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
                 * 1. Event Detection
                 */
                flags = event_detect(&sample);

                /*
                 * 2. Risk Level
                 */
                (void)risk_level_update(&sample);

                /* -------------------------
                 * 3. New Event
                 * ------------------------- */

                if (flags != EVENT_NONE)
                {
                    EventInfo event;
                    EventHistoryEntry latest;

                    event.timestamp_ms = sample.timestamp_ms;
                    event.event_flags = flags;

                    /*
                     * Event History
                     * Record 상태와 상관없이 저장
                     */
                    if (event_history_push(
                            sample.timestamp_ms,
                            flags))
                    {
                        (void)snprintf(
                            msg, sizeof(msg),
                            "[HISTORY] Count:%lu / 10\n",
                            (unsigned long)event_history_count());

                        uart_print(msg);

                        if (event_history_get(0U, &latest))
                        {
                            (void)snprintf(
                                msg, sizeof(msg),
                                "[HISTORY] Latest:%lu ms | "
                                "Flags:%u | %s\n",
                                (unsigned long)latest.timestamp_ms,
                                (unsigned)latest.event_flags,
                                event_flags_name(latest.event_flags));

                            uart_print(msg);
                        }
                    }

                    /*
                     * Event Record
                     *
                     * EMPTY  -> 기록 시작
                     * ACTIVE -> Flags 누적
                     * SAVED  -> 기존 기록 유지
                     */
                    if (event_record_trigger(&event))
                    {
                        uart_print("[RECORD] Recording started\n");
                    }
                    else
                    {
                        event_record_add_flags(flags);
                    }

                    /*
                     * 기존 LED / HEX 처리
                     */
                    led_hex_show(flags);

                    /*
                     * 기존 Audio-out 처리
                     */
                    alarm_start(event_classify(flags));

                    /*
                     * Event Debug Output
                     */
                    (void)snprintf(
                        msg, sizeof(msg),
                        "[EVENT] Time:%lu ms | Flags:%u | %s\n",
                        (unsigned long)sample.timestamp_ms,
                        (unsigned)flags,
                        event_flags_name(flags));

                    uart_print(msg);

                    /* Record 현재 상태 출력 */
                    print_event_record();
                }

                /* -------------------------
                 * 4. Record Sample Update
                 * ------------------------- */

                /*
                 * ACTIVE 상태에서 현재 샘플 저장
                 *
                 * 이벤트 발생 샘플도
                 * post-event 첫 샘플로 저장됨.
                 */
                event_record_update(&sample);

                /* -------------------------
                 * 5. Ring Buffer Update
                 * ------------------------- */

                /*
                 * Record 처리 이후 Ring Buffer 저장
                 *
                 * 현재 샘플이 pre-event 기록에
                 * 중복 포함되는 것을 방지
                 */
                ring_buffer_push(&sample);

                /* -------------------------
                 * 6. Record Save Check
                 * ------------------------- */

                if (!record_saved_logged &&
                    event_record_is_saved())
                {
                    record_saved_logged = true;

                    uart_print("\n========== RECORD SAVED ==========\n");

                    print_event_record();

                    uart_print(
                        "[RECORD] Post-event sampling completed\n");

                    uart_print("==================================\n");
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
                    (unsigned)sample.sound_level);

                uart_print(msg);
            }
        }

        /* =================================
         * 5. Baseline Calibration Completion
         * ================================= */

        if (startup_state == STARTUP_CALIBRATING)
        {
            uint32_t elapsed_ms =
                (uint32_t)(timer_get_ms() - calibration_start_ms);

            if (elapsed_ms >= BASELINE_CALIBRATION_MS)
            {
                /* 정상 샘플이 없는 경우 */
                if (baseline_count == 0U)
                {
                    uart_print(
                        "[ERROR] No baseline samples collected\n");

                    alarm_stop();
                    led_hex_clear();

                    return 1;
                }

                /* 초기 1초 평균값 계산 */
                vibration_average =
                    (uint16_t)(vibration_sum / baseline_count);

                light_average =
                    (uint16_t)(light_sum / baseline_count);

                sound_average =
                    (uint16_t)(sound_sum / baseline_count);

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
                    (unsigned)sound_average);

                uart_print(msg);

                /* -------------------------
                 * Event Detector Baseline
                 * ------------------------- */

                event_detector_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* -------------------------
                 * Risk Level Baseline
                 * ------------------------- */

                risk_level_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* -------------------------
                 * Ready LED Signal
                 * ------------------------- */

                led_hex_ready_start(timer_get_ms());

                /* 기존 Audio-out 독립 테스트 유지 */
                alarm_start(DISPLAY_LIGHT);
                uart_print("[AUDIO TEST] Alarm started\n");

                startup_state = STARTUP_READY_SIGNAL;

                uart_print(
                    "[READY] LEDR0~9 blinking 3 times\n");
            }
        }

        /* =================================
         * 6. Automatic History Output
         * ================================= */

        if (startup_state == STARTUP_MONITORING)
        {
            uint32_t current_ms = timer_get_ms();

            if ((uint32_t)(current_ms - last_history_print_ms) >= HISTORY_PRINT_INTERVAL_MS)
            {
                last_history_print_ms = current_ms;

                print_event_history();
            }
        }
    }

    return 0;
}
