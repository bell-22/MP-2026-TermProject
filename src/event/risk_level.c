
#include "../../include/risk_level.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 초기 1초 평균값 */
static uint16_t s_vibration_base;
static uint16_t s_light_base;
static uint16_t s_sound_base;

static bool s_baseline_ready;

/* 이전 샘플 시간 관리 */
static bool s_have_last_sample;
static uint32_t s_last_sample_ms;

/* 이상 상태 추적 */
static bool s_tracking;
static uint32_t s_abnormal_start_ms;
static uint32_t s_abnormal_samples;

/* 현재 위험 단계 */
static RiskLevel s_current;


/* 두 측정값의 절댓값 차이 */
static uint32_t abs_diff_u16(uint16_t a, uint16_t b)
{
    if (a >= b) {
        return (uint32_t)(a - b);
    }

    return (uint32_t)(b - a);
}


/*
 * 위험도 모듈 초기화
 */
void risk_level_init(void)
{
    s_vibration_base = 0U;
    s_light_base = 0U;
    s_sound_base = 0U;

    s_baseline_ready = false;

    s_have_last_sample = false;
    s_last_sample_ms = 0U;

    s_tracking = false;
    s_abnormal_start_ms = 0U;
    s_abnormal_samples = 0U;

    s_current = RISK_NORMAL;
}


/*
 * 초기 평균값 설정
 */
void risk_level_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base)
{
    /* 이전 위험도 상태 초기화 */
    risk_level_init();

    s_vibration_base = vibration_base;
    s_light_base = light_base;
    s_sound_base = sound_base;

    s_baseline_ready = true;
}


/*
 * 센서 데이터 기반 위험 단계 판정
 */
RiskLevel risk_level_update(
    const SensorSample *sample)
{
    bool shock;
    bool sound;
    bool light;

    unsigned active_count;

    uint32_t vibration_diff;
    uint32_t light_diff;
    uint32_t sound_diff;

    if (sample == NULL) {
        return s_current;
    }

    if (!s_baseline_ready) {
        return RISK_NORMAL;
    }


    /*
     * 1. 샘플 간격 확인
     *
     * 기존 기능 유지
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
     * 2. 세 센서의 변화량 계산
     */

    vibration_diff = abs_diff_u16(
        sample->vibration,
        s_vibration_base
    );

    light_diff = abs_diff_u16(
        sample->light,
        s_light_base
    );

    sound_diff = abs_diff_u16(
        sample->sound_level,
        s_sound_base
    );


    /*
     * 3. 센서별 이상 상태 판정
     */

    shock =
        vibration_diff >= SHOCK_CHANGE_THRESHOLD;

    sound =
        sound_diff >= SOUND_CHANGE_THRESHOLD;

    light =
        light_diff >= LIGHT_CHANGE_THRESHOLD;


    /* 동시에 이상인 센서 개수 */
    active_count =
        (unsigned)shock +
        (unsigned)sound +
        (unsigned)light;


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
        s_abnormal_start_ms = sample->timestamp_ms;
        s_abnormal_samples = 0U;
    }


    /*
     * 6. 연속 이상 측정 확인
     */
    if (s_abnormal_samples < EVENT_CONFIRM_SAMPLES) {
        s_abnormal_samples++;
    }

    if (s_abnormal_samples < EVENT_CONFIRM_SAMPLES) {

        s_current = RISK_NORMAL;

        return s_current;
    }


    /*
     * 7. DANGER 판정
     *
     * - 두 센서 이상 동시 이상
     * - 진동 변화량이 기준의 2배 이상
     * - 소음 변화량이 기준의 2배 이상
     * - 조도 변화량이 기준의 2배 이상
     * - 이상 상태 1000ms 이상 지속
     */
    if (
        active_count >= 2U ||

        (shock &&
        vibration_diff >=
            RISK_SEVERE_MULTIPLIER *
            SHOCK_CHANGE_THRESHOLD) ||

        (sound &&
        sound_diff >=
            RISK_SEVERE_MULTIPLIER *
            SOUND_CHANGE_THRESHOLD) ||

        (light &&
        light_diff >=
            RISK_SEVERE_MULTIPLIER *
            LIGHT_CHANGE_THRESHOLD) ||

        (uint32_t)(
            sample->timestamp_ms -
            s_abnormal_start_ms
        ) >= RISK_DANGER_HOLD_MS
    ) {

        s_current = RISK_DANGER;
    }
    else {

        /*
         * 8. CAUTION 판정
         *
         * 이상 상태가 확인되었지만
         * DANGER 조건에는 해당하지 않음
         */
        s_current = RISK_CAUTION;
    }

    return s_current;
}
