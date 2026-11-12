#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define TEST_ADC_BASE 0xFF204000u

static uint32_t mock_adc_registers[8];
static uint32_t mock_update_count = 0;

static uint32_t mock_mmio_read(uint32_t address)
{
    assert(address >= TEST_ADC_BASE);
    assert(address < TEST_ADC_BASE + 32u);
    assert((address - TEST_ADC_BASE) % 4u == 0u);

    return mock_adc_registers[(address - TEST_ADC_BASE) / 4u];
}

static void mock_mmio_write(uint32_t address, uint32_t value)
{
    assert(address >= TEST_ADC_BASE);
    assert(address < TEST_ADC_BASE + 32u);

    if (address == TEST_ADC_BASE)
    {
        mock_update_count++;
    }
    else
    {
        assert(address == TEST_ADC_BASE + 4u);
        (void)value;
    }
}

/* Replace hardware MMIO access with mock functions */
#define ADC_MMIO_READ(addr) mock_mmio_read(addr)
#define ADC_MMIO_WRITE(addr, value) mock_mmio_write(addr, value)

#include "../src/sensors/adc.c"

#include "vibration.h"
#include "light.h"
#include "system_config.h"

int main(void)
{
    uint16_t value = 0;

    adc_init();
    vibration_init();
    light_init();

    printf("=== Sensor Test ===\n");

    /* ADC update */
    adc_update();
    assert(mock_update_count == 1);
    printf("[PASS] ADC update\n");

    /* ADC channel read */
    mock_adc_registers[0] = 1234;
    mock_adc_registers[1] = 2345;

    assert(adc_read(0, &value));
    assert(value == 1234);

    assert(adc_read(1, &value));
    assert(value == 2345);
    printf("[PASS] ADC channel read\n");

    /* 12-bit masking */
    mock_adc_registers[2] = 0xFFFFu;

    assert(adc_read(2, &value));
    assert(value == 4095);
    printf("[PASS] ADC 12-bit masking\n");

    /* Invalid inputs */
    assert(!adc_read(8, &value));
    assert(!adc_read(0, NULL));
    printf("[PASS] Invalid ADC input\n");

    /* Vibration sensor */
    mock_adc_registers[VIBRATION_ADC_CHANNEL] = 1500;

    assert(vibration_read(&value));
    assert(value == 1500);
    printf("[PASS] Vibration sensor\n");

    /* Light sensor */
    mock_adc_registers[LIGHT_ADC_CHANNEL] = 2700;

    assert(light_read(&value));
    assert(value == 2700);
    printf("[PASS] Light sensor\n");

    printf("\nAll Sensor tests passed!\n");

    return 0;
}