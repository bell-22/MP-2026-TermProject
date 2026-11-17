
#include <stdio.h>             // printf 사용
#include "event_detector.h"  // 이벤트 감지 함수 선언

static int failures = 0;      // 실패한 테스트 개수

/* 예상 결과와 실제 이벤트 감지 결과를 비교 */
static void check(const char *name,
                uint8_t actual,
                uint8_t expected)
{
    if (actual == expected) {
        printf("[PASS] %s: %u\n",
            name, (unsigned)actual);
    } else {
        printf("[FAIL] %s: expected %u, got %u\n",
            name,
            (unsigned)expected,
            (unsigned)actual);
        failures++;
    }
}

int main(void)
{
    // 가상 센서값 생성
    // 시간 0ms, 진동 100, 소리 100, 조도 1000
    SensorSample sample = {0U, 100U, 100U, 1000U};

    // 이벤트 감지 모듈 초기화
    event_detector_init();

    /* 1. 정상 상태 테스트 */
    check("normal",
        event_detect(&sample),
        EVENT_NONE);

    /* 2. 충격과 소음의 첫 번째 초과 샘플 */
    sample.timestamp_ms = 10U;
    sample.vibration = 2100U;
    sample.sound_level = 1200U;

    // 아직 1회 초과이므로 이벤트 미발생
    check("first abnormal sample",
        event_detect(&sample),
        EVENT_NONE);

    /* 3. 두 번째 연속 초과: 복합 이벤트 */
    sample.timestamp_ms = 20U;

    check("shock + sound",
        event_detect(&sample),
        EVENT_SHOCK | EVENT_SOUND);

    /* 4. 이상 상태 유지 시 중복 감지 방지 */
    sample.timestamp_ms = 30U;

    check("no duplicate",
        event_detect(&sample),
        EVENT_NONE);

    /* 5. 정상 범위 복귀 */
    sample.timestamp_ms = 40U;
    sample.vibration = 100U;
    sample.sound_level = 100U;

    check("back to normal",
        event_detect(&sample),
        EVENT_NONE);

    /* 6. 조도값 1000에서 1700으로 변화 */
    sample.timestamp_ms = 50U;
    sample.light = 1700U;

    // 첫 변화 샘플은 아직 미확정
    check("first light change",
        event_detect(&sample),
        EVENT_NONE);

    /* 7. 조도 변화가 2회 연속 발생 */
    sample.timestamp_ms = 60U;

    check("light event",
        event_detect(&sample),
        EVENT_LIGHT);

    /* 8. 재감지 대기시간 테스트 */
    sample.timestamp_ms = 70U;
    sample.vibration = 100U;

    check("reset shock condition",
        event_detect(&sample),
        EVENT_NONE);

    // 다시 충격 발생
    sample.timestamp_ms = 100U;
    sample.vibration = 2100U;

    check("shock again first",
        event_detect(&sample),
        EVENT_NONE);

    sample.timestamp_ms = 110U;

    // 이전 충격 감지 후 1초가 지나지 않았음
    check("shock cooldown",
        event_detect(&sample),
        EVENT_NONE);

    /* 9. 대기시간 이후 충격 재감지 */
    sample.timestamp_ms = 120U;
    sample.vibration = 100U;

    check("reset after cooldown",
        event_detect(&sample),
        EVENT_NONE);

    sample.timestamp_ms = 1030U;
    sample.vibration = 2100U;

    check("shock later first",
        event_detect(&sample),
        EVENT_NONE);

    sample.timestamp_ms = 1040U;

    check("shock later second",
        event_detect(&sample),
        EVENT_SHOCK);

    /* 10. NULL 입력 처리 테스트 */
    check("NULL input",
        event_detect(NULL),
        EVENT_NONE);

    /* 테스트 최종 결과 출력 */
    if (failures == 0) {
        printf("All event detector tests passed.\n");
        return 0;
    }

    printf("%d test(s) failed.\n", failures);
    return 1;
}
