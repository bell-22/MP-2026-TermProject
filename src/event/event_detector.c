#include "../../include/event_detector.h"
#include "../../include/system_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ==========================================
 * DE1-SoC Sensor Blackbox - Event Detector
 * ==========================================
 *
 * 기능:
 * 1. 초기 센서 평균값 저장
 * 2. 현재 센서값과 초기 평균값의 변화량 계산
 * 3. 진동 / 소음 / 조도 이벤트 감지
 * 4. 연속 감지를 통한 순간 노이즈 방지
 * 5. 래치를 이용한 동일 이벤트 중복 감지 방지
 * 6. 쿨다운을 이용한 이벤트 재감지 제한
 * 7. 복합 이벤트 판정
 *
 * 모든 이벤트 감지 및 해제 임계값은
 * system_config.h의 설정값을 사용한다.
 *
 * 현재 감지 임계값:
 * SHOCK : 800
 * SOUND : 1800
 * LIGHT : 150
 *
 * 임계값은 실제 보드 실험을 통해 조정할 수 있다.
 */


/* =====================================
 * 1. Event Detection State
 * ===================================== */

/*
 * 센서별 이벤트 감지 상태
 *
 * consecutive:
 *   임계값 이상의 변화량이 연속으로
 *   감지된 샘플 개수
 *
 * last_event_ms:
 *   마지막 이벤트 확정 시간
 *
 * latched:
 *   동일한 이상 상태에서 이벤트가
 *   반복 발생하지 않도록 하는 잠금 상태
 *
 * has_event:
 *   이전에 이벤트가 발생한 적이 있는지 표시
 *
 *   최초 이벤트 발생 전에는 쿨다운을
 *   적용하지 않기 위해 사용한다.
 */
typedef struct
{
    uint32_t consecutive;
    uint32_t last_event_ms;
    bool latched;
    bool has_event;
} DetectionState;


/* 센서별 독립적인 이벤트 감지 상태 */
static DetectionState s_shock;
static DetectionState s_sound;
static DetectionState s_light;


/* =====================================
 * 2. Sensor Baseline
 * ===================================== */

/*
 * 초기 Calibration에서 계산한 센서 평균값
 *
 * main.c에서 초기 보정이 완료되면
 * event_detector_set_baseline()을 통해 전달받는다.
 *
 * 이후 각 센서의 이벤트 감지 기준값으로 사용한다.
 */
static uint16_t s_vibration_base;
static uint16_t s_light_base;
static uint16_t s_sound_base;

/*
 * 초기 평균 측정 완료 여부
 *
 * false:
 *   초기 기준값이 설정되지 않은 상태
 *   이벤트 감지 수행하지 않음
 *
 * true:
 *   초기 기준값 설정 완료
 *   이벤트 감지 가능
 */
static bool s_baseline_ready;


/* =====================================
 * 3. Sensor Reset State
 * ===================================== */

/*
 * 소음 이벤트 재감지 방지용 상태
 *
 * 소음 변화량이 해제 임계값 미만인 상태로
 * 일정 시간 유지되었는지 확인한다.
 *
 * s_sound_quiet_started:
 *   조용한 상태의 시간 측정 시작 여부
 *
 * s_sound_quiet_start_ms:
 *   조용한 상태가 시작된 시간
 */
static bool s_sound_quiet_started;
static uint32_t s_sound_quiet_start_ms;

/*
 * 조도 이벤트 재감지 방지용 상태
 *
 * 조도 변화량이 해제 임계값 미만인 상태로
 * 일정 시간 유지되었는지 확인한다.
 *
 * s_light_stable_started:
 *   안정된 조도 상태의 시간 측정 시작 여부
 *
 * s_light_stable_start_ms:
 *   안정된 조도 상태가 시작된 시간
 */
static bool s_light_stable_started;
static uint32_t s_light_stable_start_ms;


/* =====================================
 * 4. Sensor Difference Calculation
 * ===================================== */

/*
 * 두 측정값의 절댓값 차이 계산
 *
 * DELTA = |현재 측정값 - 초기 평균값|
 *
 * 센서값이 초기 평균값보다 증가하거나
 * 감소하는 경우를 모두 이벤트 감지에 반영한다.
 *
 * 예:
 *   현재값 1200, 기준값 1000 -> 200
 *   현재값  800, 기준값 1000 -> 200
 *
 * VGA 실시간 화면의 부호 있는 DELTA와 달리
 * 이벤트 감지에서는 변화량의 크기를 사용한다.
 *
 * uint16_t의 뺄셈에서 음수 결과가 발생하지 않도록
 * 큰 값에서 작은 값을 빼도록 처리한다.
 */
static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    if (a >= b)
    {
        return (uint16_t)(a - b);
    }

    return (uint16_t)(b - a);
}


/* =====================================
 * 5. Common Event Confirmation
 * ===================================== */

/*
 * 일반 이벤트 확정 함수
 *
 * 적용 조건:
 *
 * 1. 센서 변화량이 감지 임계값 이상
 * 2. EVENT_CONFIRM_SAMPLES 이상 연속 감지
 * 3. 기존 이벤트 래치가 해제된 상태
 * 4. EVENT_COOLDOWN_MS 이상 경과
 *
 * 현재 설정:
 *   연속 감지 횟수 : 2회
 *   이벤트 쿨다운 : 1000ms
 *
 * 진동 이벤트에서 직접 사용하며,
 * 소음 / 조도 이벤트에서도 각각의 별도
 * 래치 해제 및 쿨다운 검사 후 사용한다.
 *
 * 매개변수:
 *   state    : 해당 센서의 감지 상태
 *   exceeded : 감지 임계값 이상인지 여부
 *   now_ms   : 현재 샘플의 측정 시간
 *
 * 반환값:
 *   true  : 새로운 이벤트 확정
 *   false : 이벤트 미확정
 */
static bool detect_confirmed(
    DetectionState *state,
    bool exceeded,
    uint32_t now_ms)
{
    /*
     * A. 감지 임계값 미만
     *
     * 변화량이 감지 임계값 미만이면
     * 연속 감지 횟수와 래치를 초기화한다.
     *
     * 소음과 조도는 별도의 래치 해제 조건을
     * 먼저 검사하므로, 활성 래치 상태에서는
     * 해당 센서의 전용 함수가 해제를 관리한다.
     */
    if (!exceeded)
    {
        state->consecutive = 0U;
        state->latched = false;

        return false;
    }

    /*
     * B. 기존 이벤트 중복 감지 방지
     *
     * 이미 이벤트가 확정되어 래치가 설정된 경우
     * 같은 이상 상태를 다시 이벤트로 처리하지 않는다.
     */
    if (state->latched)
    {
        return false;
    }

    /*
     * C. 연속 감지 횟수 증가
     *
     * 연속 감지 횟수가 설정값보다 작은 경우에만
     * 카운터를 증가시킨다.
     *
     * 설정값에 도달하면 더 이상 증가시키지 않아
     * 불필요한 카운터 증가를 방지한다.
     */
    if (state->consecutive < EVENT_CONFIRM_SAMPLES)
    {
        state->consecutive++;
    }

    /*
     * 아직 연속 감지 조건을 만족하지 못한 경우
     * 이벤트를 확정하지 않는다.
     *
     * 현재는 연속 2개의 샘플에서 임계값 이상인
     * 경우에 이벤트 확정 조건을 만족한다.
     */
    if (state->consecutive < EVENT_CONFIRM_SAMPLES)
    {
        return false;
    }

    /*
     * D. 이벤트 재감지 대기시간 확인
     *
     * [수정] SHOCK 쿨다운 및 래치 오류 방지
     *
     * 이전에는 쿨다운 조건을 확인하기 전에
     * latched를 true로 설정하였다.
     *
     * 이 경우 쿨다운 중 이벤트가 확정되지 않아도
     * 래치가 설정되어, 쿨다운 종료 이후까지
     * 이상 상태가 지속될 때 감지하지 못할 수 있었다.
     *
     * 따라서 쿨다운을 먼저 확인하고,
     * 조건을 만족한 경우에만 래치를 설정한다.
     *
     * has_event가 false인 최초 이벤트에는
     * 쿨다운을 적용하지 않는다.
     *
     * 쿨다운 중에는 연속 감지 횟수를 유지하되
     * 새로운 래치는 설정하지 않는다.
     */
    if (state->has_event &&
        (uint32_t)(now_ms - state->last_event_ms)
            < EVENT_COOLDOWN_MS)
    {
        return false;
    }

    /*
     * E. 이벤트 확정
     *
     * 연속 감지 및 쿨다운 조건을 모두 만족한 경우에만
     * 이벤트 감지 래치를 설정한다.
     *
     * 이후 동일한 이상 상태에서는
     * 새로운 이벤트를 반복 반환하지 않는다.
     */
    state->latched = true;

    /*
     * 이벤트 발생 이력 저장
     *
     * 다음 이벤트의 쿨다운 판단에 사용한다.
     */
    state->has_event = true;
    state->last_event_ms = now_ms;

    /* 새로운 이벤트 확정 성공 */
    return true;
}


/* =====================================
 * 6. SOUND Event Confirmation
 * ===================================== */

/*
 * 소음 이벤트 확정
 *
 * 이벤트 감지 조건:
 *
 * 1. 초기 평균 대비 절댓값 변화량이
 *    SOUND_CHANGE_THRESHOLD 이상
 *
 * 2. EVENT_CONFIRM_SAMPLES 이상 연속 감지
 *
 * 3. 이벤트 확정 후 재감지 래치 설정
 *
 * 4. 변화량이 SOUND_RESET_CHANGE_THRESHOLD
 *    미만인 상태가 SOUND_RESET_DURATION_MS
 *    이상 유지되면 래치 해제
 *
 * 5. EVENT_COOLDOWN_MS 이상 경과 후
 *    새로운 이벤트 감지 허용
 *
 * 현재 system_config.h 설정:
 *
 *   감지 임계값      : 1800
 *   해제 임계값      : 400
 *   해제 유지시간    : 300ms
 *   재감지 쿨다운    : 1000ms
 *
 * 감지 임계값과 해제 임계값을 다르게 설정하여
 * 임계값 주변의 작은 변화로 인해 이벤트가
 * 반복 발생하는 현상을 방지한다.
 */
static bool detect_sound_confirmed(
    uint16_t sound_diff,
    uint32_t now_ms)
{
    /*
     * A. 이미 소음 이벤트가 확정된 상태
     *
     * 래치가 설정되어 있으면 새로운 이벤트를
     * 감지하지 않고 해제 조건만 확인한다.
     */
    if (s_sound.latched)
    {
        /*
         * 소음 변화량이 해제 임계값 미만인지 확인
         */
        if (sound_diff < SOUND_RESET_CHANGE_THRESHOLD)
        {
            /*
             * 처음으로 조용한 상태가 확인된 경우
             * 조용한 상태의 시작 시간을 저장한다.
             */
            if (!s_sound_quiet_started)
            {
                s_sound_quiet_started = true;
                s_sound_quiet_start_ms = now_ms;
            }

            /*
             * 조용한 상태가 설정된 유지시간 이상
             * 지속되었는지 확인한다.
             */
            else if (
                (uint32_t)(now_ms -
                           s_sound_quiet_start_ms)
                    >= SOUND_RESET_DURATION_MS)
            {
                /*
                 * 충분히 조용해지면 래치 해제
                 *
                 * 다음 소음 이벤트 감지를 위해
                 * 연속 감지 횟수도 초기화한다.
                 */
                s_sound.latched = false;
                s_sound.consecutive = 0U;

                /* 조용한 상태의 시간 측정 종료 */
                s_sound_quiet_started = false;
            }
        }
        else
        {
            /*
             * 다시 해제 임계값 이상으로 증가하면
             * 조용한 상태의 유지시간을 초기화한다.
             *
             * 이후 다시 조용해질 때부터
             * 유지시간을 새로 측정한다.
             */
            s_sound_quiet_started = false;
        }

        /*
         * 래치 해제 확인 중에는
         * 새로운 이벤트를 반환하지 않는다.
         */
        return false;
    }

    /*
     * B. 소음 이벤트 재감지 쿨다운
     *
     * 조용한 구간을 거쳐 래치가 해제된 후에도
     * 기존 이벤트의 쿨다운은 유지한다.
     *
     * 쿨다운 중에는 새로운 이벤트를 확정하지 않고
     * 연속 감지 카운터도 초기화한다.
     *
     * 쿨다운이 끝난 이후부터 새로운 연속 감지
     * 조건을 확인한다.
     */
    if (s_sound.has_event &&
        (uint32_t)(now_ms -
                   s_sound.last_event_ms)
            < EVENT_COOLDOWN_MS)
    {
        s_sound.consecutive = 0U;

        return false;
    }

    /*
     * C. 새로운 소음 이벤트 감지
     *
     * 소음 변화량이 감지 임계값 이상이면
     * 일반 이벤트 확정 함수를 통해
     * 연속 감지 조건을 검사한다.
     */
    return detect_confirmed(
        &s_sound,
        sound_diff >= SOUND_CHANGE_THRESHOLD,
        now_ms);
}


/* =====================================
 * 7. LIGHT Event Confirmation
 * ===================================== */

/*
 * 조도 이벤트 확정
 *
 * 이벤트 감지 조건:
 *
 * 1. 초기 평균 대비 절댓값 변화량이
 *    LIGHT_CHANGE_THRESHOLD 이상
 *
 * 2. EVENT_CONFIRM_SAMPLES 이상 연속 감지
 *
 * 3. 이벤트 발생 후 재감지 래치 설정
 *
 * 4. 변화량이 LIGHT_RESET_CHANGE_THRESHOLD
 *    미만인 상태가 LIGHT_RESET_DURATION_MS
 *    이상 유지되면 래치 해제
 *
 * 5. EVENT_COOLDOWN_MS 이상 경과 후
 *    새로운 이벤트 감지 허용
 *
 * 현재 system_config.h 설정:
 *
 *   감지 임계값      : 150
 *   해제 임계값      : 75
 *   해제 유지시간    : 300ms
 *   재감지 쿨다운    : 1000ms
 *
 * 감지 임계값보다 해제 임계값을 낮게 설정하여
 * 감지 조건과 해제 조건이 중첩되지 않도록 한다.
 *
 * 조도가 증가하거나 감소하는 경우 모두
 * 절댓값 변화량을 기준으로 이벤트를 감지한다.
 */
static bool detect_light_confirmed(
    uint16_t light_diff,
    uint32_t now_ms)
{
    /*
     * A. 이미 조도 이벤트가 확정된 상태
     *
     * 래치가 설정되어 있으면
     * 새로운 조도 이벤트를 감지하지 않고
     * 조도가 안정되었는지 확인한다.
     */
    if (s_light.latched)
    {
        /*
         * 조도 변화량이 해제 임계값 미만인지 확인
         */
        if (light_diff < LIGHT_RESET_CHANGE_THRESHOLD)
        {
            /*
             * 처음으로 안정된 조도 상태가 확인되면
             * 유지시간 측정을 시작한다.
             */
            if (!s_light_stable_started)
            {
                s_light_stable_started = true;
                s_light_stable_start_ms = now_ms;
            }

            /*
             * 안정된 조도 상태가 설정된 유지시간
             * 이상 지속되었는지 확인한다.
             */
            else if (
                (uint32_t)(now_ms -
                           s_light_stable_start_ms)
                    >= LIGHT_RESET_DURATION_MS)
            {
                /*
                 * 조도 래치 해제
                 *
                 * 다음 이벤트 감지를 위해
                 * 연속 감지 횟수도 초기화한다.
                 */
                s_light.latched = false;
                s_light.consecutive = 0U;

                /* 안정 상태 시간 측정 종료 */
                s_light_stable_started = false;
            }
        }
        else
        {
            /*
             * 조도 변화량이 다시 해제 임계값 이상이면
             * 안정 상태의 유지시간을 초기화한다.
             */
            s_light_stable_started = false;
        }

        /*
         * 래치 해제 확인 중에는
         * 새로운 이벤트를 반환하지 않는다.
         */
        return false;
    }

    /*
     * B. 조도 이벤트 재감지 쿨다운
     *
     * 래치가 해제되어도 이전 이벤트 발생 이후
     * 설정된 쿨다운 시간이 지나야 한다.
     *
     * 쿨다운 중에는 연속 감지 횟수를 초기화한다.
     */
    if (s_light.has_event &&
        (uint32_t)(now_ms -
                   s_light.last_event_ms)
            < EVENT_COOLDOWN_MS)
    {
        s_light.consecutive = 0U;

        return false;
    }

    /*
     * C. 새로운 조도 이벤트 감지
     *
     * 감지 임계값 이상의 변화량이 발생하면
     * 일반 이벤트 확정 함수를 통해
     * 연속 감지 조건을 검사한다.
     */
    return detect_confirmed(
        &s_light,
        light_diff >= LIGHT_CHANGE_THRESHOLD,
        now_ms);
}


/* =====================================
 * 8. Public API - Initialization
 * ===================================== */

/*
 * 이벤트 감지 모듈 초기화
 *
 * main.c에서 시스템 시작 시 호출한다.
 *
 * 초기화 대상:
 *
 * 1. 센서별 이벤트 감지 상태
 * 2. 초기 센서 기준값
 * 3. 기준값 설정 완료 여부
 * 4. 소음 래치 해제 시간 측정 상태
 * 5. 조도 래치 해제 시간 측정 상태
 *
 * 초기화 직후에는 기준값이 설정되지 않았으므로
 * 이벤트 감지를 수행하지 않는다.
 */
void event_detector_init(void)
{
    /*
     * 센서별 감지 상태 초기화
     *
     * consecutive   = 0
     * last_event_ms = 0
     * latched       = false
     * has_event     = false
     */
    s_shock = (DetectionState){0};
    s_sound = (DetectionState){0};
    s_light = (DetectionState){0};

    /* 초기 센서 기준값 초기화 */
    s_vibration_base = 0U;
    s_light_base = 0U;
    s_sound_base = 0U;

    /* 초기 기준값 설정 전 상태 */
    s_baseline_ready = false;

    /* 소음 래치 해제 시간 측정 상태 초기화 */
    s_sound_quiet_started = false;
    s_sound_quiet_start_ms = 0U;

    /* 조도 래치 해제 시간 측정 상태 초기화 */
    s_light_stable_started = false;
    s_light_stable_start_ms = 0U;
}


/* =====================================
 * 9. Public API - Set Baseline
 * ===================================== */

/*
 * 초기 센서 평균값 설정
 *
 * main.c에서 초기 Calibration 완료 후 호출한다.
 *
 * 매개변수 순서:
 *
 *   vibration_base : 진동 평균값
 *   light_base     : 조도 평균값
 *   sound_base     : 소음 평균값
 *
 * 전달받은 평균값은 각 센서의 변화량 계산 및
 * 이벤트 감지 기준값으로 사용한다.
 *
 * 새로운 기준값이 설정되면 이전 기준값에 의해
 * 발생했던 감지 상태가 남지 않도록
 * 모든 이벤트 감지 상태를 초기화한다.
 */
void event_detector_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base)
{
    /* 초기 센서 평균값 저장 */
    s_vibration_base = vibration_base;
    s_light_base = light_base;
    s_sound_base = sound_base;

    /*
     * 이전 이벤트 상태 초기화
     *
     * 기존 기준값에 따른 연속 감지 횟수,
     * 래치 및 쿨다운 이력을 제거한다.
     */
    s_shock = (DetectionState){0};
    s_sound = (DetectionState){0};
    s_light = (DetectionState){0};

    /* 소음 래치 해제 시간 측정 상태 초기화 */
    s_sound_quiet_started = false;
    s_sound_quiet_start_ms = 0U;

    /* 조도 래치 해제 시간 측정 상태 초기화 */
    s_light_stable_started = false;
    s_light_stable_start_ms = 0U;

    /*
     * 초기 평균값 설정 완료
     *
     * 이후부터 정상적인 이벤트 감지가 가능하다.
     */
    s_baseline_ready = true;
}


/* =====================================
 * 10. Public API - Event Detection
 * ===================================== */

/*
 * 센서 데이터 기반 이벤트 감지
 *
 * main.c의 Event Monitoring 단계에서 호출한다.
 *
 * 처리 순서:
 *
 * 1. 센서 샘플 및 초기 기준값 확인
 * 2. 센서별 절댓값 변화량 계산
 * 3. 센서별 이벤트 확정 조건 검사
 * 4. 새로운 이벤트 발생 여부 확인
 * 5. 복합 이벤트 판정
 * 6. 이벤트 플래그 반환
 *
 * 경고음 재감지 방지:
 *
 * main.c에서 경고음 활성 상태인 경우
 * 감지용 복사본의 sound_level을 초기 소음
 * 평균값으로 대체하여 전달한다.
 *
 * 따라서 이 함수에서는 별도의 Alarm API를
 * 호출하지 않고 전달받은 샘플을 기준으로
 * 이벤트를 판정한다.
 *
 * 원본 센서 데이터의 저장 및 VGA 출력은
 * main.c에서 별도로 처리한다.
 *
 * 복합 이벤트 판정:
 *
 * 새로운 센서 이벤트가 확정된 순간,
 * 다른 센서의 래치가 활성 상태이고 현재
 * 변화량도 감지 임계값 이상이면
 * 해당 센서의 플래그를 함께 반환한다.
 *
 * 기존 History / Record의 복합 이벤트 확장 여부는
 * main.c에서 EVENT_COMBINE_WINDOW_MS 및
 * 기존 기록 정보를 이용하여 별도로 판단한다.
 */
uint8_t event_detect(const SensorSample *sample)
{
    /*
     * 이벤트 플래그 초기화
     *
     * 이벤트 발생 전에는 EVENT_NONE 상태이다.
     *
     * 각 센서의 이벤트가 확정되면
     * 비트 OR 연산으로 플래그를 추가한다.
     */
    uint8_t flags = EVENT_NONE;

    /* 센서별 초기 평균값 대비 절댓값 변화량 */
    uint16_t vibration_diff;
    uint16_t light_diff;
    uint16_t sound_diff;


    /* =================================
     * A. Input Validation
     * ================================= */

    /*
     * 잘못된 샘플 포인터가 전달된 경우
     * 이벤트를 감지하지 않는다.
     */
    if (sample == NULL)
    {
        return EVENT_NONE;
    }

    /*
     * 초기 평균값 설정 전에는 감지하지 않음
     *
     * Calibration이 완료되지 않은 상태에서
     * 잘못된 기준값으로 이벤트가 발생하는 것을
     * 방지한다.
     */
    if (!s_baseline_ready)
    {
        return EVENT_NONE;
    }


    /* =================================
     * B. Sensor Difference Calculation
     * ================================= */

    /*
     * 초기 평균값 대비 진동 변화량 계산
     *
     * vibration_diff =
     * |현재 진동값 - 초기 진동 평균값|
     */
    vibration_diff = abs_diff_u16(
        sample->vibration,
        s_vibration_base);

    /*
     * 초기 평균값 대비 소음 변화량 계산
     *
     * sound_diff =
     * |현재 소음값 - 초기 소음 평균값|
     */
    sound_diff = abs_diff_u16(
        sample->sound_level,
        s_sound_base);

    /*
     * 초기 평균값 대비 조도 변화량 계산
     *
     * light_diff =
     * |현재 조도값 - 초기 조도 평균값|
     */
    light_diff = abs_diff_u16(
        sample->light,
        s_light_base);


    /* =================================
     * C. Individual Event Detection
     * ================================= */

    /*
     * 1. SHOCK 이벤트 감지
     *
     * 진동 변화량이 SHOCK_CHANGE_THRESHOLD
     * 이상인 경우 연속 감지 및 쿨다운 조건을
     * 확인한다.
     *
     * 새로운 충격 이벤트가 확정되면
     * EVENT_SHOCK 비트를 추가한다.
     *
     * 진동은 감지 임계값 미만으로 돌아오면
     * 일반 이벤트 확정 함수에서 래치를 해제한다.
     */
    if (detect_confirmed(
            &s_shock,
            vibration_diff >= SHOCK_CHANGE_THRESHOLD,
            sample->timestamp_ms))
    {
        flags |= EVENT_SHOCK;
    }

    /*
     * 2. SOUND 이벤트 감지
     *
     * 소음 전용 이벤트 확정 함수 사용
     *
     * 감지 임계값, 연속 감지, 래치 해제 유지시간,
     * 쿨다운 조건을 함께 확인한다.
     *
     * 새로운 소음 이벤트가 확정되면
     * EVENT_SOUND 비트를 추가한다.
     */
    if (detect_sound_confirmed(
            sound_diff,
            sample->timestamp_ms))
    {
        flags |= EVENT_SOUND;
    }

    /*
     * 3. LIGHT 이벤트 감지
     *
     * 조도 전용 이벤트 확정 함수 사용
     *
     * 감지 임계값과 해제 임계값을 분리하고,
     * 안정된 조도 상태가 일정 시간 유지되는지
     * 확인하여 중복 감지를 방지한다.
     *
     * 새로운 조도 이벤트가 확정되면
     * EVENT_LIGHT 비트를 추가한다.
     */
    if (detect_light_confirmed(
            light_diff,
            sample->timestamp_ms))
    {
        flags |= EVENT_LIGHT;
    }


    /* =================================
     * D. New Event Validation
     * ================================= */

    /*
     * 새로운 이벤트가 없는 경우
     * 기존 이벤트를 반복 반환하지 않음
     *
     * 기존 센서의 래치가 유지되고 있어도
     * 새로운 이벤트가 확정되지 않았다면
     * EVENT_NONE을 반환한다.
     *
     * 이를 통해 동일한 이상 상태에서
     * History 및 Event Record가
     * 반복 생성되는 것을 방지한다.
     */
    if (flags == EVENT_NONE)
    {
        return EVENT_NONE;
    }


    /* =================================
     * E. Multiple Event Detection
     * ================================= */

    /*
     * 복합 이벤트 판정
     *
     * 새로운 이벤트가 확정된 순간
     * 다른 센서의 래치 상태와 현재 변화량을
     * 함께 확인한다.
     *
     * 래치가 설정되어 있더라도 현재 변화량이
     * 감지 임계값 미만이라면 복합 이벤트에
     * 포함하지 않는다.
     *
     * 예:
     *
     * 기존 SHOCK 이벤트가 유지되는 상태에서
     * 새로운 LIGHT 이벤트가 확정된 경우
     *
     * 진동 변화량이 감지 임계값 이상이면
     * EVENT_SHOCK | EVENT_LIGHT 반환
     *
     * 이 함수는 복합 이벤트 플래그를 생성한다.
     *
     * 기존 기록을 확장할지 새로운 독립 기록으로
     * 저장할지는 main.c에서 판단한다.
     */


    /*
     * 진동 복합 이벤트 판정
     *
     * 조건:
     * 1. 기존 진동 이벤트 래치 활성
     * 2. 변화량이 SHOCK_CHANGE_THRESHOLD 이상
     */
    if (s_shock.latched &&
        vibration_diff >= SHOCK_CHANGE_THRESHOLD)
    {
        flags |= EVENT_SHOCK;
    }

    /*
     * 소음 복합 이벤트 판정
     *
     * 조건:
     * 1. 기존 소음 이벤트 래치 활성
     * 2. 변화량이 SOUND_CHANGE_THRESHOLD 이상
     *
     * main.c에서 경고음 활성 상태에 감지용 소음값을
     * 기준값으로 대체하면 sound_diff가 0이 되므로
     * SOUND가 복합 이벤트에 추가되지 않는다.
     */
    if (s_sound.latched &&
        sound_diff >= SOUND_CHANGE_THRESHOLD)
    {
        flags |= EVENT_SOUND;
    }

    /*
     * 조도 복합 이벤트 판정
     *
     * 조건:
     * 1. 기존 조도 이벤트 래치 활성
     * 2. 변화량이 LIGHT_CHANGE_THRESHOLD 이상
     */
    if (s_light.latched &&
        light_diff >= LIGHT_CHANGE_THRESHOLD)
    {
        flags |= EVENT_LIGHT;
    }


    /* =================================
     * F. Return Event Flags
     * ================================= */

    /*
     * 최종 이벤트 플래그 반환
     *
     * 단일 이벤트:
     *
     *   EVENT_SHOCK
     *   EVENT_SOUND
     *   EVENT_LIGHT
     *
     * 복합 이벤트:
     *
     *   EVENT_SHOCK | EVENT_SOUND
     *   EVENT_SHOCK | EVENT_LIGHT
     *   EVENT_SOUND | EVENT_LIGHT
     *   EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT
     *
     * 반환된 플래그는 main.c에서
     * 이벤트 분류, History 저장, Record 저장,
     * LED / HEX 및 경고음 처리에 사용한다.
     */
    return flags;
}