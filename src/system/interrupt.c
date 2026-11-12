
#include <stdint.h>
#include <stdbool.h>

#include "interrupt.h"
#include "timer_internal.h"
#include "nios2_ctrl_reg_macros.h"

#define TIMER_IRQ_MASK 0x1u

bool interrupt_init(void)
{
    /* 초기화하는 동안 전체 IRQ 차단 */
    NIOS2_WRITE_STATUS(0u);
    NIOS2_WRITE_IENABLE(0u);

    return true;
}

void interrupt_enable(void)
{
    /* 현재 단계에서는 Timer IRQ0만 활성화 */
    NIOS2_WRITE_IENABLE(TIMER_IRQ_MASK);

    /* 전체 인터럽트 허용 (PIE=1) */
    NIOS2_WRITE_STATUS(1u);
}

void interrupt_disable(void)
{
    /* 전체 인터럽트 비활성화 */
    NIOS2_WRITE_STATUS(0u);
    NIOS2_WRITE_IENABLE(0u);
}

void interrupt_handler(void)
{
    uint32_t ipending;

    NIOS2_READ_IPENDING(ipending);

    /* IRQ0: Interval Timer */
    if ((ipending & TIMER_IRQ_MASK) != 0u)
    {
        timer_irq_handler();
    }

    /*
     * IRQ1: KEY
     *
     * 연경의 KEY 모듈과 통합할 때 추가 예정.
     * 현재 단계에서는 활성화하지 않음.
     */
}
