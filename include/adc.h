
#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

/* ADC 초기화 */
void adc_init(void);

/* 모든 채널의 변환값 갱신 요청 */
void adc_update(void);

/*
 * 지정한 ADC 채널의 최신 12비트 변환값 읽기
 * channel : 읽을 ADC 채널 번호 (0~7)
 * out     : 읽은 ADC 값을 저장할 포인터
 * 반환값  : 유효한 읽기 요청이면 true, 잘못된 인자이면 false
 */
bool adc_read(uint8_t channel, uint16_t *out);

#endif
