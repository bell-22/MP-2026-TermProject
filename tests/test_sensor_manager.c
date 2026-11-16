
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "sensor_manager.h"
#include "adc.h"
#include "vibration.h"
#include "light.h"
#include "sound.h"

static int adc_init_calls;
static int vibration_init_calls;
static int light_init_calls;
static int sound_init_calls;

static int adc_update_calls;
static int sound_update_calls;
static int sound_read_calls;

static bool vibration_ok = true;
static bool light_ok = true;
static bool sound_ok = true;

static uint16_t mock_vibration = 1200;
static uint16_t mock_light = 2400;
static uint16_t mock_sound = 3500;

void adc_init(void)
{
    adc_init_calls++;
}

void adc_update(void)
{
    adc_update_calls++;
}

bool adc_read(uint8_t channel, uint16_t *out)
{
    (void)channel;
    (void)out;
    return false;
}

void vibration_init(void)
{
    vibration_init_calls++;
}

bool vibration_read(uint16_t *out)
{
    if (!vibration_ok || out == NULL)
    {
        return false;
    }
    *out = mock_vibration;
    return true;
}

void light_init(void)
{
    light_init_calls++;
}

bool light_read(uint16_t *out)
{
    if (!light_ok || out == NULL)
    {
        return false;
    }
    *out = mock_light;
    return true;
}

void sound_init(void)
{
    sound_init_calls++;
}

void sound_update(void)
{
    sound_update_calls++;
}

bool sound_read(uint16_t *out)
{
    sound_read_calls++;

    if (!sound_ok || out == NULL)
    {
        return false;
    }
    *out = mock_sound;
    return true;
}

int main(void)
{
    SensorSample sample = {0};

    printf("=== Sensor Manager Test ===\n");

    /* Test 1: initialization */
    sensor_manager_init();

    assert(adc_init_calls == 1);
    assert(vibration_init_calls == 1);
    assert(light_init_calls == 1);
    assert(sound_init_calls == 1);
    printf("[PASS] Sensor initialization\n");

    /* Test 2: audio polling */
    sensor_manager_poll();
    sensor_manager_poll();

    assert(sound_update_calls == 2);
    printf("[PASS] Audio FIFO polling\n");

    /* Test 3: integrated sample */
    assert(sensor_manager_sample(100, &sample));
    assert(sample.timestamp_ms == 100);
    assert(sample.vibration == 1200);
    assert(sample.light == 2400);
    assert(sample.sound_level == 3500);
    assert(adc_update_calls == 1);
    assert(sound_read_calls == 1);
    printf("[PASS] Integrated sensor sample\n");

    /* Test 4: timestamp update */
    assert(sensor_manager_sample(110, &sample));
    assert(sample.timestamp_ms == 110);
    assert(adc_update_calls == 2);
    printf("[PASS] Sampling timestamp update\n");

    /* Test 5: no audio available */
    sound_ok = false;

    assert(sensor_manager_sample(120, &sample));
    assert(sample.sound_level == 0);
    printf("[PASS] Empty audio handling\n");

    sound_ok = true;

    /* Test 6: invalid output pointer */
    assert(!sensor_manager_sample(130, NULL));
    assert(adc_update_calls == 3);
    printf("[PASS] NULL output handling\n");

    /* Test 7: vibration read failure */
    vibration_ok = false;
    sample.timestamp_ms = 999;

    assert(!sensor_manager_sample(140, &sample));
    assert(sample.timestamp_ms == 999);
    vibration_ok = true;
    printf("[PASS] Vibration failure handling\n");

    /* Test 8: light read failure */
    light_ok = false;
    sample.timestamp_ms = 999;

    assert(!sensor_manager_sample(150, &sample));
    assert(sample.timestamp_ms == 999);
    light_ok = true;
    printf("[PASS] Light failure handling\n");

    printf("\nAll Sensor Manager tests passed!\n");
    return 0;
}
