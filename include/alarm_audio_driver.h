
#ifndef ALARM_AUDIO_DRIVER_H
#define ALARM_AUDIO_DRIVER_H

#include <stdint.h>

/*
 * Audio-out 경고음 출력 인터페이스
 *
 * frequency_hz == 0:
 *     경고음 출력 중지
 *
 * frequency_hz > 0:
 *     지정한 주파수의 경고음 출력
 *
 * 실제 DE1-SoC에서는 Audio CODEC을
 * 사용하는 드라이버가 이 함수를 구현한다.
 *
 * Mac Mock 테스트에서는 test_alarm.c가
 * 가상 구현을 제공한다.
 */
void alarm_audio_set_frequency(
    uint32_t frequency_hz
);

#endif /* ALARM_AUDIO_DRIVER_H */
