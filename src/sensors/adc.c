#include "adc.h"

#include <stdint.h>
#include <stddef.h>

#define ADC_BASE 0xFF204000u
#define ADC_CHANNELS 8u
#define ADC_VALUE_MASK 0x0FFFu

#ifndef ADC_MMIO_READ
#define ADC_MMIO_READ(addr) \
    (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

#ifndef ADC_MMIO_WRITE
#define ADC_MMIO_WRITE(addr, value) \
    (*(volatile uint32_t *)(uintptr_t)(addr) = (value))
#endif

void adc_init(void)
{
    /* Disable auto-update: use software-triggered updates */
    ADC_MMIO_WRITE(ADC_BASE + 0x04u, 0u);
}

void adc_update(void)
{
    /* Request conversion update for all ADC channels */
    ADC_MMIO_WRITE(ADC_BASE, 0u);
}

bool adc_read(uint8_t channel, uint16_t *out)
{
    if (out == NULL || channel >= ADC_CHANNELS)
    {
        return false;
    }

    uint32_t address = ADC_BASE + ((uint32_t)channel * 4u);
    uint32_t value = ADC_MMIO_READ(address);

    *out = (uint16_t)(value & ADC_VALUE_MASK);
    return true;
}