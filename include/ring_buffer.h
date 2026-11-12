

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "project_types.h"

/* Ring Buffer 초기화 */
void ring_buffer_init(void);

/* 새로운 센서 데이터 저장 */
void ring_buffer_push(const SensorSample *sample);

/* 현재 저장된 유효 데이터 개수 */
uint32_t ring_buffer_count(void);

/* 오래된 순서로 데이터 조회 (index = 0: 가장 오래된 데이터) */
bool ring_buffer_get(uint32_t index, SensorSample *out);

#endif
