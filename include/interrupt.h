
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdbool.h>

/*
 * 인터럽트 설정 초기화
 *
 * Timer IRQ0
 * KEY IRQ1 (KEY0 / KEY1)
 */
bool interrupt_init(void);

/* CPU 인터럽트 활성화 */
void interrupt_enable(void);

/* CPU 인터럽트 비활성화 */
void interrupt_disable(void);

/*
 * 공통 인터럽트 처리 함수
 *
 * exception_handler.c에서 호출
 */
void interrupt_handler(void);

#endif /* INTERRUPT_H */
