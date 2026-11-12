#ifndef LIGHT_H
#define LIGHT_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize light sensor */
void light_init(void);

/* Read raw light ADC value */
bool light_read(uint16_t *out);

#endif