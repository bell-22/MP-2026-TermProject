
/*
 * DE1-SoC Sensor Blackbox
 *
 * - Sensor sampling: 10ms
 * - Sensor console output: 1 second
 * - Initial calibration: 5 seconds
 * - Ring Buffer: latest 500 samples
 * - Event Record: latest event, Pre 500 + Post 500
 * - Event History: latest 10 events
 *
 * Console:
 * [SENSOR] continuous periodic output
 * [EVENT] immediately
 * [HISTORY] immediately
 * [RECORD] BEFORE immediately
 * [RECORD] AFTER every 100 post samples
 * [RECORD] SAVED after 500 post samples
 * EVENT HISTORY every 10 seconds (oldest -> newest)
 *
 * KEY0: Alarm stop
 * KEY1: Alarm stop + LED/HEX clear
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

#define SENSOR_PRINT_INTERVAL_MS 1000U
#define HISTORY_PRINT_INTERVAL_MS 10000U

/* Record 100개마다 대표 센서값 출력 */
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
 * JTAG UART Output
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
        msg,
        sizeof(msg),
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
 *
 * 오래된 이벤트 -> 최신 이벤트
 * 시간 오름차순
 * ===================================== */

static void print_event_history(void)
{
    char msg[160];
    uint32_t count = event_history_count();

    uart_print(
        "\n========== EVENT HISTORY ==========\n");

    (void)snprintf(
        msg,
        sizeof(msg),
        "Stored Events: %lu / 10\n",
        (unsigned long)count);

    uart_print(msg);

    if (count == 0U)
    {
        uart_print("[HISTORY] Empty\n");
    }

    /*
     * event_history_get(0)은 최신 이벤트.
     *
     * 따라서 index를 거꾸로 조회하여
     * 오래된 이벤트부터 출력.
     */
    for (uint32_t i = 0U; i < count; i++)
    {
        EventHistoryEntry entry;

        uint32_t index = count - 1U - i;

        if (event_history_get(index, &entry))
        {
            (void)snprintf(
                msg,
                sizeof(msg),
                "[%02lu] %lu ms | Flags:%u | %s\n",
                (unsigned long)(i + 1U),
                (unsigned long)entry.timestamp_ms,
                (unsigned)entry.event_flags,
                event_flags_name(entry.event_flags));

            uart_print(msg);
        }
    }

    uart_print(
        "===================================\n");
}

/* =====================================
 * Record BEFORE Output
 *
 * 이벤트 발생 즉시 출력.
 * 실제 Record에 복사된 Pre 500개 중
 * 100개 간격으로 5개 대표 샘플 출력.
 * ===================================== */

static void print_record_before(
    uint32_t pre_count,
    uint8_t flags)
{
    char msg[160];
    SensorSample stored;

    (void)snprintf(
        msg,
        sizeof(msg),
        "[RECORD] %s | BEFORE (%lu samples)\n",
        event_flags_name(flags),
        (unsigned long)pre_count);

    uart_print(msg);

    if (pre_count == 0U)
    {
        uart_print("  No previous samples\n");
        return;
    }

    /*
     * Pre 500개 기준:
     * index 0, 100, 200, 300, 400
     *
     * 최대 5개 대표 샘플 출력.
     */
    for (uint32_t i = 0U;
         i < pre_count;
         i += RECORD_PRINT_STEP)
    {
        if (event_record_get_sample(i, &stored))
        {
            print_sensor_sample("  ", &stored);
        }
    }
}

/* =====================================
 * Record AFTER Output
 *
 * 이벤트 발생 이후 수집되는 샘플을
 * 100개마다 1개씩 즉시 출력.
 *
 * Post 100, 200, 300, 400, 500
 * ===================================== */

static void print_record_after(
    uint32_t post_count,
    const SensorSample *sample)
{
    if (sample == NULL)
    {
        return;
    }

    if (post_count > 0U &&
        (post_count % RECORD_PRINT_STEP) == 0U)
    {
        print_sensor_sample("  ", sample);
    }
}

/* =====================================
 * Record Completion Output
 * ===================================== */

static void print_record_saved(
    uint32_t pre_count)
{
    EventInfo info;
    char msg[160];

    if (!event_record_get_info(&info))
    {
        uart_print("[RECORD] Empty\n");
        return;
    }

    uint32_t total = event_record_get_count();
    uint32_t post = total - pre_count;

    (void)snprintf(
        msg,
        sizeof(msg),
        "\n[RECORD] Pre:%lu | Post:%lu | Total:%lu\n",
        (unsigned long)pre_count,
        (unsigned long)post,
        (unsigned long)total);

    uart_print(msg);

    uart_print("[RECORD] SAVED\n\n");
}

/* =====================================
 * Main
 * ===================================== */

int main(void)
{
    SensorSample sample;
    char msg[160];

    StartupState startup_state =
        STARTUP_CALIBRATING;

    uint32_t calibration_start_ms = 0U;
    uint32_t last_sensor_print_ms = 0U;
    uint32_t last_history_print_ms = 0U;

    /* 현재 Event Record 상태 */
    uint32_t record_pre_count = 0U;
    uint32_t record_post_count = 0U;

    bool record_collecting = false;

    /* Calibration */
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

    uart_print(
        "\n=== Sensor Blackbox Stage 1 ===\n");

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
     * 3. Timer / Interrupt Initialization
     * ===================================== */

    if (!interrupt_init())
    {
        uart_print(
            "[ERROR] Interrupt init failed\n");
        return 1;
    }

    uart_print("[INIT] Interrupt OK\n");

    if (!timer_init())
    {
        uart_print(
            "[ERROR] Timer init failed\n");
        return 1;
    }

    uart_print("[INIT] Timer OK\n");

    interrupt_enable();

    uart_print("[INFO] Interrupts enabled\n");
    uart_print("[INFO] KEY0: Alarm stop\n");
    uart_print("[INFO] KEY1: LED/HEX clear\n");

    calibration_start_ms = timer_get_ms();
    last_sensor_print_ms = calibration_start_ms;

    uart_print(
        "[CAL] Baseline calibration started\n");

    uart_print(
        "[CAL] Keep sensors stable for 5 seconds\n");

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
         * KEY Handling
         * --------------------------------- */

        keys = key_take_events();

        if ((keys & KEY_EVENT_STOP) != 0U)
        {
            alarm_stop();

            uart_print(
                "[KEY0] Alarm stopped\n");
        }

        if ((keys & KEY_EVENT_CLEAR) != 0U)
        {
            alarm_stop();
            led_hex_clear();

            uart_print(
                "[KEY1] LED/HEX cleared\n");
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

        if (startup_state ==
            STARTUP_READY_SIGNAL)
        {
            if (led_hex_ready_is_complete())
            {
                startup_state =
                    STARTUP_MONITORING;

                last_history_print_ms =
                    now_ms;

                uart_print(
                    "[READY] LED signal completed\n");

                uart_print(
                    "[INFO] Start monitoring\n");
            }
        }

        /* ---------------------------------
         * 10ms Sensor Sampling
         * --------------------------------- */

        if (timer_take_sample_tick())
        {
            if (!sensor_manager_sample(
                    timer_get_ms(),
                    &sample))
            {
                uart_print(
                    "[ERROR] Sensor sampling failed\n");

                continue;
            }

            /* =============================
             * A. Calibration
             * ============================= */

            if (startup_state ==
                STARTUP_CALIBRATING)
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

            else if (startup_state ==
                     STARTUP_MONITORING)
            {
                uint8_t flags;

                flags = event_detect(&sample);

                (void)risk_level_update(&sample);

                /* -------------------------
                 * 1. Event Detection
                 * ------------------------- */

                if (flags != EVENT_NONE)
                {
                    EventInfo event;
                    bool history_saved;
                    bool record_started;

                    event.timestamp_ms =
                        sample.timestamp_ms;

                    event.event_flags =
                        flags;

                    /* ---------------------
                     * History 저장
                     * --------------------- */

                    history_saved =
                        event_history_push(
                            sample.timestamp_ms,
                            flags);

                    /* ---------------------
                     * 최신 Event Record 교체
                     * --------------------- */

                    record_started =
                        event_record_trigger(
                            &event);

                    if (record_started)
                    {
                        /*
                         * trigger 직후에는
                         * 이전 샘플만 저장됨.
                         */
                        record_pre_count =
                            event_record_get_count();

                        record_post_count = 0U;

                        record_collecting = true;
                    }
                    else
                    {
                        record_collecting = false;
                    }

                    /* ---------------------
                     * LED / Alarm
                     * --------------------- */

                    led_hex_show(flags);

                    alarm_start(
                        event_classify(flags));

                    /* ---------------------
                     * EVENT 즉시 출력
                     * --------------------- */

                    (void)snprintf(
                        msg,
                        sizeof(msg),
                        "\n[EVENT] %lu ms | Flags:%u | %s\n",
                        (unsigned long)sample.timestamp_ms,
                        (unsigned)flags,
                        event_flags_name(flags));

                    uart_print(msg);

                    /* ---------------------
                     * HISTORY 즉시 출력
                     * --------------------- */

                    if (history_saved)
                    {
                        (void)snprintf(
                            msg,
                            sizeof(msg),
                            "[HISTORY] Saved (%lu/10)\n",
                            (unsigned long)event_history_count());

                        uart_print(msg);
                    }

                    /* ---------------------
                     * BEFORE 즉시 출력
                     * --------------------- */

                    if (record_started)
                    {
                        print_record_before(
                            record_pre_count,
                            flags);

                        (void)snprintf(
                            msg,
                            sizeof(msg),
                            "[RECORD] %s | AFTER\n",
                            event_flags_name(flags));

                        uart_print(msg);
                    }
                }

                /* -------------------------
                 * 2. Event Record Update
                 * ------------------------- */

                /*
                 * 이벤트가 발생하지 않은
                 * 일반 샘플도 Post에 저장.
                 *
                 * 새로운 이벤트가 발생하면
                 * 기존 Record는 교체됨.
                 */
                event_record_update(&sample);

                /* -------------------------
                 * 3. AFTER Sample Output
                 * ------------------------- */

                if (record_collecting)
                {
                    uint32_t total =
                        event_record_get_count();

                    /*
                     * 현재까지 저장된
                     * 이후 센서 샘플 개수
                     */
                    if (total >= record_pre_count)
                    {
                        uint32_t new_post_count =
                            total - record_pre_count;

                        /*
                         * 새 샘플이 실제로
                         * 추가되었을 때만 출력.
                         */
                        if (new_post_count >
                            record_post_count)
                        {
                            record_post_count =
                                new_post_count;

                            /*
                             * 100, 200, 300,
                             * 400, 500번째 출력
                             */
                            print_record_after(
                                record_post_count,
                                &sample);
                        }
                    }

                    /* ---------------------
                     * 4. Record Saved
                     * --------------------- */

                    if (event_record_is_saved())
                    {
                        record_collecting = false;

                        /*
                         * 총 샘플 수 및
                         * 저장 완료 출력.
                         */
                        print_record_saved(
                            record_pre_count);
                    }
                }
            }

            /* =============================
             * C. Ring Buffer Update
             * ============================= */

            /*
             * Calibration / Ready / Monitoring
             * 모든 상태에서 샘플 저장.
             *
             * Record 처리 이후 실행.
             */
            ring_buffer_push(&sample);

            /* =============================
             * D. Sensor Output
             * ============================= */

            /*
             * 약 1초마다 실제 센서값 출력.
             * AFTER 저장 중에도 유지.
             */
            if ((uint32_t)(sample.timestamp_ms -
                           last_sensor_print_ms) >= SENSOR_PRINT_INTERVAL_MS)
            {
                last_sensor_print_ms =
                    sample.timestamp_ms;

                print_sensor_sample(
                    "[SENSOR]",
                    &sample);
            }
        }

        /* =================================
         * 5. Baseline Calibration Complete
         * ================================= */

        if (startup_state ==
            STARTUP_CALIBRATING)
        {
            uint32_t elapsed_ms =
                (uint32_t)(timer_get_ms() -
                           calibration_start_ms);

            if (elapsed_ms >=
                    BASELINE_CALIBRATION_MS &&
                ring_buffer_count() >=
                    PRE_EVENT_SAMPLES)
            {
                if (baseline_count == 0U)
                {
                    uart_print(
                        "[ERROR] No baseline samples\n");

                    alarm_stop();
                    led_hex_clear();

                    return 1;
                }

                /* ---------------------
                 * Baseline Average
                 * --------------------- */

                vibration_average =
                    (uint16_t)(vibration_sum /
                               baseline_count);

                light_average =
                    (uint16_t)(light_sum /
                               baseline_count);

                sound_average =
                    (uint16_t)(sound_sum /
                               baseline_count);

                /* ---------------------
                 * Baseline Output
                 * --------------------- */

                (void)snprintf(
                    msg,
                    sizeof(msg),
                    "[CAL] Samples:%lu | V:%u L:%u S:%u\n",
                    (unsigned long)baseline_count,
                    (unsigned)vibration_average,
                    (unsigned)light_average,
                    (unsigned)sound_average);

                uart_print(msg);

                /* ---------------------
                 * Detector Baseline
                 * --------------------- */

                event_detector_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* ---------------------
                 * Risk Level Baseline
                 * --------------------- */

                risk_level_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average);

                /* ---------------------
                 * Ready LED Signal
                 * --------------------- */

                led_hex_ready_start(
                    timer_get_ms());

                /* Audio-out 테스트 유지 */
                alarm_start(DISPLAY_LIGHT);

                uart_print(
                    "[AUDIO TEST] Alarm started\n");

                startup_state =
                    STARTUP_READY_SIGNAL;

                uart_print(
                    "[READY] LEDR0~9 blinking 3 times\n");
            }
        }

        /* =================================
         * 6. Automatic History Output
         * ================================= */

        /*
         * 모니터링 시작 후
         * 10초마다 최근 10건 출력.
         *
         * 오래된 -> 최신 순서.
         */
        if (startup_state ==
            STARTUP_MONITORING)
        {
            uint32_t current_ms =
                timer_get_ms();

            if ((uint32_t)(current_ms -
                           last_history_print_ms) >= HISTORY_PRINT_INTERVAL_MS)
            {
                last_history_print_ms =
                    current_ms;

                print_event_history();
            }
        }
    }

    return 0;
}
