
/*
 * DE1-SoC Sensor Blackbox
 * Stage 1 Integration Debug Test
 *
<<<<<<< Updated upstream
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
=======
 * Sensor sampling: 10ms
 * Sensor console output: 1 second
 * Initial calibration: 5 seconds
 * VGA update: 1 second
 *
 * Ring Buffer: latest 500 samples
 * Event Record: Pre 500 + Post 500
 * Event History: latest 10 events
 *
 * KEY0: Alarm stop
 * KEY1: Alarm stop + LED/HEX clear
 * KEY2: VGA real-time monitoring
 * KEY3: VGA event history
 *
 * KEY1 does NOT delete stored data.
>>>>>>> Stashed changes
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
#include "../include/vga.h"

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

<<<<<<< Updated upstream
=======
/* =====================================
 * VGA Screen Selection
 * ===================================== */

typedef enum
{
    UI_SCREEN_REALTIME = 0,
    UI_SCREEN_HISTORY = 1
} UiScreen;

/* =====================================
 * JTAG UART Output
 * ===================================== */
>>>>>>> Stashed changes

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

<<<<<<< Updated upstream
=======
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
 * Delta Calculation
 * ===================================== */

/* 두 측정값의 절댓값 차이 */
static uint16_t abs_delta_u16(
    uint16_t a,
    uint16_t b)
{
    if (a >= b)
    {
        return (uint16_t)(a - b);
    }

    return (uint16_t)(b - a);
}

/*
 * 이벤트 발생 당시 변화량 계산
 *
 * 기준:
 * 초기 5초 평균값 대비 절댓값 변화량
 *
 * 단일 이벤트:
 * 해당 센서의 변화량
 *
 * 복합 이벤트:
 * 해당하는 센서들의 변화량 중 최대값
 */
static uint16_t event_delta(
    const SensorSample *sample,
    uint8_t flags,
    uint16_t base_v,
    uint16_t base_l,
    uint16_t base_s)
{
    uint16_t max_delta = 0U;
    uint16_t delta;

    if (sample == NULL)
    {
        return 0U;
    }

    /* 충격 이벤트 */
    if ((flags & EVENT_SHOCK) != 0U)
    {
        delta = abs_delta_u16(
            sample->vibration,
            base_v
        );

        if (delta > max_delta)
        {
            max_delta = delta;
        }
    }

    /* 소음 이벤트 */
    if ((flags & EVENT_SOUND) != 0U)
    {
        delta = abs_delta_u16(
            sample->sound_level,
            base_s
        );

        if (delta > max_delta)
        {
            max_delta = delta;
        }
    }

    /* 조도 이벤트 */
    if ((flags & EVENT_LIGHT) != 0U)
    {
        delta = abs_delta_u16(
            sample->light,
            base_l
        );

        if (delta > max_delta)
        {
            max_delta = delta;
        }
    }

    return max_delta;
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
        (unsigned)sample->sound_level
    );

    uart_print(msg);
}

/* =====================================
 * Event History Output
 *
 * 오래된 이벤트 -> 최신 이벤트
 * 시간 오름차순
 *
 * VGA 화면은 최신순으로 표시하지만
 * JTAG UART는 기존 출력 순서를 유지한다.
 * ===================================== */

static void print_event_history(void)
{
    char msg[160];

    uint32_t count = event_history_count();

    uart_print(
        "\n========== EVENT HISTORY ==========\n"
    );

    (void)snprintf(
        msg,
        sizeof(msg),
        "Stored Events: %lu / 10\n",
        (unsigned long)count
    );

    uart_print(msg);

    if (count == 0U)
    {
        uart_print("[HISTORY] Empty\n");
    }

    /*
     * event_history_get(0)은 가장 최신 이벤트.
     *
     * 오래된 이벤트부터 출력하기 위해
     * index를 역순으로 조회한다.
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
                "[%02lu] %lu ms | Flags:%u | %s | Delta:%u\n",
                (unsigned long)(i + 1U),
                (unsigned long)entry.timestamp_ms,
                (unsigned)entry.event_flags,
                event_flags_name(entry.event_flags),
                (unsigned)entry.delta
            );

            uart_print(msg);
        }
    }

    uart_print(
        "===================================\n"
    );
}

/* =====================================
 * Record BEFORE Output
 *
 * 이벤트 이전 500개 중
 * 100개 간격 대표 샘플 출력
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
        (unsigned long)pre_count
    );

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
}

/* =====================================
 * Record AFTER Output
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

static void print_record_saved(uint32_t pre_count)
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
        (unsigned long)total
    );

    uart_print(msg);

    uart_print("[RECORD] SAVED\n\n");
}

/* =====================================
 * Main
 * ===================================== */
>>>>>>> Stashed changes

int main(void)
{
    SensorSample sample;

    uint32_t sample_count = 0U;
    char msg[160];

    /*
     * 초기 평균 측정 상태
     */
    StartupState startup_state = STARTUP_CALIBRATING;

    /* 기본 VGA 화면: 실시간 모니터링 */
    UiScreen selected_screen =
        UI_SCREEN_REALTIME;

    /* 현재 위험 단계 */
    RiskLevel current_risk =
        RISK_NORMAL;

    uint32_t calibration_start_ms = 0U;

<<<<<<< Updated upstream
    /* 진동 / 조도 / 소음 누적 합계 */
    uint32_t vibration_sum = 0U;
    uint32_t light_sum = 0U;
    uint32_t sound_sum = 0U;
=======
    /* 현재 Event Record 상태 */
    uint32_t record_pre_count = 0U;
    uint32_t record_post_count = 0U;

    bool record_collecting = false;

    /* =====================================
     * Calibration Variables
     * ===================================== */

    uint64_t vibration_sum = 0U;
    uint64_t light_sum = 0U;
    uint64_t sound_sum = 0U;
>>>>>>> Stashed changes

    /* 정상적으로 측정한 샘플 개수 */
    uint32_t baseline_count = 0U;

    /* 초기 1초 평균값 */
    uint16_t vibration_average = 0U;
    uint16_t light_average = 0U;
    uint16_t sound_average = 0U;


    /* =====================================
     * 1. Startup Debug
     * ===================================== */

<<<<<<< Updated upstream
    uart_print("\n=== Sensor Blackbox Stage 1 ===\n");
=======
    uart_print(
        "\n=== Sensor Blackbox with VGA ===\n"
    );

>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
    /* Audio-out enabled */
=======
    ring_buffer_init();
    uart_print("[INIT] Ring Buffer OK\n");

    event_record_init();
    uart_print("[INIT] Event Record OK\n");

    event_history_init();
    uart_print("[INIT] Event History OK\n");

    /* VGA 초기화 */
    vga_init();
    uart_print("[INIT] VGA OK\n");

>>>>>>> Stashed changes
    alarm_audio_init();
    uart_print("[INIT] Audio driver init returned\n");

    alarm_init();
    uart_print("[INIT] Alarm OK\n");


    /* =====================================
     * 3. Timer / Interrupt Initialization
     * ===================================== */

    if (!interrupt_init())
    {
<<<<<<< Updated upstream
        uart_print("[ERROR] Interrupt init failed\n");
=======
        uart_print(
            "[ERROR] Interrupt init failed\n"
        );

>>>>>>> Stashed changes
        return 1;
    }

    uart_print("[INIT] Interrupt OK\n");

    if (!timer_init())
    {
<<<<<<< Updated upstream
        uart_print("[ERROR] Timer init failed\n");
=======
        uart_print(
            "[ERROR] Timer init failed\n"
        );

>>>>>>> Stashed changes
        return 1;
    }

    uart_print("[INIT] Timer OK\n");

    interrupt_enable();

    uart_print("[INFO] Interrupts enabled\n");

    uart_print("[INFO] KEY0: Alarm stop\n");
    uart_print("[INFO] KEY1: Alarm/LED/HEX clear\n");
    uart_print("[INFO] KEY2: VGA Real-time\n");
    uart_print("[INFO] KEY3: VGA History\n");

    /* 초기 1초 평균 측정 시작 */
    calibration_start_ms = timer_get_ms();
<<<<<<< Updated upstream

    uart_print("[CAL] Baseline calibration started\n");
    uart_print("[CAL] Keep sensors stable for 1 second\n");

=======

    last_sensor_print_ms =
        calibration_start_ms;

    uart_print(
        "[CAL] Baseline calibration started\n"
    );

    uart_print(
        "[CAL] Keep sensors stable for 5 seconds\n"
    );
>>>>>>> Stashed changes

    /* =====================================
     * 4. Main Loop
     * ===================================== */

    while (1)
    {
        uint32_t now_ms;
        uint8_t keys;

        /* =================================
         * Audio-in FIFO 처리
         * ================================= */

        sensor_manager_poll();

        now_ms = timer_get_ms();

<<<<<<< Updated upstream

        /* ---------------------------------
         * KEY Event Handling
         * --------------------------------- */
=======
        /* =================================
         * KEY Handling
         * ================================= */
>>>>>>> Stashed changes

        keys = key_take_events();

        /* ---------------------------------
         * KEY0: 경고음 중지
         * --------------------------------- */

        if ((keys & KEY_EVENT_STOP) != 0U)
        {
            alarm_stop();
<<<<<<< Updated upstream
            uart_print("[KEY0] Alarm stopped\n");
=======

            uart_print(
                "[KEY0] Alarm stopped\n"
            );
>>>>>>> Stashed changes
        }

        /* ---------------------------------
         * KEY1: 경고 및 표시 초기화
         *
         * 저장된 데이터는 삭제하지 않는다.
         * --------------------------------- */

        if ((keys & KEY_EVENT_CLEAR) != 0U)
        {
            alarm_stop();
            led_hex_clear();

<<<<<<< Updated upstream
            uart_print("[KEY1] LED/HEX cleared\n");
=======
            /*
             * 아래 저장 데이터는 유지한다.
             *
             * Ring Buffer
             * Event Record
             * Event History
             *
             * 초기화 함수는 호출하지 않는다.
             */

            uart_print(
                "[KEY1] Alarm and display cleared\n"
            );
>>>>>>> Stashed changes
        }


        /* ---------------------------------
<<<<<<< Updated upstream
         * LED / Alarm Updates
=======
         * KEY2: VGA 실시간 모니터링
>>>>>>> Stashed changes
         * --------------------------------- */

        if ((keys & KEY_EVENT_VIEW_LIVE) != 0U)
        {
            /*
             * 같은 화면에서 KEY2를 다시 눌러도
             * 화면 갱신 요청을 전달한다.
             */
            vga_set_screen(
                VGA_SCREEN_REALTIME
            );

            if (selected_screen !=
                UI_SCREEN_REALTIME)
            {
                selected_screen =
                    UI_SCREEN_REALTIME;

                uart_print(
                    "[KEY2] VGA Real-time screen\n"
                );
            }
        }

        /* ---------------------------------
         * KEY3: VGA 과거 이벤트 화면
         * --------------------------------- */

        if ((keys & KEY_EVENT_VIEW_HISTORY) != 0U)
        {
            /*
             * 같은 화면에서 KEY3을 다시 눌러도
             * 최신 History를 다시 조회한다.
             *
             * 이때 새로운 이벤트를 저장하지 않는다.
             */
            vga_set_screen(
                VGA_SCREEN_HISTORY
            );

            if (selected_screen !=
                UI_SCREEN_HISTORY)
            {
                selected_screen =
                    UI_SCREEN_HISTORY;

                uart_print(
                    "[KEY3] VGA History screen\n"
                );
            }
        }

        /* =================================
         * LED / Alarm Update
         * ================================= */

        led_hex_update(now_ms);

        alarm_update(now_ms);

        /* Audio-out PCM 출력 */
        alarm_audio_update();

<<<<<<< Updated upstream

        /* ---------------------------------
=======
        /* =================================
>>>>>>> Stashed changes
         * Ready Signal Completion
         * ================================= */

        if (startup_state == STARTUP_READY_SIGNAL)
        {
            if (led_hex_ready_is_complete())
            {
                startup_state = STARTUP_MONITORING;

<<<<<<< Updated upstream
                uart_print("[READY] LED signal completed\n");
                uart_print("[INFO] Start monitoring\n");
            }
        }


        /* ---------------------------------
=======
                last_history_print_ms =
                    now_ms;

                uart_print(
                    "[READY] LED signal completed\n"
                );

                uart_print(
                    "[INFO] Start monitoring\n"
                );
            }
        }

        /* =================================
>>>>>>> Stashed changes
         * 10ms Sensor Sampling
         * ================================= */

        if (timer_take_sample_tick())
        {
            if (!sensor_manager_sample(
                    timer_get_ms(), &sample))
            {
<<<<<<< Updated upstream
                uart_print("[ERROR] Sensor sampling failed\n");
=======
                uart_print(
                    "[ERROR] Sensor sampling failed\n"
                );

>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
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
=======
                /* 위험 단계 계산 후 VGA용으로 보존 */
                current_risk =
                    risk_level_update(&sample);

                /* -------------------------
                 * 1. Event Detection / Save
                 * ------------------------- */

                if (flags != EVENT_NONE)
                {
                    EventInfo event;
                    EventInfo previous;
                    EventHistoryEntry latest;

                    bool upgraded = false;
                    bool history_saved = false;
                    bool record_started = false;

                    uint16_t delta;

                    event.timestamp_ms =
                        sample.timestamp_ms;

                    event.event_flags =
                        flags;

                    /*
                     * 이벤트 발생 당시 변화량 계산.
                     *
                     * 초기 평균 대비 변화량.
                     * 복합 이벤트는 최대 변화량.
                     */
                    delta = event_delta(
                        &sample,
                        flags,
                        vibration_average,
                        light_average,
                        sound_average
                    );

                    /* ---------------------
                     * 복합 이벤트 확장 확인
                     *
                     * 기존 Record 수집 중
                     * 새로운 이벤트가
                     * 기존 플래그를 포함하면서
                     * 새로운 플래그가 추가된 경우
                     * --------------------- */

                    if (record_collecting &&
                        !event_record_is_saved() &&
                        event_record_get_info(&previous) &&
                        event_history_get(0U, &latest) &&
                        previous.timestamp_ms ==
                            latest.timestamp_ms &&
                        previous.event_flags ==
                            latest.event_flags &&
                        flags != previous.event_flags &&
                        (uint8_t)(
                            flags & previous.event_flags
                        ) == previous.event_flags)
                    {
                        /*
                         * Record의 이벤트 종류만 갱신.
                         * 이전/이후 센서 데이터 유지.
                         */
                        if (event_record_upgrade(
                                previous.timestamp_ms,
                                flags))
                        {
                            /*
                             * History의 최신 항목 갱신.
                             *
                             * 이벤트 개수는 증가하지 않고
                             * DELTA는 더 큰 값으로 갱신.
                             */
                            upgraded =
                                event_history_upgrade_latest_with_delta(
                                    previous.timestamp_ms,
                                    flags,
                                    delta
                                );
                        }
                    }

                    /* ---------------------
                     * 새로운 독립 이벤트
                     * --------------------- */

                    if (!upgraded)
                    {
                        /*
                         * 최근 10개 History 저장
                         *
                         * 10개를 초과하면
                         * 가장 오래된 항목 덮어쓰기.
                         */
                        history_saved =
                            event_history_push_with_delta(
                                sample.timestamp_ms,
                                flags,
                                delta
                            );

                        /*
                         * 최신 Event Record 생성
                         *
                         * 이전 최대 500개와
                         * 이후 500개 저장.
                         */
                        record_started =
                            event_record_trigger(
                                &event
                            );

                        if (record_started)
                        {
                            /*
                             * Trigger 직후 Record에는
                             * 이전 데이터만 저장됨.
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
                    }

                    /* ---------------------
                     * LED / HEX / Alarm
                     * --------------------- */

                    led_hex_show(flags);

                    alarm_start(
                        event_classify(flags)
                    );

                    /* ---------------------
                     * EVENT 즉시 출력
                     * --------------------- */

                    (void)snprintf(
                        msg,
                        sizeof(msg),
                        "\n[EVENT] %lu ms | Flags:%u | %s | Delta:%u\n",
                        (unsigned long)sample.timestamp_ms,
                        (unsigned)flags,
                        event_flags_name(flags),
                        (unsigned)delta
                    );

                    uart_print(msg);

                    /* ---------------------
                     * HISTORY / RECORD 출력
                     * --------------------- */

                    if (upgraded)
                    {
                        /*
                         * 복합 이벤트 확장 성공
                         *
                         * History 개수 증가 없음.
                         * Record 수집 초기화 없음.
                         */
                        uart_print(
                            "[HISTORY] Updated latest flags/delta\n"
                        );

                        uart_print(
                            "[RECORD] Flags updated (capture continues)\n"
                        );
                    }
                    else
                    {
                        /* 새로운 History 항목 저장 */
                        if (history_saved)
                        {
                            (void)snprintf(
                                msg,
                                sizeof(msg),
                                "[HISTORY] Saved (%lu/10)\n",
                                (unsigned long)event_history_count()
                            );

                            uart_print(msg);
                        }

                        /* 새로운 Record 이전 데이터 출력 */
                        if (record_started)
                        {
                            print_record_before(
                                record_pre_count,
                                flags
                            );

                            (void)snprintf(
                                msg,
                                sizeof(msg),
                                "[RECORD] %s | AFTER\n",
                                event_flags_name(flags)
                            );

                            uart_print(msg);
                        }
                    }
                }

                /* -------------------------
                 * 2. Event Record Update
                 * ------------------------- */

                /*
                 * 일반 샘플도 Post에 저장.
                 *
                 * 복합 이벤트 확장 시
                 * 기존 Record에 이어서 저장.
                 *
                 * 새로운 독립 이벤트 발생 시
                 * 최신 Record로 교체 후 저장.
                 */
                event_record_update(&sample);

                /* -------------------------
                 * 3. AFTER Sample Output
                 * ------------------------- */

                if (record_collecting)
                {
                    uint32_t total =
                        event_record_get_count();

                    if (total >= record_pre_count)
                    {
                        uint32_t new_post_count =
                            total - record_pre_count;

                        /*
                         * 새로운 샘플이
                         * 실제로 추가된 경우
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
                                &sample
                            );
                        }
                    }

                    /* ---------------------
                     * 4. Record Saved
                     * --------------------- */

                    if (event_record_is_saved())
                    {
                        record_collecting = false;

                        print_record_saved(
                            record_pre_count
                        );
                    }
>>>>>>> Stashed changes
                }
            }


            /* =============================
             * C. Sensor Debug Output
             * ============================= */

<<<<<<< Updated upstream
            if ((sample_count % 100U) == 0U)
=======
            /*
             * 약 1초마다 실제 센서값 출력.
             * Record 수집 중에도 유지.
             */
            if ((uint32_t)(
                    sample.timestamp_ms -
                    last_sensor_print_ms
                ) >= SENSOR_PRINT_INTERVAL_MS)
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
                uart_print(msg);
=======
                print_sensor_sample(
                    "[SENSOR]",
                    &sample
                );
>>>>>>> Stashed changes
            }
        }


        /* =================================
         * 5. Baseline Calibration Completion
         * ================================= */

        if (startup_state == STARTUP_CALIBRATING)
        {
            uint32_t elapsed_ms =
                (uint32_t)(
<<<<<<< Updated upstream
                    timer_get_ms() - calibration_start_ms
=======
                    timer_get_ms() -
                    calibration_start_ms
>>>>>>> Stashed changes
                );

            if (elapsed_ms >= BASELINE_CALIBRATION_MS)
            {
                /* 정상 샘플이 없는 경우 */
                if (baseline_count == 0U)
                {
                    uart_print(
<<<<<<< Updated upstream
                        "[ERROR] No baseline samples collected\n"
=======
                        "[ERROR] No baseline samples\n"
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream

                /* -------------------------
                 * Event Detector Baseline
                 * ------------------------- */
=======
                /* ---------------------
                 * Event Detector Baseline
                 * --------------------- */
>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
=======

                /* ---------------------
                 * VGA Baseline
                 *
                 * 실시간 화면의 LIMIT와
                 * EXCESS 계산에 사용.
                 * --------------------- */

                vga_set_baseline(
                    vibration_average,
                    light_average,
                    sound_average
                );
>>>>>>> Stashed changes


                /* -------------------------
                 * Ready LED Signal
                 * ------------------------- */

<<<<<<< Updated upstream
                /*
                 * LEDR0~9 전체 3회 점멸
                 * 총 900ms
                 */
                led_hex_ready_start(timer_get_ms());

                /* Audio-out 독립 테스트: 800Hz 경고음 3회 ----- test*/
=======
                led_hex_ready_start(
                    timer_get_ms()
                );

                /* 기존 Audio-out 테스트 유지 */
>>>>>>> Stashed changes
                alarm_start(DISPLAY_LIGHT);
                uart_print("[AUDIO TEST] Alarm started\n");

                startup_state = STARTUP_READY_SIGNAL;

                uart_print(
<<<<<<< Updated upstream
                    "[READY] LEDR0~9 blinking 3 times\n"
                );
=======
                    "[AUDIO TEST] Alarm started\n"
                );

                startup_state =
                    STARTUP_READY_SIGNAL;

                uart_print(
                    "[READY] LEDR0~9 blinking 3 times\n"
                );
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
         *
         * VGA History 화면은
         * 별도로 1초마다 다시 조회한다.
         */
        if (startup_state ==
            STARTUP_MONITORING)
        {
            uint32_t current_ms =
                timer_get_ms();

            if ((uint32_t)(
                    current_ms -
                    last_history_print_ms
                ) >= HISTORY_PRINT_INTERVAL_MS)
            {
                last_history_print_ms =
                    current_ms;

                print_event_history();
>>>>>>> Stashed changes
            }
        }

        /* =================================
         * 7. VGA Display Update
         * ================================= */

        /*
         * VGA 화면 갱신은 센서 측정과 분리.
         *
         * sensor sampling: 10ms
         * VGA refresh: 1000ms
         *
         * KEY2: Real-time screen
         * KEY3: History screen
         *
         * History 화면을 보고 있는 동안에도
         * 최근 이벤트 10개를 주기적으로
         * 다시 읽어 표시한다.
         *
         * 기존 이벤트 데이터를
         * 삭제하거나 중복 저장하지 않는다.
         */
        vga_update(
            timer_get_ms(),
            current_risk
        );
    }

    return 0;
}
