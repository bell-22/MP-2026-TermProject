
#ifndef RISK_LEVEL_H
#define RISK_LEVEL_H

#include "project_types.h"
#include "system_config.h"

/* 임계값의 2배 이상이면 강한 이상 상태 */
#define RISK_SEVERE_MULTIPLIER 2U

/* 이상 상태가 1000ms 이상 지속되면 위험 */
#define RISK_DANGER_HOLD_MS 1000U

/* 샘플 간격이 20ms를 넘으면 연속성 초기화 */
#define RISK_MAX_SAMPLE_GAP_MS \
    (2U * SAMPLE_INTERVAL_MS)

/* 위험 단계 */
typedef enum {
    RISK_NORMAL = 0,
    RISK_CAUTION = 1,
    RISK_DANGER = 2
} RiskLevel;

/* 위험도 모듈 초기화 */
void risk_level_init(void);

/*
 * 초기 1초 평균값 설정
 *
 * event_detector.c와 동일한 평균값 사용
 */
void risk_level_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base
);

/*
 * 센서 데이터 기반 위험도 갱신
 *
 * 세 센서 모두 초기 평균 대비 변화량 사용
 */
RiskLevel risk_level_update(
    const SensorSample *sample
);

#endif /* RISK_LEVEL_H */
