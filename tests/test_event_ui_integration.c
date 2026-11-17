
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "alarm.h"
#include "alarm_audio_driver.h"
#include "event_classifier.h"
#include "event_detector.h"
#include "key_switch.h"
#include "led_hex.h"
#include "project_types.h"
#include "system_state.h"

/* Mac/PC Mock 테스트 전용 */
#ifndef LED_HEX_HOST_TEST
#error "Compile with -DLED_HEX_HOST_TEST"
#endif

/* LED/HEX 가상 레지스터 조회 함수 */
uint32_t led_hex_test_read_ledr(void);
uint32_t led_hex_test_read_hex30(void);

/* 실제 Audio-out 대신 사용하는 Mock */
static uint32_t s_audio_frequency_hz;

void alarm_audio_set_frequency(uint32_t frequency_hz)
{
    s_audio_frequency_hz = frequency_hz;
}

/* 테스트 결과 및 모의 기록 횟수 */
static unsigned s_pass;
static unsigned s_fail;
static unsigned s_record_start_count;
static unsigned s_record_clear_count;


/* 실제값과 예상값 비교 */
static void check_u32(
    const char *name,
    uint32_t actual,
    uint32_t expected
)
{
    if (actual == expected) {
        printf("[PASS] %-36s expected=%u actual=%u\n",
            name,
            (unsigned)expected,
            (unsigned)actual);
        s_pass++;
    } else {
        printf("[FAIL] %-36s expected=%u actual=%u\n",
            name,
            (unsigned)expected,
            (unsigned)actual);
        s_fail++;
    }
}


/* 모든 Event & UI 모듈 초기화 */
static void reset_modules(void)
{
    event_detector_init();
    system_state_init();
    key_init();
    led_hex_init();
    alarm_init();

    s_record_start_count = 0U;
    s_record_clear_count = 0U;
}


/*
 * 실제 main.c의 센서 처리 흐름을 모의한다.
 *
 * 센서값 -> 이벤트 감지 -> 분류
 *       -> 상태 전환 -> LED/HEX -> Audio
 *
 * 실제 Event Record 저장은 다음 통합 단계에서
 * 연결하므로 여기서는 기록 시작 성공을 가정한다.
 */
static uint8_t process_sample(
    const SensorSample *sample
)
{
    uint8_t flags = event_detect(sample);

    /* 감시 상태에서 이벤트 발생 */
    if ((flags != EVENT_NONE) &&
        (system_state_get() == STATE_MONITORING)) {

        /* Mock: 기록 시작 성공 가정 */
        s_record_start_count++;

        system_state_set(STATE_RECORDING);

        /* 이벤트 출력 */
        led_hex_show(flags);

        /* 이벤트 종류에 따른 Audio 경고 */
        alarm_start(event_classify(flags));
    }

    /* 비차단 출력 갱신 */
    alarm_update(sample->timestamp_ms);
    led_hex_update(sample->timestamp_ms);

    return flags;
}


/* 메인 루프의 출력 갱신 동작 */
static void update_outputs(uint32_t now_ms)
{
    alarm_update(now_ms);
    led_hex_update(now_ms);
}


/*
 * 메인 루프에서 KEY 이벤트 처리
 *
 * KEY0: 경고음 중지
 * KEY1: 기록 초기화 + 감시 상태 복귀
 */
static void process_keys(void)
{
    uint8_t keys = key_take_events();

    if ((keys & KEY_EVENT_STOP) != 0U) {
        alarm_stop();
    }

    if ((keys & KEY_EVENT_CLEAR) != 0U) {
        alarm_stop();

        /* Mock: 실제 event_record_clear() 대신 */
        s_record_clear_count++;

        led_hex_clear();
        system_state_set(STATE_MONITORING);
    }
}


/* 1. 복합 이벤트부터 KEY 초기화까지 검사 */
static void test_multiple_event_and_keys(void)
{
    SensorSample sample = {
        0U, 100U, 100U, 1000U
    };

    uint8_t flags;

    printf("\n=== Multiple event / KEY flow ===\n");

    reset_modules();

    /* 초기 상태 */
    check_u32("initial state",
            (uint32_t)system_state_get(),
            STATE_MONITORING);

    check_u32("initial LED",
            led_hex_test_read_ledr(), 0U);

    check_u32("initial HEX",
            led_hex_test_read_hex30(), 0x3F3FU);

    /* 정상 센서값 */
    flags = process_sample(&sample);
    check_u32("normal flags", flags, EVENT_NONE);

    /* 첫 번째 충격 + 소음 입력 */
    sample.timestamp_ms = 10U;
    sample.vibration = 2100U;
    sample.sound_level = 1200U;

    flags = process_sample(&sample);

    check_u32("first abnormal flags",
            flags, EVENT_NONE);

    /* 두 번째 연속 입력 -> 복합 이벤트 확정 */
    sample.timestamp_ms = 20U;
    flags = process_sample(&sample);

    check_u32("combined flags",
            flags, EVENT_SHOCK | EVENT_SOUND);

    check_u32("combined classifier",
            event_classify(flags), 4U);

    check_u32("record start (mock)",
            s_record_start_count, 1U);

    check_u32("state RECORDING",
            (uint32_t)system_state_get(),
            STATE_RECORDING);

    check_u32("combined LED0+1",
            led_hex_test_read_ledr(), 3U);

    check_u32("combined HEX 04",
            led_hex_test_read_hex30(), 0x3F66U);

    /* 복합 이벤트 첫 음: 600Hz */
    check_u32("combined low tone",
            s_audio_frequency_hz, 600U);

    check_u32("alarm active",
            (uint32_t)alarm_is_active(), 1U);

    /* 동일한 이상 상태 중복 감지 방지 */
    sample.timestamp_ms = 30U;
    flags = process_sample(&sample);

    check_u32("held abnormal no duplicate",
            flags, EVENT_NONE);

    check_u32("still one record start",
            s_record_start_count, 1U);

    /* 250ms 경과: LED OFF */
    update_outputs(270U);

    check_u32("LED off after 250ms",
            led_hex_test_read_ledr(), 0U);

    /* 400ms 경과: 1200Hz로 전환 */
    update_outputs(420U);

    check_u32("combined high tone",
            s_audio_frequency_hz, 1200U);

    /* 500ms 경과: LED 다시 ON */
    update_outputs(520U);

    check_u32("LED on after 500ms",
            led_hex_test_read_ledr(), 3U);

    /* KEY0: 경고음 해제 */
    key_irq_notify(KEY_EVENT_STOP, 530U);
    process_keys();

    check_u32("KEY0 mutes audio",
            s_audio_frequency_hz, 0U);

    check_u32("KEY0 ends alarm",
            (uint32_t)alarm_is_active(), 0U);

    check_u32("KEY0 preserves RECORDING",
            (uint32_t)system_state_get(),
            STATE_RECORDING);

    check_u32("KEY0 preserves HEX",
            led_hex_test_read_hex30(), 0x3F66U);

    /*
     * Mock: 이후 500개 샘플 기록이
     * 완료되었다고 가정한다.
     */
    system_state_set(STATE_SAVED);

    check_u32("mock recording complete",
            (uint32_t)system_state_get(),
            STATE_SAVED);

    /* 정상 범위로 돌아가 감지 래치 해제 */
    sample.timestamp_ms = 1000U;
    sample.vibration = 100U;
    sample.sound_level = 100U;

    (void)process_sample(&sample);

    /* SAVED 상태에서 새로운 충격 발생 */
    sample.timestamp_ms = 2010U;
    sample.vibration = 2100U;

    (void)process_sample(&sample);

    sample.timestamp_ms = 2020U;
    flags = process_sample(&sample);

    check_u32("new shock detected in SAVED",
            flags, EVENT_SHOCK);

    /* 감지는 되어도 새로운 기록은 시작하지 않음 */
    check_u32("SAVED blocks new trigger",
            s_record_start_count, 1U);

    check_u32("SAVED keeps HEX 04",
            led_hex_test_read_hex30(), 0x3F66U);

    check_u32("SAVED keeps audio silent",
            s_audio_frequency_hz, 0U);

    /* KEY1: 기록 초기화 */
    key_irq_notify(KEY_EVENT_CLEAR, 2100U);
    process_keys();

    check_u32("KEY1 clear request (mock)",
            s_record_clear_count, 1U);

    check_u32("KEY1 returns MONITORING",
            (uint32_t)system_state_get(),
            STATE_MONITORING);

    check_u32("KEY1 LED off",
            led_hex_test_read_ledr(), 0U);

    check_u32("KEY1 HEX 00",
            led_hex_test_read_hex30(), 0x3F3FU);

    /* 초기화 후 새로운 이벤트 감지 */
    sample.timestamp_ms = 2110U;
    sample.vibration = 100U;

    (void)process_sample(&sample);

    sample.timestamp_ms = 3110U;
    sample.vibration = 2100U;

    (void)process_sample(&sample);

    sample.timestamp_ms = 3120U;
    flags = process_sample(&sample);

    check_u32("new event after KEY1",
            flags, EVENT_SHOCK);

    check_u32("second record start (mock)",
            s_record_start_count, 2U);

    check_u32("single HEX 01",
            led_hex_test_read_hex30(), 0x3F06U);

    check_u32("single tone 800Hz",
            s_audio_frequency_hz, 800U);
}


/* 2. 충격, 소음, 조도 각각의 전체 출력 검사 */
static void test_single_event_paths(void)
{
    static const struct {
        const char *name;
        uint16_t vibration;
        uint16_t sound;
        uint16_t light;
        uint8_t flags;
        uint32_t hex;
    } cases[] = {
        {
            "SHOCK",
            2100U, 100U, 1000U,
            EVENT_SHOCK, 0x3F06U
        },
        {
            "SOUND",
            100U, 1200U, 1000U,
            EVENT_SOUND, 0x3F5BU
        },
        {
            "LIGHT",
            100U, 100U, 1700U,
            EVENT_LIGHT, 0x3F4FU
        }
    };

    unsigned i;

    printf("\n=== Single event paths ===\n");

    for (i = 0U;
        i < (unsigned)(sizeof(cases) / sizeof(cases[0]));
        i++) {

        SensorSample sample = {
            0U, 100U, 100U, 1000U
        };

        uint8_t flags;

        reset_modules();

        /* 정상 데이터로 초기 기준 설정 */
        (void)process_sample(&sample);

        /* 첫 번째 이상 데이터 */
        sample.timestamp_ms = 10U;
        sample.vibration = cases[i].vibration;
        sample.sound_level = cases[i].sound;
        sample.light = cases[i].light;

        (void)process_sample(&sample);

        /* 두 번째 이상 데이터 */
        sample.timestamp_ms = 20U;
        flags = process_sample(&sample);

        printf("-- %s --\n", cases[i].name);

        check_u32("flags",
                flags, cases[i].flags);

        check_u32("LED mask",
                led_hex_test_read_ledr(),
                cases[i].flags);

        check_u32("HEX display",
                led_hex_test_read_hex30(),
                cases[i].hex);

        check_u32("audio 800Hz",
                s_audio_frequency_hz, 800U);

        check_u32("RECORDING",
                (uint32_t)system_state_get(),
                STATE_RECORDING);

        /* 경고 시작 후 200ms */
        update_outputs(220U);

        check_u32("at 200ms (silent)",
                s_audio_frequency_hz, 0U);

        /* 소음과 조도는 두 번째 경고음 발생 */
        if (cases[i].flags != EVENT_SHOCK) {

            update_outputs(320U);

            check_u32("second beep",
                    s_audio_frequency_hz, 800U);
        }

        /* 조도는 세 번째 경고음까지 검사 */
        if (cases[i].flags == EVENT_LIGHT) {

            update_outputs(520U);

            check_u32("second gap",
                    s_audio_frequency_hz, 0U);

            update_outputs(620U);

            check_u32("third beep",
                    s_audio_frequency_hz, 800U);

            update_outputs(820U);

            check_u32("light ends",
                    s_audio_frequency_hz, 0U);
        }

        /* 소음은 두 번째 경고음 이후 종료 */
        else if (cases[i].flags == EVENT_SOUND) {

            update_outputs(520U);

            check_u32("sound ends",
                    s_audio_frequency_hz, 0U);
        }
    }
}


/* 3. KEY0과 KEY1 동시 입력 검사 */
static void test_simultaneous_keys(void)
{
    SensorSample sample = {
        0U, 100U, 100U, 1000U
    };

    printf("\n=== Simultaneous KEY0+KEY1 ===\n");

    reset_modules();

    /* 충격 이벤트 발생 */
    (void)process_sample(&sample);

    sample.timestamp_ms = 10U;
    sample.vibration = 2100U;

    (void)process_sample(&sample);

    sample.timestamp_ms = 20U;
    (void)process_sample(&sample);

    check_u32("alarm playing",
            s_audio_frequency_hz, 800U);

    /* KEY0과 KEY1을 동시에 누른 상황 */
    key_irq_notify(
        KEY_EVENT_STOP | KEY_EVENT_CLEAR,
        50U
    );

    process_keys();

    /* KEY1 초기화가 최종적으로 적용 */
    check_u32("both keys mute",
            s_audio_frequency_hz, 0U);

    check_u32("KEY1 reset wins",
            (uint32_t)system_state_get(),
            STATE_MONITORING);

    check_u32("clear requested once",
            s_record_clear_count, 1U);

    check_u32("display cleared",
            led_hex_test_read_hex30(), 0x3F3FU);

    check_u32("keys consumed",
            key_take_events(),
            KEY_EVENT_NONE);
}


/* 전체 통합 테스트 실행 */
int main(void)
{
    printf(
        "=== Event & UI Integration Test (Mac Mock) ===\n"
    );

    test_multiple_event_and_keys();
    test_single_event_paths();
    test_simultaneous_keys();

    printf("\nTest summary: %u PASS, %u FAIL\n",
        s_pass, s_fail);

    return (s_fail == 0U) ? 0 : 1;
}
