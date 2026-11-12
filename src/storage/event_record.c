
#include "event_record.h"
#include "ring_buffer.h"
#include "system_config.h"

#include <stddef.h>

/* 이벤트 기록 상태 (모듈 내부에서만 사용) */
typedef enum
{
    RECORD_EMPTY = 0,
    RECORD_ACTIVE,
    RECORD_SAVED
} RecordStatus;

/* 이벤트 전후 센서 데이터 저장 공간 */
static SensorSample record[EVENT_RECORD_SIZE];

/* 현재 이벤트 정보 */
static EventInfo event_info;

/* 기록된 전체 샘플 수 */
static uint32_t record_count = 0;

/* 이벤트 이후에 저장한 샘플 수 */
static uint32_t post_count = 0;

/* 현재 기록 상태 */
static RecordStatus record_status = RECORD_EMPTY;

/* 이벤트 기록 초기화 */
void event_record_init(void)
{
    event_record_clear();
}

/* 이벤트 발생 시 기록 시작 */
bool event_record_trigger(const EventInfo *event)
{
    if (event == NULL || record_status != RECORD_EMPTY)
    {
        return false;
    }

    /* 유효한 이벤트 플래그인지 확인 */
    if (event->event_flags == EVENT_NONE ||
        (event->event_flags &
         (uint8_t)~(EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT)) != 0U)
    {
        return false;
    }

    event_info = *event;

    record_count = 0;
    post_count = 0;

    /* Ring Buffer에 저장된 이벤트 이전 데이터 복사 */
    uint32_t pre_count = ring_buffer_count();

    if (pre_count > PRE_EVENT_SAMPLES)
    {
        pre_count = PRE_EVENT_SAMPLES;
    }

    for (uint32_t i = 0; i < pre_count; i++)
    {
        SensorSample sample;

        if (!ring_buffer_get(i, &sample))
        {
            /* 정상적인 Ring Buffer 상태에서는 발생하지 않아야 함 */
            event_record_clear();
            return false;
        }

        record[record_count++] = sample;
    }

    record_status = RECORD_ACTIVE;

    return true;
}

/* 이벤트 이후 데이터 추가 */
void event_record_update(const SensorSample *sample)
{
    if (sample == NULL || record_status != RECORD_ACTIVE)
    {
        return;
    }

    if (post_count >= POST_EVENT_SAMPLES ||
        record_count >= EVENT_RECORD_SIZE)
    {
        return;
    }

    record[record_count++] = *sample;
    post_count++;

    /* 이벤트 이후 500개 저장 완료 */
    if (post_count == POST_EVENT_SAMPLES)
    {
        record_status = RECORD_SAVED;
    }
}

/* 기록 완료 여부 */
bool event_record_is_saved(void)
{
    return record_status == RECORD_SAVED;
}

/* 이벤트 정보 조회 */
bool event_record_get_info(EventInfo *out)
{
    if (out == NULL || record_status == RECORD_EMPTY)
    {
        return false;
    }

    *out = event_info;
    return true;
}

/* 현재 유효한 기록 샘플 개수 */
uint32_t event_record_get_count(void)
{
    return record_count;
}

/* 이벤트 기록 조회 */
bool event_record_get_sample(uint32_t index, SensorSample *out)
{
    if (out == NULL || index >= record_count)
    {
        return false;
    }

    *out = record[index];
    return true;
}

/* 기록 초기화 */
void event_record_clear(void)
{
    record_count = 0;
    post_count = 0;
    record_status = RECORD_EMPTY;

    event_info.timestamp_ms = 0;
    event_info.event_flags = EVENT_NONE;
}
