#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define TEST_AUDIO_BASE 0xFF203040u

static uint32_t mock_left[128];
static uint32_t mock_right[128];

static uint32_t mock_count;
static uint32_t mock_left_index;
static uint32_t mock_right_index;
static uint32_t mock_control;
static uint32_t mock_clear_count;

static uint32_t mock_mmio_read(uint32_t address)
{
    switch (address)
    {
    case TEST_AUDIO_BASE + 0x04u:
    {
        uint32_t left_remaining =
            mock_count - mock_left_index;
        uint32_t right_remaining =
            mock_count - mock_right_index;

        return (left_remaining << 8) | right_remaining;
    }

    case TEST_AUDIO_BASE + 0x08u:
        assert(mock_left_index < mock_count);
        return mock_left[mock_left_index++];

    case TEST_AUDIO_BASE + 0x0Cu:
        assert(mock_right_index < mock_count);
        return mock_right[mock_right_index++];

    default:
        assert(false);
        return 0;
    }
}

static void mock_mmio_write(uint32_t address, uint32_t value)
{
    assert(address == TEST_AUDIO_BASE);

    mock_control = value;

    if (value == 0x04u)
    {
        ++mock_clear_count;
        mock_count = 0;
        mock_left_index = 0;
        mock_right_index = 0;
    }
}

#define AUDIO_MMIO_READ(addr) \
    mock_mmio_read(addr)

#define AUDIO_MMIO_WRITE(addr, value) \
    mock_mmio_write(addr, value)

#include "../src/sensors/sound.c"

static void mock_load(uint32_t left, uint32_t right)
{
    assert(mock_count < 128u);
    mock_left[mock_count] = left;
    mock_right[mock_count] = right;
    ++mock_count;
}

int main(void)
{
    uint16_t value = 0;

    printf("=== Sound Test ===\n");

    sound_init();
    assert(mock_clear_count == 1u);
    assert(mock_control == 0u);
    printf("[PASS] Audio FIFO initialization\n");

    assert(!sound_read(&value));
    assert(value == 0u);
    assert(!sound_read(NULL));
    printf("[PASS] Empty FIFO and NULL handling\n");

    mock_load(0x20000000u, 0x40000000u);
    sound_update();
    assert(sound_read(&value));
    assert(value == 16384u);
    printf("[PASS] Stereo amplitude extraction\n");

    mock_load(0x10000000u, 0x20000000u);
    sound_update();
    mock_load(0x60000000u, 0x30000000u);
    sound_update();

    assert(sound_read(&value));
    assert(value == 24576u);
    printf("[PASS] Peak accumulation\n");

    assert(!sound_read(&value));
    assert(value == 0u);
    printf("[PASS] Peak reset after read\n");

    mock_load(0xA0000000u, 0x10000000u);
    sound_update();

    assert(sound_read(&value));
    assert(value == 24576u);
    printf("[PASS] Negative PCM amplitude\n");

    assert(mock_left_index == mock_count);
    assert(mock_right_index == mock_count);
    printf("[PASS] FIFO consumption\n");

    printf("\nAll Sound tests passed!\n");
    return 0;
}