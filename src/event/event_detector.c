
#include "../../include/event_detector.h"    // 공통 센서 구조체 및 이벤트 종류
#include "../../include/system_config.h"    // 센서 임계값 및 감지 설정
#include <stddef.h>           // NULL 사용

/* 센서별 이벤트 감지 상태 */
typedef struct {
    uint32_t consecutive;      // 연속으로 임계값을 초과한 횟수
    uint32_t last_event_ms;    // 마지막 이벤트 발생 시간
    bool latched;              // 동일한 이상 상태의 중복 감지 방지
    bool has_event;            // 이전 이벤트 발생 여부
} DetectionState;

static DetectionState s_shock;  // 진동센서 감지 상태
static DetectionState s_sound;  // 마이크 감지 상태
static DetectionState s_light;  // 조도센서 감지 상태

static bool s_light_ready;      // 최초 조도값 수집 여부
static uint16_t s_light_base;  // 조도 변화 비교 기준값


/* 이벤트 확정 및 중복 감지 방지 함수 */
static bool detect_confirmed(
    DetectionState *state,
    bool exceeded,
    uint32_t now_ms)
{
    if (!exceeded) {
        // 임계값 이하이면 연속 감지 횟수 초기화
        state->consecutive = 0U;

        // 다음 이벤트를 감지할 수 있도록 준비
        state->latched = false;

        return false;
    }

    // 이미 확인한 이상 상태가 계속되면 중복 감지하지 않음
    if (state->latched) {
        return false;
    }

    // 임계값을 연속으로 초과한 횟수 증가
    if (state->consecutive < EVENT_CONFIRM_SAMPLES) {
        state->consecutive++;
    }

    // 설정된 연속 측정 횟수에 도달했는지 확인
    if (state->consecutive < EVENT_CONFIRM_SAMPLES) {
        return false;
    }

    // 이번 이상 상태는 한 번만 감지하도록 설정
    state->latched = true;

    // 이전 이벤트 발생 후 재감지 대기시간 확인
    if (state->has_event &&
        (uint32_t)(now_ms - state->last_event_ms)
            < EVENT_COOLDOWN_MS) {
        return false;
    }

    // 이벤트 발생 정보 갱신
    state->has_event = true;
    state->last_event_ms = now_ms;

    return true;
}


/* 이벤트 감지 모듈 초기화 */
void event_detector_init(void)
{
    // 충격 이벤트 감지 상태 초기화
    s_shock = (DetectionState){0};

    // 소음 이벤트 감지 상태 초기화
    s_sound = (DetectionState){0};

    // 조도 이벤트 감지 상태 초기화
    s_light = (DetectionState){0};

    // 첫 번째 조도값을 기준값으로 설정하기 위한 준비
    s_light_ready = false;
    s_light_base = 0U;
}


/* 센서 데이터를 입력받아 이벤트 종류 판단 */
uint8_t event_detect(const SensorSample *sample)
{
    // 이벤트가 없으면 기본적으로 EVENT_NONE 반환
    uint8_t flags = EVENT_NONE;

    // 조도 변화량 및 판정 상태 변수
    uint16_t light_diff;
    bool light_exceeded;
    bool light_was_latched;

    // 잘못된 센서 데이터 주소인지 검사
    if (sample == NULL) {
        return EVENT_NONE;
    }

    /* 1. 충격 이벤트 감지 */

    // 진동 측정값이 SHOCK_THRESHOLD를 초과하는지 확인
    if (detect_confirmed(
            &s_shock,
            sample->vibration > SHOCK_THRESHOLD,
            sample->timestamp_ms)) {

        // 충격 이벤트 비트 추가
        flags |= EVENT_SHOCK;
    }


    /* 2. 소음 이벤트 감지 */

    // 음량 특징값이 SOUND_THRESHOLD를 초과하는지 확인
    if (detect_confirmed(
            &s_sound,
            sample->sound_level > SOUND_THRESHOLD,
            sample->timestamp_ms)) {

        // 소음 이벤트 비트 추가
        flags |= EVENT_SOUND;
    }


    /* 3. 조도 변화 이벤트 감지 */

    // 처음 측정한 조도값은 비교 기준값으로 저장
    if (!s_light_ready) {
        s_light_base = sample->light;
        s_light_ready = true;

        // 충격과 소음 감지 결과는 그대로 반환
        return flags;
    }

    // 이전 기준값과 현재 조도값의 절댓값 차이 계산
    if (sample->light >= s_light_base) {
        light_diff = sample->light - s_light_base;
    } else {
        light_diff = s_light_base - sample->light;
    }

    // 밝아지거나 어두워지는 변화 모두 감지
    light_exceeded =
        (light_diff > LIGHT_CHANGE_THRESHOLD);

    // 감지 확정 전 상태를 저장
    light_was_latched = s_light.latched;

    // 조도 변화가 연속으로 발생했는지 확인
    if (detect_confirmed(
            &s_light,
            light_exceeded,
            sample->timestamp_ms)) {

        // 조도 이벤트 비트 추가
        flags |= EVENT_LIGHT;
    }

    // 조도 변화가 작으면 최근 값을 기준값으로 갱신
    if (!light_exceeded) {
        s_light_base = sample->light;
    }

    // 조도 변화가 확정되면 새로운 밝기를 기준으로 설정
    else if (!light_was_latched && s_light.latched) {
        s_light_base = sample->light;
    }

    // 충격, 소음, 조도 이벤트 비트 조합 반환
    return flags;
}
