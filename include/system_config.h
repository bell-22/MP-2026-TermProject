
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

// 아래 임계값은 테스트용 초기값.
// 실제 센서 측정 결과에 따라 조정할 것.

#define SHOCK_THRESHOLD 2000U
#define SOUND_THRESHOLD 1000U
#define LIGHT_CHANGE_THRESHOLD 500U

// 이벤트 재감지 대기시간
#define EVENT_COOLDOWN_MS 1000U

// 연속 측정 조건
#define EVENT_CONFIRM_SAMPLES 2U

/* 4. KEY & Alarm */

#define KEY_DEBOUNCE_MS 50U

// LED 점멸 간격
#define LED_BLINK_INTERVAL_MS 250U

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