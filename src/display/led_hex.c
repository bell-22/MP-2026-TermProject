
#include "led_hex.h"          // 이 모듈의 공개 함수 선언
#include "event_classifier.h" // event_classify(): 이벤트 플래그 -> HEX 코드
#include "project_types.h"   // EVENT_SHOCK, EVENT_SOUND, EVENT_LIGHT
#include "system_config.h"   // LED_BLINK_INTERVAL_MS, DISPLAY_* 설정값
#include <stdbool.h>          // bool, true, false
#include <stdint.h>           // uint8_t, uint32_t, uintptr_t

/*
 * 실행 환경 선택 (둘 중 하나를 컴파일할 때 지정한다.)
 *
 * -DLED_HEX_HOST_TEST        : Mac/PC에서 가상 레지스터를 사용한다.
 * -DLED_HEX_DE1_SOC_COMPUTER : 표준 DE1-SoC Computer(Nios II)의 레지스터 사용.
 *
 * 교수님이 제공한 Nios II 하드웨어 구성이 표준 DE1-SoC Computer와
 * 다르다면 실제 주소는 해당 BSP/Platform Designer 설정으로 확인해야 한다.
 * 단순히 Mac에서 -DLED_HEX_DE1_SOC_COMPUTER를 쓰면 안 된다.
 */
#if defined(LED_HEX_HOST_TEST) && defined(LED_HEX_DE1_SOC_COMPUTER)
#error "Select only one LED/HEX execution environment."
#elif defined(LED_HEX_HOST_TEST)
// Mac에서는 하드웨어 대신 아래 변수에 출력을 저장하여 테스트한다.
static volatile uint32_t s_mock_ledr;
static volatile uint32_t s_mock_hex30;
static volatile uint32_t s_mock_hex54;
#define LEDR_REG      (&s_mock_ledr)
#define HEX30_REG     (&s_mock_hex30)
#define HEX54_REG     (&s_mock_hex54)

// 단위 테스트에서만 사용하는 레지스터 조회 함수 (공개 API 아님).
uint32_t led_hex_test_read_ledr(void)  { return s_mock_ledr; }
uint32_t led_hex_test_read_hex30(void) { return s_mock_hex30; }
uint32_t led_hex_test_read_hex54(void) { return s_mock_hex54; }

#elif defined(LED_HEX_DE1_SOC_COMPUTER)
// Intel DE1-SoC Computer / Nios II 참조 하드웨어의 메모리 맵.
#define LEDR_REG  ((volatile uint32_t *)(uintptr_t)0xFF200000U)
#define HEX30_REG ((volatile uint32_t *)(uintptr_t)0xFF200020U)
#define HEX54_REG ((volatile uint32_t *)(uintptr_t)0xFF200030U)
#else
#error "Use -DLED_HEX_HOST_TEST (Mac) or -DLED_HEX_DE1_SOC_COMPUTER (Nios II)."
#endif

// 각 7-Segment 숫자(0~4)의 a~g 점등 비트: 1이면 해당 구간 ON.
static const uint8_t s_seven_segment[5] = {
    0x3FU,  // 0: a,b,c,d,e,f
    0x06U,  // 1: b,c
    0x5BU,  // 2: a,b,d,e,g
    0x4FU,  // 3: a,b,c,d,g
    0x66U   // 4: b,c,f,g
};

static uint8_t s_active_flags;     // 현재 표시 중인 이벤트 비트 조합
static bool s_led_is_on;           // 현재 점멸 주기에서 LED가 켜진 상태인가?
static bool s_time_initialized;    // led_hex_update()로 점멸 기준시각을 받았는가?
static uint32_t s_last_toggle_ms;  // 점멸 상태가 마지막으로 바뀐 기준 시각

/* 두 자리 HEX에 '0'과 이벤트 표시 코드(0~4)를 표시한다. */
static void write_hex_code(uint8_t display_code)
{
    if (display_code > DISPLAY_MULTIPLE) { // 유효하지 않은 코드 보호
        display_code = DISPLAY_NONE;
    }

    // HEX1(왼쪽 한 자리)에 '0', HEX0(오른쪽 한 자리)에 0~4 표시.
    // HEX3, HEX2는 0으로 두어 끈다. (각 자리는 8비트 간격)
    *HEX30_REG = ((uint32_t)s_seven_segment[0] << 8)
            | (uint32_t)s_seven_segment[display_code];

    // 이번 1주차 출력에서는 HEX4~HEX5도 비워둔다.
    *HEX54_REG = 0U;
}

/* 현재 점멸 상태와 이벤트 종류에 맞게 LED0~2를 출력한다. */
static void write_leds(void)
{
    // EVENT_SHOCK=bit0 -> LED0, SOUND=bit1 -> LED1, LIGHT=bit2 -> LED2.
    // 따라서 이벤트 플래그를 그대로 LED 비트마스크로 사용할 수 있다.
    *LEDR_REG = s_led_is_on ? (uint32_t)s_active_flags : 0U;
}

/* LED 출력 OFF, HEX는 정상 코드 '00'으로 초기화한다. */
void led_hex_init(void)
{
    s_active_flags = EVENT_NONE;      // 현재 출력 중인 이벤트 없음
    s_led_is_on = false;              // LED 점등 상태 해제
    s_time_initialized = false;       // 점멸 기준시각 초기화
    s_last_toggle_ms = 0U;
    write_leds();                     // LED0~9 OFF
    write_hex_code(DISPLAY_NONE);     // HEX1 HEX0: 00
}

/* 새 이벤트가 발생하면 LED 조합과 HEX 표시 내용을 갱신한다. */
void led_hex_show(uint8_t event_flags)
{
    const uint8_t valid_mask =
        (uint8_t)(EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT);
    const uint8_t flags = (uint8_t)(event_flags & valid_mask);

    if (flags == EVENT_NONE) {        // 이벤트가 없으면 정상 출력으로 복귀
        led_hex_clear();
        return;
    }

    if (flags != s_active_flags) {    // 새 이벤트(또는 종류 변경)일 때
        s_active_flags = flags;
        s_led_is_on = true;           // 즉시 해당 LED 켜기
        s_time_initialized = false;   // 다음 update 호출에서 점멸 시작시각 설정
    }

    write_leds();                     // 해당하는 LED를 동시에 점등
    write_hex_code(event_classify(flags)); // 비트 플래그를 0~4로 변환해 HEX 표시
}

/* 시스템 시간에 맞춰 LED를 점멸시킨다. delay()를 사용하지 않는다. */
void led_hex_update(uint32_t timestamp_ms)
{
    if (s_active_flags == EVENT_NONE) {  // 표시 중인 이벤트가 없으면 무동작
        return;
    }

    if (!s_time_initialized) {         // 이벤트 발생 직후 첫 update
        s_last_toggle_ms = timestamp_ms;
        s_time_initialized = true;
        return;
    }

    // unsigned 연산으로 시스템 시간의 uint32_t 래핑도 고려한다.
    const uint32_t elapsed = timestamp_ms - s_last_toggle_ms;
    if (elapsed < LED_BLINK_INTERVAL_MS) { // 아직 점멸 간격 미경과
        return;
    }

    // 호출이 늦어졌다면 경과한 점멸 간격 개수를 한 번에 반영한다.
    const uint32_t periods = elapsed / LED_BLINK_INTERVAL_MS;
    s_last_toggle_ms += periods * LED_BLINK_INTERVAL_MS;

    if ((periods & 1U) != 0U) {        // 홀수 번 바뀌면 LED 상태 반전
        s_led_is_on = !s_led_is_on;
        write_leds();                 // HEX는 유지, LED만 갱신
    }
}

/* KEY1 기록 초기화 등에서 호출: 출력만 정상 상태로 돌린다. */
void led_hex_clear(void)
{
    led_hex_init();                   // 버퍼/이벤트 기록 데이터에는 접근하지 않음
}
