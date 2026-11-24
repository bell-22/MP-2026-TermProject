
#ifndef LIGHT_H
#define LIGHT_H

#include <stdint.h>
#include <stdbool.h>

/* 조도 센서 모듈 초기화 */
void light_init(void);

bool light_read(uint16_t *out);

#endif
