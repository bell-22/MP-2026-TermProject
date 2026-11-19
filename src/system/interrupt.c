
#include <stdint.h>
#include <stdbool.h>

#include "../../include/interrupt.h"
#include "../../include/timer.h"
#include "../../include/key_switch.h"
#include "../../src/system/timer_internal.h"
#include "../../include/nios2_ctrl_reg_macros.h"

/*
 * Nios II Interrupt Mask
 *
 * IRQ0: Timer
 * IRQ1: KEY
 */
#define TIMER_IRQ_MASK 0x1U
#define KEY_IRQ_MASK   0x2U

/*
 * DE1-SoC KEY PIO
 */
#define KEY_BASE 0xFF200050U

#define KEY_INTMASK_REG \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x08U))

#define KEY_EDGE_REG \
    (*(volatile uint32_t *)(uintptr_t)(KEY_BASE + 0x0CU))

/*
 * KEY0 / KEY1 사용
 *
 * bit0: KEY0
 * bit1: KEY1
 */
#define KEY_USED_MASK 0x3U


/*
 * 인터럽트 초기화
 */
bool interrupt_init(void)
{
    /*
     * 1. 초기화 중 CPU 인터럽트 차단
     */
    NIOS2_WRITE_STATUS(0U);
    NIOS2_WRITE_IENABLE(0U);

    /*
     * 2. KEY 인터럽트 임시 차단
     */
    KEY_INTMASK_REG = 0U;

    /*
     * 3. 이전 KEY 입력 기록 초기화
     *
     * KEY0~KEY3의 기존 edge capture 제거
     */
    KEY_EDGE_REG = 0xFU;

    /*
     * 4. KEY0 / KEY1 인터럽트 허용
     */
    KEY_INTMASK_REG = KEY_USED_MASK;

    return true;
}


/*
 * CPU 인터럽트 활성화
 */
void interrupt_enable(void)
{
    /*
     * Timer IRQ0 + KEY IRQ1 활성화
     *
     * 0x1 | 0x2 = 0x3
     */
    NIOS2_WRITE_IENABLE(
        TIMER_IRQ_MASK | KEY_IRQ_MASK
    );

    /* 전체 인터럽트 허용 (PIE = 1) */
    NIOS2_WRITE_STATUS(1U);
}


/*
 * CPU 인터럽트 비활성화
 */
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

    /*
     * 현재 발생한 인터럽트 확인
     */
    NIOS2_READ_IPENDING(ipending);


    /*
     * 1. Timer IRQ0 처리
     */
    if ((ipending & TIMER_IRQ_MASK) != 0U)
    {
        timer_irq_handler();
    }


    /*
     * 2. KEY IRQ1 처리
     */
    if ((ipending & KEY_IRQ_MASK) != 0U)
    {
        uint32_t edge;
        uint8_t pressed;

        /*
         * KEY edge capture 읽기
         */
        edge = KEY_EDGE_REG;

        /*
         * 발생한 KEY 인터럽트 기록 해제
         */
        KEY_EDGE_REG = edge;

        /*
         * KEY0 / KEY1만 사용
         */
        pressed = (uint8_t)(
            edge & KEY_USED_MASK
        );

        /*
         * 실제 KEY 입력이 있는 경우
         * key_switch.c에 이벤트 전달
         */
        if (pressed != 0U)
        {
            key_irq_notify(
                pressed,
                timer_get_ms()
            );
        }
    }
}
