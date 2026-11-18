
#include "led_hex.h"
#include "event_classifier.h"
#include "project_types.h"
#include "system_config.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * 실행 환경 선택
 *
 * 실제 DE1-SoC:
 *   -DLED_HEX_DE1_SOC_COMPUTER
 *
 * 기존 PC 단위 테스트:
 *   -DLED_HEX_HOST_TEST
 */
#if defined(LED_HEX_HOST_TEST) && \
    defined(LED_HEX_DE1_SOC_COMPUTER)

#error "Select only one LED/HEX environment."

#elif defined(LED_HEX_HOST_TEST)

static volatile uint32_t s_mock_ledr;
static volatile uint32_t s_mock_hex30;
static volatile uint32_t s_mock_hex54;

#define LEDR_REG  (&s_mock_ledr)
#define HEX30_REG (&s_mock_hex30)
#define HEX54_REG (&s_mock_hex54)

uint32_t led_hex_test_read_ledr(void)
{
    return s_mock_ledr;
}

uint32_t led_hex_test_read_hex30(void)
{
    return s_mock_hex30;
}

uint32_t led_hex_test_read_hex54(void)
{
    return s_mock_hex54;
}

#elif defined(LED_HEX_DE1_SOC_COMPUTER)

#define LEDR_REG \
    ((volatile uint32_t *)(uintptr_t)0xFF200000U)

#define HEX30_REG \
    ((volatile uint32_t *)(uintptr_t)0xFF200020U)

#define HEX54_REG \
    ((volatile uint32_t *)(uintptr_t)0xFF200030U)

#else

#error "Select LED/HEX execution environment."

#endif


/*
 * LEDR0~9 전체 점등 비트마스크
 *
 * 0x3FF = 0000001111111111 (10개 LED ON)
 */
#define ALL_LEDS_MASK 0x3FFU


/* 7-Segment 숫자 0~4 */
static const uint8_t s_seven_segment[5] = {
    0x3FU,  /* 0 */
    0x06U,  /* 1 */
    0x5BU,  /* 2 */
    0x4FU,  /* 3 */
    0x66U   /* 4 */
};


/*
 * 일반 이벤트 LED 상태
 */
static uint8_t s_active_flags;
static bool s_led_is_on;
static bool s_time_initialized;
static uint32_t s_last_toggle_ms;


/*
 * 초기 측정 완료 LED 알림 상태
 */
static bool s_ready_active;
static bool s_ready_complete;

static uint32_t s_ready_start_ms;
static uint32_t s_ready_phase;


/*
 * HEX1 HEX0에 두 자리 숫자 표시
 */
static void write_hex_code(uint8_t display_code)
{
    if (display_code > DISPLAY_MULTIPLE) {
        display_code = DISPLAY_NONE;
    }

    /* HEX1: 0, HEX0: 이벤트 코드 */
    *HEX30_REG =
        ((uint32_t)s_seven_segment[0] << 8) |
        (uint32_t)s_seven_segment[display_code];

    /* HEX4~5 OFF */
    *HEX54_REG = 0U;
}


/*
 * 일반 이벤트 LED 출력
 *
 * 충격 -> LED0
 * 소음 -> LED1
 * 조도 -> LED2
 */
static void write_leds(void)
{
    if (s_led_is_on) {
        *LEDR_REG = (uint32_t)s_active_flags;
    }
    else {
        *LEDR_REG = 0U;
    }
}


/*
 * LED / HEX 모듈 초기화
 */
void led_hex_init(void)
{
    /* 일반 이벤트 상태 초기화 */
    s_active_flags = EVENT_NONE;
    s_led_is_on = false;
    s_time_initialized = false;
    s_last_toggle_ms = 0U;

    /* 초기 측정 완료 알림 상태 초기화 */
    s_ready_active = false;
    s_ready_complete = false;

    s_ready_start_ms = 0U;
    s_ready_phase = 0U;

    /* 전체 LED OFF */
    write_leds();

    /* HEX 00 표시 */
    write_hex_code(DISPLAY_NONE);
}


/*
 * 초기 평균 측정 완료 LED 알림 시작
 *
 * LEDR0~9 전체를 3회 점멸
 *
 * 150ms ON
 * 150ms OFF
 *
 * 총 900ms
 */
void led_hex_ready_start(uint32_t timestamp_ms)
{
    /* 기존 이벤트 표시 초기화 */
    led_hex_clear();

    /* 알림 시작 상태 */
    s_ready_active = true;
    s_ready_complete = false;

    s_ready_start_ms = timestamp_ms;
    s_ready_phase = 0U;

    /* 첫 번째 점멸: 전체 LED 즉시 ON */
    *LEDR_REG = ALL_LEDS_MASK;
}


/*
 * 초기 측정 완료 LED 알림 종료 여부
 */
bool led_hex_ready_is_complete(void)
{
    return s_ready_complete;
}


/*
 * 이벤트 발생 시 LED / HEX 표시
 */
void led_hex_show(uint8_t event_flags)
{
    const uint8_t valid_mask =
        (uint8_t)(
            EVENT_SHOCK |
            EVENT_SOUND |
            EVENT_LIGHT
        );

    const uint8_t flags =
        (uint8_t)(event_flags & valid_mask);

    /*
     * 초기 완료 알림이 진행 중이면
     * 이벤트 표시가 LED 점멸을 방해하지 않도록 함
     */
    if (s_ready_active) {
        return;
    }

    /* 이벤트 없음 */
    if (flags == EVENT_NONE) {
        led_hex_clear();
        return;
    }

    /* 새 이벤트 또는 이벤트 종류 변경 */
    if (flags != s_active_flags) {

        s_active_flags = flags;

        /* 이벤트 LED 즉시 ON */
        s_led_is_on = true;

        /* 점멸 시간 다시 시작 */
        s_time_initialized = false;
    }

    /* 이벤트 LED 출력 */
    write_leds();

    /* 이벤트 종류를 HEX에 표시 */
    write_hex_code(event_classify(flags));
}


/*
 * LED 점멸 갱신
 *
 * 1. 초기 측정 완료 LED 전체 점멸
 * 2. 일반 이벤트 LED 점멸
 *
 * delay() 없이 시스템 시간 사용
 */
void led_hex_update(uint32_t timestamp_ms)
{
    /*
     * 1. 초기 측정 완료 LED 알림
     */
    if (s_ready_active) {

        const uint32_t elapsed =
            timestamp_ms - s_ready_start_ms;

        const uint32_t total_duration =
            2U *
            READY_LED_BLINK_INTERVAL_MS *
            READY_LED_BLINK_COUNT;

        /*
         * LED 전체 3회 점멸 완료
         */
        if (elapsed >= total_duration) {

            /* 전체 LED OFF */
            *LEDR_REG = 0U;

            s_ready_active = false;
            s_ready_complete = true;

            return;
        }

        /*
         * 현재 점멸 구간 계산
         *
         * phase 0: ON
         * phase 1: OFF
         * phase 2: ON
         * phase 3: OFF
         * phase 4: ON
         * phase 5: OFF
         */
        const uint32_t phase =
            elapsed / READY_LED_BLINK_INTERVAL_MS;

        /*
         * 점멸 구간이 변경된 경우에만
         * LED 레지스터 갱신
         */
        if (phase != s_ready_phase) {

            s_ready_phase = phase;

            if ((phase & 1U) == 0U) {
                *LEDR_REG = ALL_LEDS_MASK;
            }
            else {
                *LEDR_REG = 0U;
            }
        }

        return;
    }


    /*
     * 2. 일반 이벤트 LED 점멸
     */

    /* 이벤트가 없으면 갱신하지 않음 */
    if (s_active_flags == EVENT_NONE) {
        return;
    }

    /* 이벤트 발생 직후 첫 점멸 기준시간 설정 */
    if (!s_time_initialized) {

        s_last_toggle_ms = timestamp_ms;
        s_time_initialized = true;

        return;
    }

    /* 마지막 점멸 이후 경과시간 */
    const uint32_t elapsed =
        timestamp_ms - s_last_toggle_ms;

    /* 점멸 간격에 도달하지 않음 */
    if (elapsed < LED_BLINK_INTERVAL_MS) {
        return;
    }

    /*
     * 점멸 주기 계산
     *
     * 메인 루프 호출이 지연돼도
     * 실제 경과시간을 기준으로 동작
     */
    const uint32_t periods =
        elapsed / LED_BLINK_INTERVAL_MS;

    s_last_toggle_ms +=
        periods * LED_BLINK_INTERVAL_MS;

    /* 홀수 회 점멸 상태가 변경되면 반전 */
    if ((periods & 1U) != 0U) {

        s_led_is_on = !s_led_is_on;

        write_leds();
    }
}


/*
 * LED / HEX 이벤트 표시 초기화
 *
 * KEY1에서 호출
 *
 * 초기 측정 완료 알림이 진행 중일 경우
 * 해당 알림은 취소하지 않음
 */
void led_hex_clear(void)
{
    s_active_flags = EVENT_NONE;

    s_led_is_on = false;
    s_time_initialized = false;
    s_last_toggle_ms = 0U;

    /*
     * 준비 완료 알림 중이라면
     * 전체 LED 점멸 상태 유지
     */
    if (!s_ready_active) {
        write_leds();
    }

    /* HEX 00 표시 */
    write_hex_code(DISPLAY_NONE);
}
