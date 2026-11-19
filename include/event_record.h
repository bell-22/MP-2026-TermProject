
#ifndef EVENT_RECORD_H
#define EVENT_RECORD_H

#include <stdint.h>
#include <stdbool.h>

#include "project_types.h"

/* Event Record 초기화 */
void event_record_init(void);

/*
 * 새로운 이벤트 발생 시
 * 기존 Record를 최신 이벤트 기준으로 교체
 */
bool event_record_trigger(const EventInfo *event);

/* 최신 이벤트 이후 센서 샘플 저장 */
void event_record_update(const SensorSample *sample);

/* 최신 Record 저장 완료 여부 */
bool event_record_is_saved(void);

/* 현재 저장된 최신 이벤트 정보 조회 */
bool event_record_get_info(EventInfo *out);

/* 현재 저장된 전체 센서 샘플 개수 */
uint32_t event_record_get_count(void);

/* 저장된 센서 샘플 조회 */
bool event_record_get_sample(
    uint32_t index,
    SensorSample *out);

/* Record 초기화 */
void event_record_clear(void);

#endif /* EVENT_RECORD_H */
