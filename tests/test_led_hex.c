
#include <stdio.h>               // printf 사용
#include <stdint.h>              // uint8_t, uint32_t 사용
#include "led_hex.h"           // LED/HEX 출력 모듈
#include "event_detector.h"    // 1주차 첫 모듈: 이벤트 감지
#include "event_classifier.h"  // 1주차 두 번째 모듈: 이벤트 분류
#include "project_types.h"     // SensorSample 및 이벤트 상수

#ifndef LED_HEX_HOST_TEST
#error "This PC test must be compiled with -DLED_HEX_HOST_TEST"
#endif

// led_hex.c가 호스트 테스트 모드에서만 제공하는 모의 레지스터 조회 함수.
uint32_t led_hex_test_read_ledr(void);
uint32_t led_hex_test_read_hex30(void);
uint32_t led_hex_test_read_hex54(void);

static int s_pass = 0;            // 통과한 검사 수
static int s_fail = 0;            // 실패한 검사 수

/* 실제 출력(가상 레지스터 값)과 예상 값을 비교한다. */
static void check(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        printf("[PASS] %-27s = 0x%08X\n", name, (unsigned)actual);
        s_pass++;
    } else {
        printf("[FAIL] %-27s expected=0x%08X actual=0x%08X\n",
            name, (unsigned)expected, (unsigned)actual);
        s_fail++;
    }
}

/* 정상, 충격, 소음, 조도, 복합 이벤트의 LED와 HEX 표현 검사. */
static void test_display_cases(void)
{
    static const struct {
        uint8_t flags;            // event_detect()의 반환값
        uint32_t expected_led;    // LED0~2 비트 패턴
        uint32_t expected_hex;    // HEX1='0', HEX0=0~4
    } cases[] = {
        { EVENT_NONE,                                0U, 0x3F3FU },
        { EVENT_SHOCK,                               1U, 0x3F06U },
        { EVENT_SOUND,                               2U, 0x3F5BU },
        { EVENT_LIGHT,                               4U, 0x3F4FU },
        { EVENT_SHOCK | EVENT_SOUND,                 3U, 0x3F66U },
        { EVENT_SHOCK | EVENT_LIGHT,                 5U, 0x3F66U },
        { EVENT_SOUND | EVENT_LIGHT,                 6U, 0x3F66U },
        { EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT,   7U, 0x3F66U },
        { 0x80U,                                     0U, 0x3F3FU }, // 미정의 비트 무시
        { 0x81U,                                     1U, 0x3F06U }  // 미정의 비트 + 충격
    };

    for (unsigned i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char label[48];
        led_hex_init();            // 매 검사마다 이전 상태를 제거
        led_hex_show(cases[i].flags);
        (void)snprintf(label, sizeof(label), "flags %u LED", (unsigned)cases[i].flags);
        check(label, led_hex_test_read_ledr(), cases[i].expected_led);
        (void)snprintf(label, sizeof(label), "flags %u HEX", (unsigned)cases[i].flags);
        check(label, led_hex_test_read_hex30(), cases[i].expected_hex);
        check("HEX5/4 blank", led_hex_test_read_hex54(), 0U);
    }
}

/* 250ms마다 LED만 깜박이고 HEX는 그대로 유지되는지 검사. */
static void test_blink_and_clear(void)
{
    led_hex_init();
    check("init LED OFF", led_hex_test_read_ledr(), 0U);
    check("init HEX 00", led_hex_test_read_hex30(), 0x3F3FU);

    led_hex_show(EVENT_SHOCK);
    check("show: LED0 ON", led_hex_test_read_ledr(), 1U);

    led_hex_update(1000U);        // 첫 호출: 시작 시각 설정
    led_hex_update(1249U);        // 249ms: 변화 없음
    check("249ms LED ON", led_hex_test_read_ledr(), 1U);
    led_hex_update(1250U);        // 250ms: LED OFF
    check("250ms LED OFF", led_hex_test_read_ledr(), 0U);
    check("blink keeps HEX 01", led_hex_test_read_hex30(), 0x3F06U);
    led_hex_update(1500U);        // 500ms: LED 다시 ON
    check("500ms LED ON", led_hex_test_read_ledr(), 1U);
    led_hex_update(2250U);        // 1250ms: 3번 바뀌어 OFF
    check("1250ms LED OFF", led_hex_test_read_ledr(), 0U);

    led_hex_show(EVENT_SOUND);    // 이벤트 종류가 바뀌면 LED1을 즉시 ON
    check("new event: LED1 ON", led_hex_test_read_ledr(), 2U);
    check("new event: HEX 02", led_hex_test_read_hex30(), 0x3F5BU);

    led_hex_clear();              // 경고 출력만 초기화
    check("clear LED OFF", led_hex_test_read_ledr(), 0U);
    check("clear HEX 00", led_hex_test_read_hex30(), 0x3F3FU);
}

/* 가상 센서값 -> 감지 -> 분류 -> LED/HEX 까지 연결 테스트. */
static void test_full_event_flow(void)
{
    event_detector_init();
    led_hex_init();

    SensorSample sample = { 0U, 100U, 100U, 1000U }; // 정상/초기 조도 기준
    uint8_t flags = event_detect(&sample);
    check("normal detect", flags, EVENT_NONE);

    sample.timestamp_ms = 10U;  // 첫 번째 충격+소음 샘플
    sample.vibration = 2100U;   // 임계값 2000 초과
    sample.sound_level = 1200U; // 임계값 1000 초과
    flags = event_detect(&sample);
    check("first abnormal detect", flags, EVENT_NONE); // 연속 2회 조건

    sample.timestamp_ms = 20U;  // 두 번째 연속 초과 샘플
    flags = event_detect(&sample);
    check("double event flags", flags, EVENT_SHOCK | EVENT_SOUND);
    check("classifier HEX code", event_classify(flags), 4U);

    led_hex_show(flags);          // 실제 통합 때처럼 출력 API 호출
    led_hex_update(sample.timestamp_ms);
    check("combined LED0+1", led_hex_test_read_ledr(), 3U);
    check("combined HEX 04", led_hex_test_read_hex30(), 0x3F66U);
}

int main(void)
{
    printf("=== LED/HEX Output Test ===\n");
    test_display_cases();
    test_blink_and_clear();

    printf("\n=== Detector -> Classifier -> LED/HEX Integration ===\n");
    test_full_event_flow();

    printf("\nTest summary: %d PASS, %d FAIL\n", s_pass, s_fail);
    return (s_fail == 0) ? 0 : 1;   // 실패가 있으면 종료 코드 1
}
