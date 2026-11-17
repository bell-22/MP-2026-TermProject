
#ifndef RISK_LEVEL_H
#define RISK_LEVEL_H

#include "project_types.h"
#include "system_config.h"

/*
 * 위험 단계 판정 기준
 * 아래 값은 초기 구현용 임시 설정값이다.
 */

/* 임계값의 2배 이상이면 강한 이상 상태 */
#define RISK_SEVERE_MULTIPLIER 2U

/* 이상 상태가 1000ms 이상 지속되면 위험 */
#define RISK_DANGER_HOLD_MS 1000U

/* 센서 샘플 간격이 20ms를 넘으면 연속성 초기화 */
#define RISK_MAX_SAMPLE_GAP_MS \
    (2U * SAMPLE_INTERVAL_MS)

/* 위험 단계 */
typedef enum {
    RISK_NORMAL = 0,
    RISK_CAUTION = 1,
    RISK_DANGER = 2
} RiskLevel;

/* 위험 단계 판정 모듈 초기화 */
void risk_level_init(void);

/*
 * SensorSample 기반 위험 단계 갱신
 *
 * 10ms마다 호출한다.
 *
 * NULL 입력 시 이전 위험 단계를 유지한다.
 */
RiskLevel risk_level_update(
    const SensorSample *sample
);

#endif /* RISK_LEVEL_H */
