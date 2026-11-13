
#include <stdio.h>               // printf 함수 사용
#include "event_classifier.h"  // 이번에 작성한 이벤트 분류 모듈
#include "event_detector.h"    // 앞 단계에 작성한 이벤트 감지 모듈
#include "system_config.h"     // DISPLAY_* 출력 코드

static int g_pass_count = 0;    // 성공한 검사 개수
static int g_fail_count = 0;    // 실패한 검사 개수

/* 반환 코드와 예상 코드를 비교하고 PASS/FAIL을 출력한다. */
static void check_code(const char *name, uint8_t actual, uint8_t expected)
{
    if (actual == expected) {
        printf("[PASS] %-28s expected=%u, actual=%u\n",
               name, (unsigned)expected, (unsigned)actual);
        g_pass_count++;
    } else {
        printf("[FAIL] %-28s expected=%u, actual=%u\n",
               name, (unsigned)expected, (unsigned)actual);
        g_fail_count++;
    }
}

/* 분류 함수만 직접 테스트한다. (센서 코드 필요 없음) */
static void test_classifier_only(void)
{
    // 입력 비트 플래그 0~7과 올바른 HEX 표시 코드 0~4 비교
    check_code("NONE (0)",         event_classify(EVENT_NONE), DISPLAY_NONE);
    check_code("SHOCK (1)",        event_classify(EVENT_SHOCK), DISPLAY_SHOCK);
    check_code("SOUND (2)",        event_classify(EVENT_SOUND), DISPLAY_SOUND);
    check_code("SHOCK+SOUND (3)",  event_classify(EVENT_SHOCK | EVENT_SOUND), DISPLAY_MULTIPLE);
    check_code("LIGHT (4)",        event_classify(EVENT_LIGHT), DISPLAY_LIGHT);
    check_code("SHOCK+LIGHT (5)",  event_classify(EVENT_SHOCK | EVENT_LIGHT), DISPLAY_MULTIPLE);
    check_code("SOUND+LIGHT (6)",  event_classify(EVENT_SOUND | EVENT_LIGHT), DISPLAY_MULTIPLE);
    check_code("ALL (7)",          event_classify(EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT), DISPLAY_MULTIPLE);

    // 정의되지 않은 비트(0x80 등)는 무시해야 한다.
    check_code("unknown bit only",  event_classify(0x80U), DISPLAY_NONE);
    check_code("unknown + SHOCK",   event_classify(0x81U), DISPLAY_SHOCK);
    check_code("unknown + SOUND",   event_classify(0x82U), DISPLAY_SOUND);
    check_code("unknown + LIGHT",   event_classify(0x84U), DISPLAY_LIGHT);
    check_code("unknown + MULTIPLE",event_classify(0x83U), DISPLAY_MULTIPLE);
    check_code("all bits set",      event_classify(0xFFU), DISPLAY_MULTIPLE);
}

/* Mock SensorSample -> event_detect() -> event_classify() 연결 검사 */
static void test_detect_to_classify(const char *name,
                                    uint16_t vibration,
                                    uint16_t sound_level,
                                    uint16_t light,
                                    uint8_t expected_flags,
                                    uint8_t expected_display)
{
    // 각 사례는 완전히 독립적으로 테스트하기 위해 상태 초기화
    event_detector_init();

    // 정상 센서값으로 조도 기준점과 초기 상태를 설정한다.
    SensorSample sample = {0U, 100U, 100U, 1000U};
    (void)event_detect(&sample);

    // 첫 번째 이상 샘플. 확인 조건이 2회라면 아직 이벤트가 없다.
    sample.timestamp_ms = 10U;
    sample.vibration = vibration;
    sample.sound_level = sound_level;
    sample.light = light;
    (void)event_detect(&sample);

    // 두 번째 이상 샘플. 감지 결과를 분류 함수에 전달한다.
    sample.timestamp_ms = 20U;
    const uint8_t flags = event_detect(&sample);
    const uint8_t display_code = event_classify(flags);

    // 감지한 플래그와 HEX 표시 코드가 각각 올바른지 확인한다.
    char label[80];
    (void)snprintf(label, sizeof(label), "%s flags", name);
    check_code(label, flags, expected_flags);
    (void)snprintf(label, sizeof(label), "%s HEX code", name);
    check_code(label, display_code, expected_display);
}

int main(void)
{
    printf("=== Event Classifier Unit Test ===\n");
    test_classifier_only();

    printf("\n=== Detector + Classifier Mock Integration Test ===\n");
    // 실제 센서나 보드 없이 가상 센서값으로 두 모듈을 함께 확인한다.
    test_detect_to_classify("shock", 2100U, 100U, 1000U,
                            EVENT_SHOCK, DISPLAY_SHOCK);
    test_detect_to_classify("sound", 100U, 1200U, 1000U,
                            EVENT_SOUND, DISPLAY_SOUND);
    test_detect_to_classify("light", 100U, 100U, 1700U,
                            EVENT_LIGHT, DISPLAY_LIGHT);
    test_detect_to_classify("shock+sound", 2100U, 1200U, 1000U,
                            EVENT_SHOCK | EVENT_SOUND, DISPLAY_MULTIPLE);

    printf("\nTest summary: %d PASS, %d FAIL\n", g_pass_count, g_fail_count);
    return (g_fail_count == 0) ? 0 : 1; // 실패 시 종료 코드 1
}
