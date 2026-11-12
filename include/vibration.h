#ifndef VIBRATION_H
#define VIBRATION_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize vibration sensor */
void vibration_init(void);

/* Read raw vibration ADC value */
bool vibration_read(uint16_t *out);

#endif