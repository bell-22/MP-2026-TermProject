
#include "../../include/alarm.h"
#include "../../include/alarm_audio_driver.h"
#include "../../include/system_config.h"

/* 현재 경고 패턴 진행 여부 */
static bool s_active;

/* 경고 시작 시각 설정 여부 */
static bool s_started;

/* 현재 경고 이벤트 코드 */
static uint8_t s_event_code;

/* 경고 시작 시각 */
static uint32_t s_start_ms;

/* 현재 출력 요청 중인 주파수 */
static uint32_t s_output_frequency_hz;


/*
 * 실제 출력 주파수 변경
 *
 * 같은 주파수라면 드라이버를
 * 반복 호출하지 않는다.
 */
static void output_frequency(uint32_t frequency_hz)
{
    if (s_output_frequency_hz != frequency_hz) {

        s_output_frequency_hz = frequency_hz;

        alarm_audio_set_frequency(frequency_hz);
    }
}


/* Audio 경고 시스템 초기화 */
void alarm_init(void)
{
    s_active = false;
    s_started = false;

    s_event_code = DISPLAY_NONE;

    s_start_ms = 0U;
    s_output_frequency_hz = 0U;

    /* 초기 경고음 OFF */
    alarm_audio_set_frequency(0U);
}


/* 이벤트 종류에 따른 경고 시작 */
void alarm_start(uint8_t event_code)
{
    /* 이미 경고 중이면 재시작하지 않음 */
    if (s_active) {
        return;
    }

    /* 정상 및 잘못된 코드 무시 */
    if (event_code < DISPLAY_SHOCK ||
        event_code > DISPLAY_MULTIPLE) {
        return;
    }

    /* 이벤트 종류 저장 */
    s_event_code = event_code;

    /* 첫 update에서 시작 시각 설정 */
    s_started = false;
    s_active = true;
}


/* 시스템 시간에 따른 경고 패턴 갱신 */
void alarm_update(uint32_t timestamp_ms)
{
    uint32_t elapsed;
    uint32_t duration_ms;

    uint32_t frequency_hz = 0U;

    /* 경고가 진행 중이지 않으면 무동작 */
    if (!s_active) {
        return;
    }

    /* 처음 호출한 시각을 시작 시각으로 저장 */
    if (!s_started) {
        s_start_ms = timestamp_ms;
        s_started = true;
    }

    /*
     * 시작 시점부터 경과한 시간
     * unsigned 연산으로 타이머 오버플로 고려
     */
    elapsed = (uint32_t)(
        timestamp_ms - s_start_ms
    );

    /*
     * 1. 복합 이벤트
     *
     * 낮은 음 600Hz: 400ms
     * 높은 음 1200Hz: 400ms
     */
    if (s_event_code == DISPLAY_MULTIPLE) {

        duration_ms = 2U * ALARM_MULTIPLE_DURATION_MS;

        if (elapsed >= duration_ms) {
            alarm_stop();
            return;
        }

        if (elapsed < ALARM_MULTIPLE_DURATION_MS) {
            frequency_hz = ALARM_MULTIPLE_LOW_HZ;
        } else {
            frequency_hz = ALARM_MULTIPLE_HIGH_HZ;
        }
    }

    /*
     * 2. 단일 이벤트
     *
     * SHOCK: 800Hz 1회
     * SOUND: 800Hz 2회
     * LIGHT: 800Hz 3회
     */
    else {

        /* DISPLAY_SHOCK=1, SOUND=2, LIGHT=3 */
        uint32_t beep_count =
            (uint32_t)s_event_code;

        /* 경고음 200ms + 무음 100ms */
        uint32_t period_ms =
            ALARM_DURATION_MS + ALARM_GAP_MS;

        /* 마지막 경고 뒤에는 무음 간격 없음 */
        duration_ms =
            beep_count * ALARM_DURATION_MS
            + (beep_count - 1U) * ALARM_GAP_MS;

        /* 전체 경고 종료 */
        if (elapsed >= duration_ms) {
            alarm_stop();
            return;
        }

        /* 현재 경고음 출력 구간인지 확인 */
        if ((elapsed % period_ms)
            < ALARM_DURATION_MS) {

            frequency_hz = ALARM_NORMAL_FREQ_HZ;
        }
    }

    /*
     * 주파수 변경 요청
     *
     * 0Hz: 무음
     * 600Hz: 복합 낮은 음
     * 800Hz: 일반 경고
     * 1200Hz: 복합 높은 음
     */
    output_frequency(frequency_hz);
}


/* KEY0 등으로 경고음 즉시 중지 */
void alarm_stop(void)
{
    /* Audio 출력 중단 */
    output_frequency(0U);

    /* 경고 상태 초기화 */
    s_active = false;
    s_started = false;

    s_event_code = DISPLAY_NONE;
}


/* 경고 패턴 실행 여부 반환 */
bool alarm_is_active(void)
{
    /*
     * 무음 간격에도 true
     *
     * 추후 Audio-in 재감지 방지 기능에서
     * 활용 가능하다.
     */
    return s_active;
}
