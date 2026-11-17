#include "../../include/sound.h"

#include <stdint.h>
#include <stddef.h>

#define AUDIO_BASE 0xFF203040u
#define AUDIO_CONTROL (AUDIO_BASE + 0x00u)
#define AUDIO_FIFOSPACE (AUDIO_BASE + 0x04u)
#define AUDIO_LEFTDATA (AUDIO_BASE + 0x08u)
#define AUDIO_RIGHTDATA (AUDIO_BASE + 0x0Cu)

#define AUDIO_CLEAR_READ 0x04u
#define AUDIO_FIFO_CAPACITY 128u

#ifndef AUDIO_MMIO_READ
#define AUDIO_MMIO_READ(addr) \
    (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

#ifndef AUDIO_MMIO_WRITE
#define AUDIO_MMIO_WRITE(addr, value) \
    (*(volatile uint32_t *)(uintptr_t)(addr) = (value))
#endif

static uint16_t peak_amplitude;
static bool has_samples;

/* Convert signed 32-bit PCM to 16-bit amplitude. */
static uint16_t sound_amplitude(uint32_t raw)
{
    int32_t sample = (int32_t)raw;

    uint32_t amplitude =
        (sample < 0)
            ? (uint32_t)(-(int64_t)sample)
            : (uint32_t)sample;

    return (uint16_t)(amplitude >> 16);
}

void sound_init(void)
{
    peak_amplitude = 0;
    has_samples = false;

    /* Clear input FIFO, then release reset. */
    AUDIO_MMIO_WRITE(AUDIO_CONTROL, AUDIO_CLEAR_READ);
    AUDIO_MMIO_WRITE(AUDIO_CONTROL, 0u);
}

void sound_update(void)
{
    uint32_t fifospace = AUDIO_MMIO_READ(AUDIO_FIFOSPACE);

    uint32_t right_count = fifospace & 0xFFu;
    uint32_t left_count = (fifospace >> 8) & 0xFFu;

    uint32_t count =
        (left_count < right_count) ? left_count : right_count;

    if (count > AUDIO_FIFO_CAPACITY)
    {
        count = AUDIO_FIFO_CAPACITY;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t left = AUDIO_MMIO_READ(AUDIO_LEFTDATA);
        uint32_t right = AUDIO_MMIO_READ(AUDIO_RIGHTDATA);

        uint16_t left_amp = sound_amplitude(left);
        uint16_t right_amp = sound_amplitude(right);

        uint16_t amplitude =
            (left_amp > right_amp) ? left_amp : right_amp;

        if (amplitude > peak_amplitude)
        {
            peak_amplitude = amplitude;
        }

        has_samples = true;
    }
}

bool sound_read(uint16_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    if (!has_samples)
    {
        *out = 0;
        return false;
    }

    *out = peak_amplitude;

    peak_amplitude = 0;
    has_samples = false;

    return true;
}