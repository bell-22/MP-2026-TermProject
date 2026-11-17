
#include "risk_level.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 최초 조도 기준값 설정 여부 */
static bool s_light_ready;

/* 조도 변화 비교 기준값 */
static uint16_t s_light_base;

/* 이전 샘플 시간 관리 */
static bool s_have_last_sample;
static uint32_t s_last_sample_ms;

/* 이상 상태 지속 여부 */
static bool s_tracking;

/* 이상 상태가 처음 시작된 시간 */
static uint32_t s_abnormal_start_ms;

/* 연속 이상 측정 횟수 */
static uint32_t s_abnormal_samples;

/* 현재 위험 단계 */
static RiskLevel s_current;


/* 두 센서값의 절댓값 차이 계산 */
static uint32_t abs_diff_u16(uint16_t a, uint16_t b)
{
    if (a >= b) {
        return (uint32_t)(a - b);
    }

    return (uint32_t)(b - a);
}


/* 위험 단계 판정 모듈 초기화 */
void risk_level_init(void)
{
    s_light_ready = false;
    s_light_base = 0U;

    s_have_last_sample = false;
    s_last_sample_ms = 0U;

    s_tracking = false;
    s_abnormal_start_ms = 0U;
    s_abnormal_samples = 0U;

    s_current = RISK_NORMAL;
}


/* 센서 데이터 기반 위험 단계 판정 */
RiskLevel risk_level_update(
    const SensorSample *sample
)
{
    bool shock;
    bool sound;
    bool light;

    unsigned active_count;
    uint32_t light_diff;

    /* 잘못된 입력이면 이전 상태 유지 */
    if (sample == NULL) {
        return s_current;
    }

    /*
     * 1. 샘플 시간 간격 확인
     *
     * 센서 샘플이 크게 누락되면
     * 연속 이상 상태 판정을 다시 시작한다.
     */
    if (s_have_last_sample &&
        (uint32_t)(
            sample->timestamp_ms - s_last_sample_ms
        ) > RISK_MAX_SAMPLE_GAP_MS) {

        s_tracking = false;
        s_abnormal_samples = 0U;
    }

    s_have_last_sample = true;
    s_last_sample_ms = sample->timestamp_ms;


    /*
     * 2. 최초 조도 기준값 설정
     */
    if (!s_light_ready) {

        s_light_base = sample->light;
        s_light_ready = true;
    }

    light_diff = abs_diff_u16(
        sample->light,
        s_light_base
    );


    /*
     * 3. 센서별 임계값 초과 여부
     */
    shock =
        sample->vibration > SHOCK_THRESHOLD;

    sound =
        sample->sound_level > SOUND_THRESHOLD;

    light =
        light_diff > LIGHT_CHANGE_THRESHOLD;


    /* 동시에 이상인 센서 개수 */
    active_count =
        (unsigned)shock
        + (unsigned)sound
        + (unsigned)light;


    /*
     * 조도 변화가 작으면 비교 기준값 갱신
     *
     * 큰 변화가 지속되는 동안에는
     * 기존 기준값을 유지한다.
     */
    if (!light) {
        s_light_base = sample->light;
    }


    /*
     * 4. 모든 센서가 정상인 경우
     */
    if (active_count == 0U) {

        s_tracking = false;
        s_abnormal_samples = 0U;

        s_current = RISK_NORMAL;

        return s_current;
    }


    /*
     * 5. 이상 상태 최초 발생
     */
    if (!s_tracking) {

        s_tracking = true;

        s_abnormal_start_ms =
            sample->timestamp_ms;

        s_abnormal_samples = 0U;
    }


    /*
     * 6. 연속 이상 측정 조건 확인
     */
    if (s_abnormal_samples < EVENT_CONFIRM_SAMPLES) {
        s_abnormal_samples++;
    }

    /* 아직 연속 측정 조건 미충족 */
    if (s_abnormal_samples < EVENT_CONFIRM_SAMPLES) {

        s_current = RISK_NORMAL;

        return s_current;
    }


    /*
     * 7. DANGER 판정
     *
     * 아래 중 하나라도 만족하면 위험:
     *
     * - 두 종류 이상 센서 동시 이상
     * - 진동값이 임계값의 2배 이상
     * - 소음값이 임계값의 2배 이상
     * - 조도 변화량이 임계값의 2배 이상
     * - 이상 상태가 1000ms 이상 지속
     */
    if (
        active_count >= 2U ||

        (shock &&
         (uint32_t)sample->vibration >=
             RISK_SEVERE_MULTIPLIER
             * SHOCK_THRESHOLD) ||

        (sound &&
         (uint32_t)sample->sound_level >=
             RISK_SEVERE_MULTIPLIER
             * SOUND_THRESHOLD) ||

        (light &&
         light_diff >=
             RISK_SEVERE_MULTIPLIER
             * LIGHT_CHANGE_THRESHOLD) ||

        (uint32_t)(
            sample->timestamp_ms
            - s_abnormal_start_ms
        ) >= RISK_DANGER_HOLD_MS
    ) {

        s_current = RISK_DANGER;
    }

    /*
     * 8. CAUTION 판정
     *
     * 이상 상태가 확인되었지만
     * 위험 조건까지는 만족하지 않은 경우
     */
    else {

        s_current = RISK_CAUTION;
    }


    /* 최종 위험 단계 반환 */
    return s_current;
}
