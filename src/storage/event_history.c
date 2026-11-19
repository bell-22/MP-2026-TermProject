
#include "../../include/event_history.h"
#include "../../include/project_types.h"

#include <stddef.h>

/* 최근 이벤트 10개 저장 공간 */
static EventHistoryEntry history[EVENT_HISTORY_CAPACITY];

/* 다음 이벤트를 저장할 위치 */
static uint32_t history_head = 0U;

/* 현재 저장된 이벤트 개수 */
static uint32_t history_count = 0U;

/* 이벤트 플래그 유효성 검사 */
static bool history_flags_valid(uint8_t flags)
{
    const uint8_t valid_flags =
        EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT;

    return flags != EVENT_NONE &&
           (flags & (uint8_t)~valid_flags) == 0U;
}

/* 이벤트 이력 초기화 */
void event_history_init(void)
{
    event_history_clear();
}

/* 새 이벤트 저장 */
bool event_history_push(uint32_t timestamp_ms,
                        uint8_t flags)
{
    if (!history_flags_valid(flags))
    {
        return false;
    }

    /* 현재 위치에 이벤트 저장 */
    history[history_head].timestamp_ms = timestamp_ms;
    history[history_head].event_flags = flags;

    /* 다음 저장 위치로 이동 */
    history_head =
        (history_head + 1U) % EVENT_HISTORY_CAPACITY;

    /* 최대 10개까지만 증가 */
    if (history_count < EVENT_HISTORY_CAPACITY)
    {
        history_count++;
    }

    return true;
}

/* 저장된 이벤트 개수 조회 */
uint32_t event_history_count(void)
{
    return history_count;
}

/* 최신 이벤트부터 조회 */
bool event_history_get(uint32_t index,
                       EventHistoryEntry *out)
{
    uint32_t position;

    if (out == NULL || index >= history_count)
    {
        return false;
    }

    /* 가장 최근에 저장된 이벤트부터 역순 조회 */
    position =
        (history_head + EVENT_HISTORY_CAPACITY - 1U - index) % EVENT_HISTORY_CAPACITY;

    *out = history[position];

    return true;
}

/* 모든 이벤트 이력 삭제 */
void event_history_clear(void)
{
    history_head = 0U;
    history_count = 0U;
}
