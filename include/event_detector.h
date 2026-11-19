
#ifndef EVENT_DETECTOR_H
#define EVENT_DETECTOR_H

#include "project_types.h"

/*
 * 이벤트 감지 모듈 초기화
 */
void event_detector_init(void);

/*
 * 초기 1초 동안 계산한 평균값 설정
 *
 * vibration_base: 진동 평균
 * light_base: 조도 평균
 * sound_base: 소음 평균
 */
void event_detector_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base
);

/*
 * 센서 데이터 기반 이벤트 감지
 *
 * 진동 / 소음 / 조도 모두
 * 초기 평균값 대비 변화량 사용
 *
 * EVENT_NONE  = 0
 * EVENT_SHOCK = 1
 * EVENT_SOUND = 2
 * EVENT_LIGHT = 4
 */
uint8_t event_detect(const SensorSample *sample);

#endif /* EVENT_DETECTOR_H */
