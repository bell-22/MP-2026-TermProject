
#ifndef ALARM_AUDIO_DRIVER_H
#define ALARM_AUDIO_DRIVER_H

#include <stdint.h>

/* DE1-SoC Audio-out FIFO 및 드라이버 상태 초기화 */
void alarm_audio_init(void);

/*
 * 경고음 출력 주파수 설정
 *
 * 0Hz: 경고음 중지
 * 600Hz: 복합 이벤트 낮은 음
 * 800Hz: 일반 이벤트 경고음
 * 1200Hz: 복합 이벤트 높은 음
 */
void alarm_audio_set_frequency(uint32_t frequency_hz);

/*
 * Audio-out FIFO에 실제 PCM 샘플 공급
 *
 * 경고음을 재생하는 동안 main 반복문에서
 * 지속적으로 호출해야 한다.
 */
void alarm_audio_update(void);

#endif /* ALARM_AUDIO_DRIVER_H */
