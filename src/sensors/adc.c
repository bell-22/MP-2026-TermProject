#include "../../include/adc.h"

#include <stdint.h> // 정수 자료형
#include <stddef.h> // NULL

#define ADC_BASE 0xFF204000u
#define ADC_CHANNELS 8u
#define ADC_VALUE_MASK 0x0FFFu // 하위 12비트

// MMIO 읽기 매크로
#ifndef ADC_MMIO_READ
#define ADC_MMIO_READ(addr) \
    (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

#ifndef ADC_MMIO_WRITE
#define ADC_MMIO_WRITE(addr, value) \
    (*(volatile uint32_t *)(uintptr_t)(addr) = (value))
#endif

void adc_init(void)
{
    /* 자동 갱신 모드 비활성화 */
    ADC_MMIO_WRITE(ADC_BASE + 0x04u, 0u);
}

void adc_update(void)
{
    /* ADC 주소에 0을 써서 모든 채널의 반환값 갱신 요청 */
    ADC_MMIO_WRITE(ADC_BASE, 0u);
}

// 읽기 요청의 유효성 여부를 bool 값으로 반환
bool adc_read(uint8_t channel, uint16_t *out)
{
    /* 저장할 메모리 주소 X or 채널 범위 밖 */
    if (out == NULL || channel >= ADC_CHANNELS)
    {
        return false;
    }

    /* 채널 주소 계산 및 값 읽기 */
    uint32_t address = ADC_BASE + ((uint32_t)channel * 4u);
    uint32_t value = ADC_MMIO_READ(address);

    /* 하위 12비트 추출  -> 16비트 정수로 변환  및 *out에 저장 */
    *out = (uint16_t)(value & ADC_VALUE_MASK);
    return true;
}