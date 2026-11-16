#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>

void sound_init(void);
void sound_update(void);
bool sound_read(uint16_t *out);

#endif