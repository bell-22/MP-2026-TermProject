
#include <stdio.h>
#include <assert.h>

#include "project_types.h"
#include "system_config.h"
#include "ring_buffer.h"

int main(void)
{
    SensorSample sample;
    SensorSample result;

    printf("=== Ring Buffer Test ===\n");

    /* TEST 1: 초기화 */
    ring_buffer_init();

    assert(ring_buffer_count() == 0);
    assert(!ring_buffer_get(0, &result));

    printf("[PASS] Initialization\n");

    /* TEST 2: 샘플 600개 입력 */
    for (uint32_t i = 0; i < 600; i++)
    {
        sample.timestamp_ms = i * SAMPLE_INTERVAL_MS;
        sample.vibration = (uint16_t)i;
        sample.sound_level = (uint16_t)(i + 10);
        sample.light = (uint16_t)(i + 20);

        ring_buffer_push(&sample);
    }

    /* 버퍼 크기는 최대 500개 */
    assert(ring_buffer_count() == RING_BUFFER_SIZE);

    printf("[PASS] Buffer capacity\n");

    /* TEST 3: 가장 오래된 데이터 조회 */
    assert(ring_buffer_get(0, &result));

    /*
     * 600개 입력 후 처음 100개가 덮어써짐
     * 0부터 세었으므로 첫 유효 인덱스는 100
     */
    assert(result.timestamp_ms == 1000);
    assert(result.vibration == 100);

    printf("[PASS] Oldest sample\n");

    /* TEST 4: 가장 최근 데이터 조회 */
    assert(ring_buffer_get(499, &result));

    assert(result.timestamp_ms == 5990);
    assert(result.vibration == 599);

    printf("[PASS] Latest sample\n");

    /* TEST 5: 모든 샘플 순서 확인 */
    for (uint32_t i = 0; i < RING_BUFFER_SIZE; i++)
    {
        assert(ring_buffer_get(i, &result));

        assert(result.timestamp_ms ==
               (i + 100) * SAMPLE_INTERVAL_MS);
    }

    printf("[PASS] Chronological order\n");

    /* TEST 6: 범위 및 NULL 입력 처리 */
    assert(!ring_buffer_get(500, &result));
    assert(!ring_buffer_get(0, NULL));

    ring_buffer_push(NULL);
    assert(ring_buffer_count() == RING_BUFFER_SIZE);

    printf("[PASS] Invalid input handling\n");

    printf("\nAll Ring Buffer tests passed!\n");

    return 0;
}
