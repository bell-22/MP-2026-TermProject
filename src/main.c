
/*
 * DE1-SoC Sensor Blackbox
 * Stage 1 + Storage Integration Test
 *
 * - Initial calibration: 5 seconds
 * - Sensor sampling: 10ms
 * - Sensor console output: 1 second
 * - Ring Buffer: latest 500 samples
 * - Event Record: latest event pre/post 500 samples
 * - Event History: latest 10 events
 * - History console output: 10 seconds
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

#include "../include/ring_buffer.h"
#include "../include/event_record.h"
#include "../include/event_history.h"

/* =====================================
 * Configuration
 * ===================================== */

#define JTAG_UART_BASE 0xFF201000U

#define JTAG_UART_DATA \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x00U))

#define JTAG_UART_CONTROL \
    (*(volatile uint32_t *)(uintptr_t)(JTAG_UART_BASE + 0x04U))

#define INITIAL_CALIBRATION_MS 5000U
#define SENSOR_PRINT_INTERVAL_MS 1000U
#define HISTORY_PRINT_INTERVAL_MS 10000U

/* Record 대표 샘플 출력 간격: 100개 = 약 1초 */
#define RECORD_PRINT_STEP 100U

/* =====================================
 * Startup State
 * ===================================== */

typedef enum
{
    STARTUP_CALIBRATING = 0,
    STARTUP_READY_SIGNAL,
    STARTUP_MONITORING
} StartupState;

/* =====================================
 * Direct JTAG UART
 * ===================================== */

static void uart_print(const char *str)
{
    if (str == NULL)
    {
        return;
    }

    while (*str != '\0')
    {
        uint32_t timeout = 1000000U;

        while (((JTAG_UART_CONTROL >> 16) & 0xFFFFU) == 0U)
        {
            if (--timeout == 0U)
            {
                return;
            }
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
 * Sensor Sample Output
 * ===================================== */

static void print_sensor_sample(
    const char *prefix,
    const SensorSample *sample)
{
    char msg[160];

    if (prefix == NULL || sample == NULL)
    {
        return;
    }

    (void)snprintf(
        msg, sizeof(msg),
        "%s %lu ms | V:%u L:%u S:%u\n",
        prefix,
        (unsigned long)sample->timestamp_ms,
        (unsigned)sample->vibration,
        (unsigned)sample->light,
        (unsigned)sample->sound_level);

    uart_print(msg);
}

/* =====================================
 * Event History Output
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
        uart_print("[HISTORY] Empty\n");
    }

    for (uint32_t i = 0U; i < count; i++)
    {
        EventHistoryEntry entry;

        if (event_history_get(i, &entry))
        {
            (void)snprintf(
                msg, sizeof(msg),
                "[%02lu] %lu ms | Flags:%u | %s\n",
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
 * Record BEFORE Output
 *
 * Record에 복사된 이전 센서 데이터에서
 * 100개 간격으로 대표 샘플 출력
 *
 * 모든 센서 데이터는 실제 Record에 보존
 * ===================================== */

static void print_record_before(uint32_t pre_count,
                                uint8_t flags)
{
    SensorSample stored;
    char msg[100];

    (void)snprintf(
        msg, sizeof(msg),
        "[RECORD] %s | BEFORE (%lu samples)\n",
        event_flags_name(flags),
        (unsigned long)pre_count);
    uart_print(msg);

    if (pre_count == 0U)
    {
        uart_print("  No previous samples\n");
        return;
    }

    for (uint32_t i = 0U;
         i < pre_count;
         i += RECORD_PRINT_STEP)
    {
        if (event_record_get_sample(i, &stored))
        {
            print_sensor_sample("  ", &stored);
        }
    }

    /* 마지막 샘플도 출력 */
    if ((pre_count - 1U) % RECORD_PRINT_STEP != 0U)
    {
        if (event_record_get_sample(pre_count - 1U, &stored))
        {
            print_sensor_sample("  ", &stored);
        }
    }
}

/* =====================================
 * Record AFTER Output
 *
 * 이벤트 이후 수집한 실제 샘플 출력
 * 첫 번째 샘플 + 이후 100개 간격
 * ===================================== */

static void print_record_after(
    uint32_t post_count,
    const SensorSample *sample)
{
    if (sample == NULL)
    {
        return;
    }

    if (post_count == 1U ||
        (post_count % RECORD_PRINT_STEP) == 0U)
    {
        print_sensor_sample("  ", sample);
    }
}

/* =====================================
 * Record Completion Output
 * ===================================== */

static void print_record_saved(uint32_t pre_count)
{
    EventInfo info;
    char msg[160];

    if (!event_record_get_info(&info))
    {
        return;
    }

    uint32_t total = event_record_get_count();
    uint32_t post = total - pre_count;

    uart_print("\n===== RECORD SAVED =====\n");

    (void)snprintf(
        msg, sizeof(msg),
        "[RECORD] %lu ms | Flags:%u | %s\n",
        (unsigned long)info.timestamp_ms,
        (unsigned)info.event_flags,
        event_flags_name(info.event_flags));
    uart_print(msg);

    (void)snprintf(
        msg, sizeof(msg),
        "[RECORD] Pre:%lu | Post:%lu | Total:%lu\n",
        (unsigned long)pre_count,
        (unsigned long)post,
        (unsigned long)total);
    uart_print(msg);

    uart_print("[RECORD] SAVED\n");
    uart_print("========================\n");
}

/* =====================================
 * Main
 * ===================================== */

int main(void)
{
    SensorSample sample;
    char msg[160];

    StartupState startup_state = STARTUP_CALIBRATING;

    uint32_t calibration_start_ms = 0U;
    uint32_t last_sensor_print_ms = 0U;
    uint32_t last_history_print_ms = 0U;

    /* 최신 Record의 이전 샘플 수 */
    uint32_t record_pre_count = 0U;

    /* Record 이후 샘플 수 */
    uint32_t record_post_count = 0U;

    /* 완료 로그 중복 출력 방지 */
    bool record_saved_logged = false;

    /* 초기 5초 센서값 합계 */
    uint64_t vibration_sum = 0U;
    uint64_t light_sum = 0U;
    uint64_t sound_sum = 0U;

    uint32_t baseline_count = 0U;

    uint16_t vibration_average = 0U;
    uint16_t light_average = 0U;
    uint16_t sound_average = 0U;

    /* =====================================
     * 1. Startup
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

    ring_buffer_init();
    uart_print("[INIT] Ring Buffer OK\n");

    event_record_init();
    uart_print("[INIT] Event Record OK\n");

    event_history_init();
    uart_print("[INIT] Event History OK\n");

    alarm_audio_init();
    uart_print("[INIT] Audio driver init returned\n");

    alarm_init();
    uart_print("[INIT] Alarm OK\n");

    /* =====================================
     * 3. Timer / Interrupt
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
    last_sensor_print_ms = calibration_start_ms;

    uart_print("[CAL] Baseline calibration started\n");
    uart_print("[CAL] Keep sensors stable for 5 seconds\n");

    /* =====================================
     * 4. Main Loop
     * ===================================== */

    while (1)
    {
        uint32_t now_ms;
        uint8_t keys;

        sensor_manager_poll();

        now_ms = timer_get_ms();

        /* ---------------------------------
         * KEY Handling
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
         * LED / Alarm Update
         * --------------------------------- */

        led_hex_update(now_ms);
        alarm_update(now_ms);
        alarm_audio_update();

        /* ---------------------------------
         * Ready Signal Completion
         * --------------------------------- */

        if (startup_state == STARTUP_READY_SIGNAL)
        {
            if (led_hex_ready_is_complete())
            {
                startup_state = STARTUP_MONITORING;

                last_history_print_ms = now_ms;

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

            /* =============================
             * A. Initial Calibration
             * ============================= */

            if (startup_state == STARTUP_CALIBRATING)
            {
                vibration_sum +=
                    (uint64_t)sample.vibration;

                light_sum +=
                    (uint64_t)sample.light;

                sound_sum +=
                    (uint64_t)sample.sound_level;

                baseline_count++;
            }

            /* =============================
             * B. Event Monitoring
             * ============================= */

            else if (startup_state == STARTUP_MONITORING)
            {
                uint8_t flags;
                bool record_started = false;
                bool history_saved = false;

                flags = event_detect(&sample);

                (void)risk_level_update(&sample);

                /* -------------------------
                 * 1. New Event
                 * ------------------------- */

                if (flags != EVENT_NONE)
                {
                    EventInfo event;

                    event.timestamp_ms = sample.timestamp_ms;
                    event.event_flags = flags;

                    /* History 저장 */
                    history_saved = event_history_push(
                        sample.timestamp_ms,
                        flags);

                    /* 최신 이벤트 기준 Record 교체 */
                    if (event_record_trigger(&event))
                    {
                        record_saved_logged = false;
                        record_started = true;

                        /*
                         * trigger 직후에는
                         * 이전 센서 샘플만 존재
                         */
                        record_pre_count =
                            event_record_get_count();

                        record_post_count = 0U;
                    }

                    /* 기존 이벤트 출력 기능 */
                    led_hex_show(flags);
                    alarm_start(event_classify(flags));

                    /* -------------------------
                     * 2. EVENT Output
                     * ------------------------- */

                    (void)snprintf(
                        msg, sizeof(msg),
                        "\n[EVENT] %lu ms | Flags:%u | %s\n",
                        (unsigned long)sample.timestamp_ms,
                        (unsigned)flags,
                        event_flags_name(flags));

                    uart_print(msg);

                    /* -------------------------
                     * 3. BEFORE Output
                     * ------------------------- */

                    if (record_started)
                    {
                        print_record_before(
                            record_pre_count,
                            flags);

                        (void)snprintf(
                            msg, sizeof(msg),
                            "[RECORD] %s | AFTER\n",
                            event_flags_name(flags));

                        uart_print(msg);
                    }
                }

                /* -------------------------
                 * 4. Record Update
                 * ------------------------- */

                /*
                 * 이벤트 이후 샘플 저장
                 */
                event_record_update(&sample);

                /*
                 * Record가 활성 상태라면
                 * 이후 샘플 개수 갱신
                 */
                if (!event_record_is_saved() &&
                    event_record_get_count() >
                        record_pre_count)
                {
                    uint32_t total =
                        event_record_get_count();

                    record_post_count =
                        total - record_pre_count;

                    /*
                     * AFTER 대표 샘플 출력
                     */
                    print_record_after(
                        record_post_count,
                        &sample);
                }

                /* -------------------------
                 * 5. Record Completion
                 * ------------------------- */

                if (!record_saved_logged &&
                    event_record_is_saved())
                {
                    record_saved_logged = true;

                    /*
                     * 500번째 AFTER 샘플 출력
                     */
                    record_post_count =
                        event_record_get_count() - record_pre_count;

                    if (record_post_count ==
                        POST_EVENT_SAMPLES)
                    {
                        print_record_after(
                            record_post_count,
                            &sample);
                    }

                    print_record_saved(
                        record_pre_count);
                }

                /* -------------------------
                 * 6. History Count Output
                 * ------------------------- */

                if (history_saved)
                {
                    (void)snprintf(
                        msg, sizeof(msg),
                        "[HISTORY] Saved (%lu/10)\n",
                        (unsigned long)event_history_count());

                    uart_print(msg);
                }
            }

            /* =============================
             * C. Ring Buffer Update
             * ============================= */

            /*
             * Calibration / Ready / Monitoring
             * 모든 상태에서 저장
             *
             * 반드시 Record 처리 이후 수행
             */
            ring_buffer_push(&sample);

            /* =============================
             * D. Sensor Console Output
             * ============================= */

            if ((uint32_t)(sample.timestamp_ms - last_sensor_print_ms) >= SENSOR_PRINT_INTERVAL_MS)
            {
                last_sensor_print_ms = sample.timestamp_ms;

                print_sensor_sample(
                    "[SENSOR]",
                    &sample);
            }
        }

        /* =================================
         * 5. Baseline Calibration Completion
         * ================================= */

        if (startup_state == STARTUP_CALIBRATING)
        {
            uint32_t elapsed_ms =
                (uint32_t)(timer_get_ms() - calibration_start_ms);

            if (elapsed_ms >= INITIAL_CALIBRATION_MS &&
                ring_buffer_count() >= PRE_EVENT_SAMPLES)
            {
                if (baseline_count == 0U)
                {
                    uart_print(
                        "[ERROR] No baseline samples collected\n");

                    alarm_stop();
                    led_hex_clear();

                    return 1;
                }

                vibration_average =
                    (uint16_t)(vibration_sum / baseline_count);

                light_average =
                    (uint16_t)(light_sum / baseline_count);

                sound_average =
                    (uint16_t)(sound_sum / baseline_count);

                /* Baseline Output */

                (void)snprintf(
                    msg, sizeof(msg),
                    "[CAL] Samples:%lu | "
                    "V:%u L:%u S:%u\n",
                    (unsigned long)baseline_count,
                    (unsigned)vibration_average,
                    (unsigned)light_average,
                    (unsigned)sound_average);

                uart_print(msg);

                /* Detector Baseline */

                event_detector_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* Risk Level Baseline */

                risk_level_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* Ready LED Signal */

                led_hex_ready_start(timer_get_ms());

                /* 기존 Audio-out 테스트 유지 */
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
