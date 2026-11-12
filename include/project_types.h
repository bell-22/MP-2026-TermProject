
#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Event Types */
// 이벤트 종류 (비트 플래그)
typedef enum
{
    EVENT_NONE = 0,
    EVENT_SHOCK = 1 << 0, // 충격
    EVENT_SOUND = 1 << 1, // 소음
    EVENT_LIGHT = 1 << 2  // 조도 변화
} EventType;

// 이벤트 조합은 비트 OR 사용
// 예: EVENT_SHOCK | EVENT_SOUND

/* System States */
typedef enum
{
    STATE_MONITORING = 0, // 실시간 감시
    STATE_RECORDING,      // 이벤트 이후 데이터 기록
    STATE_SAVED           // 기록 완료 및 보존
} SystemState;

/* Sensor Data */
// 10ms 주기로 생성하는 통합 센서 데이터
typedef struct
{
    uint32_t timestamp_ms; // 시스템 시작 후 시간 (ms)

    uint16_t vibration;   // 진동 ADC 원시값
    uint16_t sound_level; // 10ms 오디오 구간의 음량 특징값
    uint16_t light;       // 조도 ADC 원시값

} SensorSample;

/* Event Information */
typedef struct
{
    uint32_t timestamp_ms; // 이벤트 발생 시간 (ms) -> 시스템 시작부터 경과한 밀리초
    uint8_t event_flags;   // EventType 0~7 비트 조합

} EventInfo;

#endif // PROJECT_TYPES_H
