
#include <stdio.h>
#include <assert.h>

#include "project_types.h"
#include "system_config.h"
#include "ring_buffer.h"
#include "event_record.h"

/* 10ms 간격 Mock 센서 샘플 */
static SensorSample make_sample(uint32_t index)
{
    SensorSample sample = {
        .timestamp_ms = index * SAMPLE_INTERVAL_MS,
        .vibration = (uint16_t)(index % 1000U),
        .sound_level = (uint16_t)(index % 500U),
        .light = (uint16_t)(index % 300U)};

    return sample;
}

/* 정상적인 저장 시나리오 */
static void test_full_record(void)
{
    ring_buffer_init();
    event_record_init();

    /* 이벤트 발생 이전 데이터 500개 준비 */
    for (uint32_t i = 0; i < 500U; i++)
    {
        SensorSample sample = make_sample(i);
        ring_buffer_push(&sample);
    }

    assert(ring_buffer_count() == 500U);

    /* 500번째 샘플에서 이벤트 발생 (5000ms) */
    EventInfo event = {
        .timestamp_ms = 5000U,
        .event_flags = EVENT_SHOCK};

    assert(event_record_trigger(&event));
    assert(event_record_get_count() == 500U);
    assert(!event_record_is_saved());

    /* 기록 중 새 이벤트는 거절 */
    assert(!event_record_trigger(&event));

    /* 이벤트 발생 샘플부터 500개 추가 */
    for (uint32_t i = 500U; i < 1000U; i++)
    {
        SensorSample sample = make_sample(i);

        event_record_update(&sample);
        ring_buffer_push(&sample);
    }

    assert(event_record_is_saved());
    assert(event_record_get_count() == 1000U);

    /* 이벤트 정보 확인 */
    EventInfo saved_info;
    assert(event_record_get_info(&saved_info));
    assert(saved_info.timestamp_ms == 5000U);
    assert(saved_info.event_flags == EVENT_SHOCK);

    /* 전체 1000개 데이터의 시간순서 검증 */
    for (uint32_t i = 0; i < 1000U; i++)
    {
        SensorSample result;
        assert(event_record_get_sample(i, &result));

        assert(result.timestamp_ms ==
               i * SAMPLE_INTERVAL_MS);
    }

    /* 발생 직전과 발생 시점이 구분되는지 확인 */
    SensorSample before;
    SensorSample trigger;

    assert(event_record_get_sample(499U, &before));
    assert(event_record_get_sample(500U, &trigger));

    assert(before.timestamp_ms == 4990U);
    assert(trigger.timestamp_ms == 5000U);

    /* 저장 완료 후에는 덮어쓰기 금지 */
    SensorSample extra = make_sample(1000U);

    event_record_update(&extra);

    assert(event_record_get_count() == 1000U);
    assert(!event_record_trigger(&event));

    SensorSample last;
    assert(event_record_get_sample(999U, &last));
    assert(last.timestamp_ms == 9990U);

    printf("[PASS] Full 1000-sample recording\n");
    printf("[PASS] Event information\n");
    printf("[PASS] Sample order and trigger boundary\n");
    printf("[PASS] Record protection\n");
}

/* 초기 5초 이전에 이벤트가 발생하는 경우 */
static void test_early_event(void)
{
    ring_buffer_init();
    event_record_init();

    /* 이전 데이터가 100개만 존재 */
    for (uint32_t i = 0; i < 100U; i++)
    {
        SensorSample sample = make_sample(i);
        ring_buffer_push(&sample);
    }

    EventInfo event = {
        .timestamp_ms = 1000U,
        .event_flags = EVENT_SOUND};

    assert(event_record_trigger(&event));
    assert(event_record_get_count() == 100U);

    for (uint32_t i = 100U; i < 600U; i++)
    {
        SensorSample sample = make_sample(i);

        event_record_update(&sample);
        ring_buffer_push(&sample);
    }

    assert(event_record_is_saved());
    assert(event_record_get_count() == 600U);

    SensorSample first;
    SensorSample last;

    assert(event_record_get_sample(0U, &first));
    assert(event_record_get_sample(599U, &last));

    assert(first.timestamp_ms == 0U);
    assert(last.timestamp_ms == 5990U);

    printf("[PASS] Early event recording\n");
}

/* 초기화 및 잘못된 입력 테스트 */
static void test_clear_and_invalid_input(void)
{
    ring_buffer_init();
    event_record_init();

    EventInfo info;
    SensorSample sample;

    assert(!event_record_get_info(&info));
    assert(!event_record_is_saved());
    assert(event_record_get_count() == 0U);

    assert(!event_record_trigger(NULL));
    assert(!event_record_trigger(&(EventInfo){
        .timestamp_ms = 0U,
        .event_flags = EVENT_NONE}));

    assert(!event_record_get_sample(0U, &sample));
    assert(!event_record_get_sample(0U, NULL));

    /* 복합 이벤트도 허용 */
    EventInfo event = {
        .timestamp_ms = 0U,
        .event_flags = EVENT_SHOCK | EVENT_LIGHT};

    assert(event_record_trigger(&event));

    /* NULL 업데이트는 무시 */
    event_record_update(NULL);
    assert(event_record_get_count() == 0U);

    /* 기록 중 초기화 */
    event_record_clear();

    assert(event_record_get_count() == 0U);
    assert(!event_record_get_info(&info));
    assert(!event_record_is_saved());

    /* 초기화 후 새 기록 가능 */
    assert(event_record_trigger(&event));
    event_record_clear();

    printf("[PASS] Clear and restart\n");
    printf("[PASS] Invalid input handling\n");
}

int main(void)
{
    printf("=== Event Record Test ===\n");

    test_full_record();
    test_early_event();
    test_clear_and_invalid_input();

    printf("\nAll Event Record tests passed!\n");

    return 0;
}
