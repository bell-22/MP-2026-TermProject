
#include "../../include/event_record.h"
#include "../../include/ring_buffer.h"
#include "../../include/system_config.h"

#include <stddef.h>

/* 이벤트 기록 상태 */
typedef enum
{
    RECORD_EMPTY = 0,
    RECORD_ACTIVE,
    RECORD_SAVED
} RecordStatus;

/* 최신 이벤트 전후 센서 데이터 저장 공간 */
static SensorSample record[EVENT_RECORD_SIZE];

/* 가장 최근 이벤트 정보 */
static EventInfo event_info;

/* 현재 기록된 전체 샘플 수 */
static uint32_t record_count = 0U;

/* 이벤트 이후 저장한 샘플 수 */
static uint32_t post_count = 0U;

/* 현재 기록 상태 */
static RecordStatus record_status = RECORD_EMPTY;

/* 유효한 이벤트 플래그 */
#define VALID_EVENT_FLAGS \
    (EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT)

/* 이벤트 플래그 유효성 검사 */
static bool event_flags_valid(uint8_t flags)
{
    return flags != EVENT_NONE &&
           (flags & (uint8_t)~VALID_EVENT_FLAGS) == 0U;
}

/* Record 초기화 */
void event_record_init(void)
{
    event_record_clear();
}

/*
 * 최신 이벤트 발생 시 Record 전체 교체
 *
 * EMPTY  -> 새로운 Record 시작
 * ACTIVE -> 진행 중인 Record를 최신 이벤트로 교체
 * SAVED  -> 완료된 Record를 최신 이벤트로 교체
 *
 * 최초 이벤트가 아닌 가장 최근 이벤트 기준으로
 * 이전 500개 + 이후 500개 센서 데이터를 저장
 */
bool event_record_trigger(const EventInfo *event)
{
    uint32_t pre_count;

    if (event == NULL)
    {
        return false;
    }

    if (!event_flags_valid(event->event_flags))
    {
        return false;
    }

    /*
     * 현재 Ring Buffer의 이전 데이터 개수 확인
     */
    pre_count = ring_buffer_count();

    if (pre_count > PRE_EVENT_SAMPLES)
    {
        pre_count = PRE_EVENT_SAMPLES;
    }

    /*
     * 최신 이벤트 정보 저장
     * timestamp와 flags 모두 새 이벤트 값으로 교체
     */
    event_info = *event;

    /* 기존 Record 초기화 */
    record_count = 0U;
    post_count = 0U;

    /* 최신 이벤트 이전 센서 데이터 복사 */
    for (uint32_t i = 0U; i < pre_count; i++)
    {
        SensorSample sample;

        if (!ring_buffer_get(i, &sample))
        {
            event_record_clear();
            return false;
        }

        record[record_count++] = sample;
    }

    /* 최신 이벤트 이후 센서 데이터 수집 시작 */
    record_status = RECORD_ACTIVE;

    return true;
}

/*
 * 최신 이벤트 이후 센서 데이터 저장
 * 이벤트 발생 직후부터 500개 수집
 */
void event_record_update(const SensorSample *sample)
{
    if (sample == NULL ||
        record_status != RECORD_ACTIVE)
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

    /* 최신 이벤트 이후 500개 수집 완료 */
    if (post_count == POST_EVENT_SAMPLES)
    {
        record_status = RECORD_SAVED;
    }
}

/* 최신 Record 저장 완료 여부 */
bool event_record_is_saved(void)
{
    return record_status == RECORD_SAVED;
}

/* 최신 이벤트 정보 조회 */
bool event_record_get_info(EventInfo *out)
{
    if (out == NULL ||
        record_status == RECORD_EMPTY)
    {
        return false;
    }

    *out = event_info;

    return true;
}

/* 현재 기록된 전체 센서 샘플 수 */
uint32_t event_record_get_count(void)
{
    return record_count;
}

/* 저장된 센서 샘플 조회 */
bool event_record_get_sample(
    uint32_t index,
    SensorSample *out)
{
    if (out == NULL ||
        index >= record_count)
    {
        return false;
    }

    *out = record[index];

    return true;
}

/* Record 초기화 */
void event_record_clear(void)
{
    record_count = 0U;
    post_count = 0U;

    record_status = RECORD_EMPTY;

    event_info.timestamp_ms = 0U;
    event_info.event_flags = EVENT_NONE;
}
