
#include "../../include/event_history.h"
#include "../../include/project_types.h"

#include <stddef.h>

/* 최근 이벤트 10개 */
static EventHistoryEntry history[EVENT_HISTORY_CAPACITY];

/* 다음 이벤트 저장 위치 */
static uint32_t history_head = 0U;

/* 현재 이벤트 개수 */
static uint32_t history_count = 0U;

/* 이벤트 플래그 검사 */
static bool flags_valid(uint8_t flags)
{
    // 유효 마스크 플래그
    const uint8_t mask =
        EVENT_SHOCK |
        EVENT_SOUND |
        EVENT_LIGHT;

    // 0 = 이벤트 없음 -> 저장 X
    return flags != EVENT_NONE &&
           (flags & (uint8_t)~mask) == 0U;
}

/* History 초기화 */
void event_history_init(void)
{
    event_history_clear();
}

/* 새 이벤트 저장 */
bool event_history_push_with_delta(
    uint32_t timestamp_ms,
    uint8_t flags,
    uint16_t delta)
{
    if (!flags_valid(flags))
    {
        return false;
    }

    /* 이벤트 정보 저장 */
    history[history_head].timestamp_ms =
        timestamp_ms;

    history[history_head].event_flags =
        flags;

    history[history_head].delta =
        delta;

    /* 다음 저장 위치 */
    history_head =
        (history_head + 1U) %
        EVENT_HISTORY_CAPACITY;

    /* 최대 10개 */
    if (history_count < EVENT_HISTORY_CAPACITY)
    {
        history_count++;
    }

    return true;
}

/* 기존 API 유지 */
bool event_history_push(
    uint32_t timestamp_ms,
    uint8_t flags)
{
    return event_history_push_with_delta(
        timestamp_ms,
        flags,
        0U);
}

/* 복합 이벤트 갱신 */
bool event_history_upgrade_latest_with_delta(
    uint32_t timestamp_ms,
    uint8_t new_flags,
    uint16_t delta)
{
    uint32_t position;
    uint8_t old_flags;

    if (history_count == 0U ||
        !flags_valid(new_flags))
    {
        return false;
    }

    /* 가장 최근 이벤트 위치 */
    position =
        (history_head +
         EVENT_HISTORY_CAPACITY - 1U) %
        EVENT_HISTORY_CAPACITY;

    /* 동일 이벤트인지 확인 */
    if (history[position].timestamp_ms !=
        timestamp_ms)
    {
        return false;
    }

    // 기존 플래그 가져오기
    old_flags = history[position].event_flags;

    /* 기존 플래그 포함? 새 플래그가 기존 플래그도 포함하는지 검사 */
    if (new_flags == old_flags ||
        (uint8_t)(new_flags & old_flags) !=
            old_flags)
    {
        return false;
    }

    /* 기존 이벤트를 복합 이벤트로 갱신 */
    history[position].event_flags =
        new_flags;

    /* 기존 변화량보다 클 경우에만 갱신 */
    if (delta > history[position].delta)
    {
        history[position].delta = delta;
    }

    return true;
}

/* 기존 API 유지 */
bool event_history_upgrade_latest(
    uint32_t timestamp_ms,
    uint8_t new_flags)
{
    uint32_t position;

    if (history_count == 0U)
    {
        return false;
    }

    position =
        (history_head +
         EVENT_HISTORY_CAPACITY - 1U) %
        EVENT_HISTORY_CAPACITY;

    return event_history_upgrade_latest_with_delta(
        timestamp_ms,
        new_flags,
        history[position].delta);
}

/* 저장 개수 조회 */
uint32_t event_history_count(void)
{
    return history_count;
}

/* 최신 이벤트부터 조회 */
bool event_history_get(
    uint32_t index,
    EventHistoryEntry *out)
{
    uint32_t position;

    if (out == NULL ||
        index >= history_count)
    {
        return false;
    }

    /* index 0 = 최신 이벤트 */
    position =
        (history_head +
         EVENT_HISTORY_CAPACITY -
         1U - index) %
        EVENT_HISTORY_CAPACITY;

    *out = history[position];

    return true;
}

/* History 초기화 */
void event_history_clear(void)
{
    history_head = 0U;
    history_count = 0U;
}
