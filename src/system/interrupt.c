
#include <stdint.h>
#include <stdbool.h>

#include "../../include/interrupt.h"
#include "../../include/timer.h"
#include "../../include/key_switch.h"
#include "../../src/system/timer_internal.h"
#include "../../include/nios2_ctrl_reg_macros.h"

#define TIMER_IRQ_MASK 0x1U
#define KEY_IRQ_MASK 0x2U

#define KEY_BASE 0xFF200050U

#define KEY_INTMASK_REG \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x08U))

#define KEY_EDGE_REG \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x0CU))

/*
 * KEY0 ~ KEY3 사용
 *
 * bit0: KEY0 - 경고음 중지
 * bit1: KEY1 - 경고음 및 LED/HEX 초기화
 * bit2: KEY2 - VGA 실시간 화면
 * bit3: KEY3 - VGA 과거 이벤트 화면
 *
 * 0xF = 0000 1111
 */
#define KEY_USED_MASK 0xFU

bool interrupt_init(void)
{
    NIOS2_WRITE_STATUS(0U);
    NIOS2_WRITE_IENABLE(0U);
    KEY_INTMASK_REG = 0U;

    /* KEY0~KEY3의 기존 edge capture 제거 */
    KEY_EDGE_REG = KEY_USED_MASK;

    KEY_INTMASK_REG = KEY_USED_MASK;

    return true;
}

// 활성화
void interrupt_enable(void)
{
    NIOS2_WRITE_IENABLE(
        TIMER_IRQ_MASK | KEY_IRQ_MASK);

    NIOS2_WRITE_STATUS(1U);
}

// 비활성화
void interrupt_disable(void)
{
    /* CPU 인터럽트 차단 */
    NIOS2_WRITE_STATUS(0U);
    NIOS2_WRITE_IENABLE(0U);

    /* KEY PIO 인터럽트 차단 */
    KEY_INTMASK_REG = 0U;
}

/*
 * 공통 인터럽트 처리 함수
 *
 * exception_handler.c에서 호출됨
 */
void interrupt_handler(void)
{
    uint32_t ipending;

    NIOS2_READ_IPENDING(ipending);

    /* Timer IRQ0 */
    if ((ipending & TIMER_IRQ_MASK) != 0U)
    {
        timer_irq_handler();
    }

    /* KEY IRQ1 */
    if ((ipending & KEY_IRQ_MASK) != 0U)
    {
        uint32_t edge;
        uint8_t pressed;

        edge = KEY_EDGE_REG;
        KEY_EDGE_REG = edge;

        pressed = (uint8_t)(edge & KEY_USED_MASK);

        if (pressed != 0U)
        {
            key_irq_notify(
                pressed,
                timer_get_ms());
        }
    }
}
