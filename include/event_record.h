
#ifndef EVENT_RECORD_H
#define EVENT_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include "project_types.h"

/* 이벤트 기록 모듈 초기화 */
void event_record_init(void);

/* 이벤트 이전 데이터를 복사하고 기록 시작 */
bool event_record_trigger(const EventInfo *event);

/* 이벤트 발생 이후 샘플 저장 */
void event_record_update(const SensorSample *sample);

/* 이벤트 기록 완료 여부 */
bool event_record_is_saved(void);

/* 이벤트 정보 조회 */
bool event_record_get_info(EventInfo *out);

/* 현재 기록된 샘플 개수 */
uint32_t event_record_get_count(void);

/* 시간순으로 기록 데이터 조회 */
bool event_record_get_sample(uint32_t index, SensorSample *out);

/* 기록 초기화 */
void event_record_clear(void);

#endif
