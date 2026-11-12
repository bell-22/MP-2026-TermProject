
#include <stdint.h>
#include <stdbool.h>

#include "timer.h"
#include "timer_internal.h"
#include "system_config.h"
#include "nios2_ctrl_reg_macros.h"

#define TIMER_BASE 0xFF202000u

#define REG32(addr) \
    (*(volatile uint32_t *)(uintptr_t)(addr))

#define TIMER_STATUS REG32(TIMER_BASE + 0x00u)
#define TIMER_CONTROL REG32(TIMER_BASE + 0x04u)
#define TIMER_LOW REG32(TIMER_BASE + 0x08u)
#define TIMER_HIGH REG32(TIMER_BASE + 0x0Cu)

/* DE1-SoC Computer 기본 Timer clock: 100MHz */
#define TIMER_CLOCK_HZ 100000000u

/* 인터럽트에서 변경되므로 volatile 사용 */
static volatile uint32_t elapsed_ms = 0;
static volatile uint32_t sample_divider = 0;
static volatile uint32_t pending_samples = 0;

bool timer_init(void)
{
    uint32_t period =
        (TIMER_CLOCK_HZ / 1000u) - 1u;

    /* Timer 정지 */
    TIMER_CONTROL = 0x8u;

    /* 이전 Timeout 제거 */
    TIMER_STATUS = 0u;

    /* 소프트웨어 시간 초기화 */
    elapsed_ms = 0;
    sample_divider = 0;
    pending_samples = 0;

    /* 1ms 주기 설정 */
    TIMER_LOW = period & 0xFFFFu;
    TIMER_HIGH = (period >> 16) & 0xFFFFu;

    /*
     * ITO   = 1 : Interrupt Enable
     * CONT  = 1 : Continuous Mode
     * START = 1 : Timer Start
     *
     * 0b0111 = 0x7
     */
    TIMER_CONTROL = 0x7u;

    return true;
}

/* Timer IRQ0 처리 */
void timer_irq_handler(void)
{
    /* Timer 인터럽트 원인 해제 */
    TIMER_STATUS = 0u;

    /* 1ms 증가 */
    elapsed_ms++;

    /* 10ms마다 샘플링 요청 */
    sample_divider++;

    if (sample_divider >= SAMPLE_INTERVAL_MS)
    {
        sample_divider = 0;

        /* 대기 요청 개수 증가 */
        if (pending_samples != UINT32_MAX)
        {
            pending_samples++;
        }
    }
}

/* 현재 경과시간 확인 */
uint32_t timer_get_ms(void)
{
    return elapsed_ms;
}

/* 10ms 샘플 요청 소비 */
bool timer_take_sample_tick(void)
{
    uint32_t saved_status;
    bool result = false;

    /*
     * ISR과 pending_samples를 공유하므로
     * 읽기-수정-쓰기 구간에서 인터럽트 보호
     */
    NIOS2_READ_STATUS(saved_status);
    NIOS2_WRITE_STATUS(saved_status & ~1u);

    if (pending_samples > 0u)
    {
        pending_samples--;
        result = true;
    }

    /* 이전 CPU 인터럽트 상태 복원 */
    NIOS2_WRITE_STATUS(saved_status);

    return result;
}
