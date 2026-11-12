#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

/* ADC initialization */
void adc_init(void);

/* Trigger one conversion update for all channels */
void adc_update(void);

/* Read the latest 12-bit value from the specified channel */
bool adc_read(uint8_t channel, uint16_t *out);

#endif