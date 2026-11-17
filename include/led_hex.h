
#ifndef LED_HEX_H
#define LED_HEX_H

#include <stdint.h>  // uint8_t, uint32_t 자료형 사용

/*
 * LED / 7-Segment 출력 모듈 (연경 담당)
 *
 * 입력값은 HEX 표시 코드가 아닌 이벤트 비트 플래그다.
 *   충격 = EVENT_SHOCK (1), 소음 = EVENT_SOUND (2), 조도 = EVENT_LIGHT (4)
 *   동시 이벤트는 비트 OR로 전달한다. 예: EVENT_SHOCK | EVENT_SOUND
 */

void led_hex_init(void);                         // LED OFF, HEX 00으로 초기화
void led_hex_show(uint8_t event_flags);         // 이벤트 LED 조합 및 HEX 00~04 표시
void led_hex_update(uint32_t timestamp_ms);     // 250ms 간격 LED 점멸 갱신 (논블로킹)
void led_hex_clear(void);                        // LED OFF, HEX 00으로 초기화 (기록은 보존)

#endif // LED_HEX_H
