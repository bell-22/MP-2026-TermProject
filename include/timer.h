
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

/* 1ms 하드웨어 Timer 초기화 */
bool timer_init(void);

/* 시스템 시작 후 경과시간(ms) */
uint32_t timer_get_ms(void);

/* 10ms 샘플링 요청 하나를 가져옴 */
bool timer_take_sample_tick(void);

#endif
