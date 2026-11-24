
#ifndef VIBRATION_H
#define VIBRATION_H

#include <stdint.h>
#include <stdbool.h>

void vibration_init(void);

bool vibration_read(uint16_t *out);

#endif
