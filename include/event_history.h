
#ifndef EVENT_HISTORY_H
#define EVENT_HISTORY_H

#include <stdint.h>
#include <stdbool.h>

#define EVENT_HISTORY_CAPACITY 10U

typedef struct
{
    uint32_t timestamp_ms;
    uint8_t event_flags;
} EventHistoryEntry;

/* 이벤트 이력 초기화 */
void event_history_init(void);

/* 새 이벤트 저장 */
bool event_history_push(uint32_t timestamp_ms,
                        uint8_t flags);

/* 현재 저장된 이벤트 개수 */
uint32_t event_history_count(void);

/*
 * 이벤트 이력 조회
 * index 0 = 가장 최근 이벤트
 * index 1 = 두 번째로 최근 이벤트
 */
bool event_history_get(uint32_t index,
                       EventHistoryEntry *out);

/* 모든 이벤트 이력 삭제 */
void event_history_clear(void);

#endif
