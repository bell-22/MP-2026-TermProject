
#ifndef EVENT_RECORD_H
#define EVENT_RECORD_H

#include <stdint.h>
#include <stdbool.h>

#include "project_types.h"

/* 이벤트 기록 초기화 */
void event_record_init(void);

/* 최초 이벤트 발생 시 기록 시작 */
bool event_record_trigger(const EventInfo *event);

/* ACTIVE 상태에서 새로운 이벤트 플래그 누적 */
void event_record_add_flags(uint8_t flags);

/* 이벤트 이후 센서 샘플 저장 */
void event_record_update(const SensorSample *sample);

/* 이벤트 기록 완료 여부 */
bool event_record_is_saved(void);

/* 현재 이벤트 정보 조회 */
bool event_record_get_info(EventInfo *out);

/* 현재 저장된 센서 샘플 개수 */
uint32_t event_record_get_count(void);

/* 저장된 센서 샘플 조회 (index = 0부터) */
bool event_record_get_sample(uint32_t index, SensorSample *out);

/* 이벤트 기록 초기화 및 EMPTY 상태로 복귀 */
void event_record_clear(void);

#endif /* EVENT_RECORD_H */
