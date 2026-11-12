
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdbool.h>

/* 인터럽트 설정 초기화 */
bool interrupt_init(void);

/* 인터럽트 활성화 */
void interrupt_enable(void);

/* 인터럽트 비활성화 */
void interrupt_disable(void);

#endif
