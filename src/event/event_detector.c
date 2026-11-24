
#include "../../include/event_detector.h"
#include "../../include/system_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 센서별 이벤트 감지 상태 */
typedef struct
{
    uint32_t consecutive;
    uint32_t last_event_ms;
    bool latched;
    bool has_event;
} DetectionState;

static DetectionState s_shock;
static DetectionState s_sound;
static DetectionState s_light;

/* 초기 1초 평균값 */
static uint16_t s_vibration_base;
static uint16_t s_light_base;
static uint16_t s_sound_base;

/* 초기 평균 측정 완료 여부 */
static bool s_baseline_ready;

/* 소음 재감지 방지용 상태 */
static bool s_sound_quiet_started;
static uint32_t s_sound_quiet_start_ms;

static bool s_light_stable_started;
static uint32_t s_light_stable_start_ms;

/*
 * 두 측정값의 절댓값 차이
 */
static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    if (a >= b)
    {
        return (uint16_t)(a - b);
    }

    return (uint16_t)(b - a);
}

/*
 * 일반 이벤트 확정 함수
 *
 * - 연속 2회 감지
 * - 동일 이상 상태 중복 방지
 * - 1초 쿨다운
 *
 * 진동 / 조도에 사용
 */
static bool detect_confirmed(
    DetectionState *state,
    bool exceeded,
    uint32_t now_ms)
{
    if (!exceeded)
    {
        state->consecutive = 0U;
        state->latched = false;
        return false;
    }

    if (state->latched)
    {
        return false;
    }

    if (state->consecutive < EVENT_CONFIRM_SAMPLES)
    {
        state->consecutive++;
    }

    if (state->consecutive < EVENT_CONFIRM_SAMPLES)
    {
        return false;
    }

    state->latched = true;

    if (state->has_event &&
        (uint32_t)(now_ms - state->last_event_ms) < EVENT_COOLDOWN_MS)
    {
        return false;
    }

    state->has_event = true;
    state->last_event_ms = now_ms;

    return true;
}

/*
 * 소음 이벤트 확정
 *
 * 1. 초기 평균 대비 변화량 900 이상
 * 2. 연속 2회 이상 감지
 * 3. 이벤트 확정 후 재감지 잠금
 * 4. 변화량 400 미만 상태가 300ms 지속되면 잠금 해제
 * 5. 이벤트 간 1초 쿨다운 유지
 */
static bool detect_sound_confirmed(
    uint16_t sound_diff,
    uint32_t now_ms)
{
    /*
     * 이미 소음 이벤트가 확정된 상태
     */
    if (s_sound.latched)
    {

        if (sound_diff < SOUND_RESET_CHANGE_THRESHOLD)
        {

            if (!s_sound_quiet_started)
            {
                s_sound_quiet_started = true;
                s_sound_quiet_start_ms = now_ms;
            }
            else if (
                (uint32_t)(now_ms - s_sound_quiet_start_ms) >= SOUND_RESET_DURATION_MS)
            {
                /* 충분히 조용해지면 재감지 허용 */
                s_sound.latched = false;
                s_sound.consecutive = 0U;

                s_sound_quiet_started = false;
            }
        }
        else
        {
            /* 다시 시끄러워지면 조용한 시간 초기화 */
            s_sound_quiet_started = false;
        }

        return false;
    }

    /*
     * 조용한 구간을 거친 뒤에도
     * 기존 1초 쿨다운은 유지
     *
     * 쿨다운 중에 잘못 잠기지 않도록
     * 이벤트 판정보다 먼저 확인
     */
    if (s_sound.has_event &&
        (uint32_t)(now_ms - s_sound.last_event_ms) < EVENT_COOLDOWN_MS)
    {

        s_sound.consecutive = 0U;
        return false;
    }

    /*
     * 새 소음 이벤트 감지
     */
    return detect_confirmed(
        &s_sound,
        sound_diff >= SOUND_CHANGE_THRESHOLD,
        now_ms);
}

/*
 * 조도 이벤트 확정
 *
 * 1. 초기 평균 대비 변화량이 임계값 이상
 * 2. 연속 2회 이상 감지
 * 3. 이벤트 발생 후 재감지 잠금
 * 4. 변화량이 해제 임계값 미만인 상태
 *    300ms 지속되면 재감지 허용
 * 5. 이벤트 간 1초 쿨다운 유지
 */
static bool detect_light_confirmed(
    uint16_t light_diff,
    uint32_t now_ms)
{
    /* 이미 조도 이벤트가 확정된 상태 */
    if (s_light.latched)
    {
        if (light_diff < LIGHT_RESET_CHANGE_THRESHOLD)
        {
            if (!s_light_stable_started)
            {
                s_light_stable_started = true;
                s_light_stable_start_ms = now_ms;
            }
            else if (
                (uint32_t)(now_ms - s_light_stable_start_ms) >= LIGHT_RESET_DURATION_MS)
            {
                s_light.latched = false;
                s_light.consecutive = 0U;

                s_light_stable_started = false;
            }
        }
        else
        {
            s_light_stable_started = false;
        }

        return false;
    }

    /* 재감지 쿨다운 */
    if (s_light.has_event &&
        (uint32_t)(now_ms - s_light.last_event_ms) < EVENT_COOLDOWN_MS)
    {
        s_light.consecutive = 0U;
        return false;
    }

    return detect_confirmed(
        &s_light,
        light_diff >= LIGHT_CHANGE_THRESHOLD,
        now_ms);
}

/*
 * 이벤트 감지 모듈 초기화
 */
void event_detector_init(void)
{
    s_shock = (DetectionState){0};
    s_sound = (DetectionState){0};
    s_light = (DetectionState){0};

    s_vibration_base = 0U;
    s_light_base = 0U;
    s_sound_base = 0U;

    s_baseline_ready = false;

    s_sound_quiet_started = false;
    s_sound_quiet_start_ms = 0U;

    s_light_stable_started = false;
    s_light_stable_start_ms = 0U;
}

/*
 * 초기 1초 평균값 설정
 */
void event_detector_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base)
{
    s_vibration_base = vibration_base;
    s_light_base = light_base;
    s_sound_base = sound_base;

    /* 이전 이벤트 상태 초기화 */
    s_shock = (DetectionState){0};
    s_sound = (DetectionState){0};
    s_light = (DetectionState){0};

    s_sound_quiet_started = false;
    s_sound_quiet_start_ms = 0U;

    s_light_stable_started = false;
    s_light_stable_start_ms = 0U;

    s_baseline_ready = true;
}

/*
 * 센서 데이터 기반 이벤트 감지
 */


/*
 * 센서 데이터 기반 이벤트 감지
 *
 * 복합 이벤트 판정:
 * 새로운 센서 이벤트가 확정된 순간,
 * 다른 센서도 최초 감지 임계값 이상이면
 * 복합 이벤트로 처리한다.
 */
uint8_t event_detect(const SensorSample *sample)
{
    uint8_t flags = EVENT_NONE;

    uint16_t vibration_diff;
    uint16_t light_diff;
    uint16_t sound_diff;

    if (sample == NULL)
    {
        return EVENT_NONE;
    }

    /* 초기 평균값 설정 전에는 감지하지 않음 */
    if (!s_baseline_ready)
    {
        return EVENT_NONE;
    }

    /* 1. 초기 평균값 대비 센서 변화량 계산 */
    vibration_diff = abs_diff_u16(
        sample->vibration,
        s_vibration_base);

    sound_diff = abs_diff_u16(
        sample->sound_level,
        s_sound_base);

    light_diff = abs_diff_u16(
        sample->light,
        s_light_base);

    /* 2. 기존 센서별 이벤트 확정 로직 유지 */

    if (detect_confirmed(
            &s_shock,
            vibration_diff >= SHOCK_CHANGE_THRESHOLD,
            sample->timestamp_ms))
    {
        flags |= EVENT_SHOCK;
    }

    if (detect_sound_confirmed(
            sound_diff,
            sample->timestamp_ms))
    {
        flags |= EVENT_SOUND;
    }

    if (detect_light_confirmed(
            light_diff,
            sample->timestamp_ms))
    {
        flags |= EVENT_LIGHT;
    }

    /*
     * 3. 새로운 이벤트가 없는 경우
     * 기존 이벤트를 반복 반환하지 않음
     */
    if (flags == EVENT_NONE)
    {
        return EVENT_NONE;
    }

    /*
     * 4. 복합 이벤트 판정
     *
     * 새로운 이벤트가 확정된 순간
     * 다른 센서의 래치 상태와
     * 최초 감지 임계값을 함께 확인한다.
     */

    /* 진동: 변화량 400 이상 */
    if (s_shock.latched &&
        vibration_diff >= SHOCK_CHANGE_THRESHOLD)
    {
        flags |= EVENT_SHOCK;
    }

    /* 소음: 변화량 900 이상 */
    if (s_sound.latched &&
        sound_diff >= SOUND_CHANGE_THRESHOLD)
    {
        flags |= EVENT_SOUND;
    }

    /* 조도: 변화량 150 이상 */
    if (s_light.latched &&
        light_diff >= LIGHT_CHANGE_THRESHOLD)
    {
        flags |= EVENT_LIGHT;
    }

    return flags;
}
