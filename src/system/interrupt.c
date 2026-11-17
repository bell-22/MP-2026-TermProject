
#include <stdint.h>
#include <stdbool.h>

#include "../../include/interrupt.h"
#include "../../src/system/timer_internal.h"
#include "../../include/nios2_ctrl_reg_macros.h"

/* Nios II 인터럽트 번호 */
#define TIMER_IRQ_MASK 0x1u  /* IRQ0: Timer */
#define KEY_IRQ_MASK   0x2u  /* IRQ1: KEY */

/* KEY 버튼 하드웨어 주소 */
#define KEY_BASE 0xFF200050u

#define KEY_MASK \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x08u))

#define KEY_EDGE \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x0Cu))
bool interrupt_init(void)
{
    /* 초기화하는 동안 전체 IRQ 차단 */
    NIOS2_WRITE_STATUS(0u);
    NIOS2_WRITE_IENABLE(0u);
    
    /* KEY 하드웨어 인터럽트 초기화 */
    KEY_MASK = 0u;    /* KEY 인터럽트 차단 */
    KEY_EDGE = 0xFu;  /* 이전 버튼 입력 기록 제거 */
    KEY_MASK = 0x3u;  /* KEY0, KEY1 인터럽트 허용 */

    return true;
}

void interrupt_enable(void)
{
    /* 현재 단계에서는 Timer IRQ0+ KEY IRQ1 활성화 */
    NIOS2_WRITE_IENABLE(TIMER_IRQ_MASK | KEY_IRQ_MASK);

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

    /* IRQ1: KEY0, KEY1 */
    if ((ipending & KEY_IRQ_MASK) != 0u)
    {
        /* 눌린 버튼 확인 */
        uint32_t pressed = KEY_EDGE;

        /* 인터럽트 발생 기록 해제 */
        KEY_EDGE = pressed;

        /* KEY0, KEY1의 하위 2비트만 사용 */
        pressed &= 0x3u;

        if (pressed != 0u)
        {
            /* 연경 담당 KEY 모듈에 전달 */
            key_irq_notify(
                (uint8_t)pressed,
                timer_get_ms()
            );
        }
    }
}
