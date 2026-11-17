
#include "../../include/ring_buffer.h"
#include "../../include/system_config.h"
#include <stddef.h>

/* 고정 크기의 센서 데이터 저장 공간 */
static SensorSample buffer[RING_BUFFER_SIZE];

/* 다음 데이터를 저장할 위치 */
static uint32_t head = 0;

/* 현재 저장된 유효 데이터 개수 */
static uint32_t count = 0;

/* Ring Buffer 초기화 */
void ring_buffer_init(void)
{
    head = 0;
    count = 0;
}

/* 새로운 샘플 저장 */
void ring_buffer_push(const SensorSample *sample)
{
    if (sample == NULL)
    {
        return;
    }

    /* 현재 head 위치에 데이터 저장 */
    buffer[head] = *sample;

    /* 다음 저장 위치로 이동 */
    head = (head + 1U) % RING_BUFFER_SIZE;

    /* 버퍼가 가득 차지 않았을 때만 count 증가 */
    if (count < RING_BUFFER_SIZE)
    {
        count++;
    }
}

/* 현재 저장된 샘플 개수 반환 */
uint32_t ring_buffer_count(void)
{
    return count;
}

/* index 순서에 해당하는 샘플 조회 */
bool ring_buffer_get(uint32_t index, SensorSample *out)
{
    if (out == NULL || index >= count)
    {
        return false;
    }

    /*
     * head: 다음 저장 위치
     * count: 현재 유효 샘플 수
     *
     * 가장 오래된 샘플 위치:
     * (head + RING_BUFFER_SIZE - count) % RING_BUFFER_SIZE
     */

    uint32_t oldest =
        (head + RING_BUFFER_SIZE - count) % RING_BUFFER_SIZE;

    uint32_t position =
        (oldest + index) % RING_BUFFER_SIZE;

    *out = buffer[position];

    return true;
}
