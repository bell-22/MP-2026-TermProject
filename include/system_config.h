
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

/* 1. Timer & Sampling */
#define TIMER_INTERVAL_MS 1U
#define SAMPLE_INTERVAL_MS 10U

/* 2. Ring Buffer & Event Recording */

// 이벤트 발생 이전 5초
#define PRE_EVENT_DURATION_MS 5000U

// 이벤트 발생 이후 5초
#define POST_EVENT_DURATION_MS 5000U

// 10ms 주기 기준
#define PRE_EVENT_SAMPLES \
    (PRE_EVENT_DURATION_MS / SAMPLE_INTERVAL_MS)

#define POST_EVENT_SAMPLES \
    (POST_EVENT_DURATION_MS / SAMPLE_INTERVAL_MS)

// Ring Buffer: 500개
#define RING_BUFFER_SIZE \
    PRE_EVENT_SAMPLES

// 이벤트 기록: 총 1000개
#define EVENT_RECORD_SIZE \
    (PRE_EVENT_SAMPLES + POST_EVENT_SAMPLES)

// 저장 가능한 이벤트 기록 개수
#define MAX_EVENT_RECORDS 1U



/* 3. Event Detection */

/*
 * 초기 기준값 측정 시간
 *
 * 프로그램 시작 후 1초 동안
 * 진동 / 조도 / 소음 평균값 계산
 */
#define BASELINE_CALIBRATION_MS 1000U

/*
 * 초기 평균값 대비 변화량 임계값
 *
 * 실제 보드 실험을 통해 추후 조정
 */
#define SHOCK_CHANGE_THRESHOLD 800U
#define SOUND_CHANGE_THRESHOLD 1800U
#define LIGHT_CHANGE_THRESHOLD 150U

/*
 * 소음 이벤트 중복 감지 방지
 *
 * 초기 소음 평균값 대비 변화량이
 * 400 미만인 상태가 300ms 유지되면
 * 다음 소음 이벤트 감지 허용
 */
#define SOUND_RESET_CHANGE_THRESHOLD 400U
#define SOUND_RESET_DURATION_MS 300U

/* 이벤트 재감지 대기시간 */
#define EVENT_COOLDOWN_MS 1000U

/*
 * 복합 이벤트 확장 허용 시간 (추가)
 *
 * 최초 이벤트 발생 후 500ms 이내에서만
 * 기존 이벤트의 복합 확장을 허용한다.
 *
 * 500ms를 초과하면 독립 이벤트로 처리한다.
 */
#define EVENT_COMBINE_WINDOW_MS 500U

/* 연속 2회 이상 감지 시 이벤트 확정 */
#define EVENT_CONFIRM_SAMPLES 2U

#define LIGHT_RESET_CHANGE_THRESHOLD 75U
#define LIGHT_RESET_DURATION_MS      300U
/* 4. KEY & Alarm */

#define KEY_DEBOUNCE_MS 50U

// LED 점멸 간격
#define LED_BLINK_INTERVAL_MS 250U

/*
 * 초기 측정 완료 알림
 * LEDR0~9 전체를 3회 점멸
 */
#define READY_LED_BLINK_INTERVAL_MS 150U
#define READY_LED_BLINK_COUNT 3U

// 경고음 기본 지속시간
#define ALARM_DURATION_MS 200U

// 단일 이벤트 경고 주파수
#define ALARM_NORMAL_FREQ_HZ 800U

// 복합 이벤트 경고 주파수
#define ALARM_MULTIPLE_LOW_HZ 600U
#define ALARM_MULTIPLE_HIGH_HZ 1200U

// 일반 경고음 사이 무음 시간
#define ALARM_GAP_MS 100U

// 복합 이벤트 경고음 1회 지속시간 
#define ALARM_MULTIPLE_DURATION_MS 400U

// 복합 이벤트 두 음 사이 무음 시간
//#define ALARM_MULTIPLE_GAP_MS 200U

/* 5. Display Codes */

#define DISPLAY_NONE 0U
#define DISPLAY_SHOCK 1U
#define DISPLAY_SOUND 2U
#define DISPLAY_LIGHT 3U
#define DISPLAY_MULTIPLE 4U

// 센서 채널 배선 임시
/* ADC channel assignment (temporary) */
#define VIBRATION_ADC_CHANNEL 0U
#define LIGHT_ADC_CHANNEL 1U

#endif // SYSTEM_CONFIG_H