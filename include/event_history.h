
#ifndef EVENT_HISTORY_H
#define EVENT_HISTORY_H

#include <stdint.h>
#include <stdbool.h>

#define EVENT_HISTORY_CAPACITY 10U

typedef struct
{
    uint32_t timestamp_ms;
    uint8_t event_flags;

    /* 이벤트 발생 당시 최대 변화량 */
    uint16_t delta;

} EventHistoryEntry;

/* 이벤트 이력 초기화 */
void event_history_init(void);

/* 기존 이벤트 저장 API */
bool event_history_push(
    uint32_t timestamp_ms,
    uint8_t flags
);

/* DELTA를 포함한 이벤트 저장 */
bool event_history_push_with_delta(
    uint32_t timestamp_ms,
    uint8_t flags,
    uint16_t delta
);

/* 저장된 이벤트 개수 */
uint32_t event_history_count(void);

/* index 0 = 가장 최근 이벤트 */
bool event_history_get(
    uint32_t index,
    EventHistoryEntry *out
);

/* 기존 복합 이벤트 확장 API */
bool event_history_upgrade_latest(
    uint32_t timestamp_ms,
    uint8_t new_flags
);

/* DELTA를 포함한 복합 이벤트 확장 */
bool event_history_upgrade_latest_with_delta(
    uint32_t timestamp_ms,
    uint8_t new_flags,
    uint16_t delta
);

/* 전체 History 초기화 */
void event_history_clear(void);

#endif /* EVENT_HISTORY_H */
