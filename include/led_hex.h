
#ifndef LED_HEX_H
#define LED_HEX_H

#include <stdint.h>
#include <stdbool.h>

/*
 * LED / 7-Segment 출력 모듈
 *
 * 이벤트 표시:
 *   충격 = EVENT_SHOCK (1) -> LED0
 *   소음 = EVENT_SOUND (2) -> LED1
 *   조도 = EVENT_LIGHT (4) -> LED2
 *
 * HEX 표시:
 *   정상 = 00
 *   충격 = 01
 *   소음 = 02
 *   조도 = 03
 *   복합 = 04
 */

/* LED / HEX 초기화 */
void led_hex_init(void);

/* 이벤트 발생 시 LED / HEX 표시 */
void led_hex_show(uint8_t event_flags);

/*
 * LED 점멸 상태 갱신
 *
 * 일반 이벤트 LED 점멸과
 * 초기 측정 완료 LED 점멸을 함께 처리
 */
void led_hex_update(uint32_t timestamp_ms);

/* 이벤트 LED / HEX 초기화 */
void led_hex_clear(void);


/*
 * 초기 평균 측정 완료 알림 시작
 *
 * LEDR0~9 전체를 3회 점멸
 *
 * timestamp_ms:
 *   점멸을 시작하는 시스템 시간
 */
void led_hex_ready_start(uint32_t timestamp_ms);


/*
 * 초기 측정 완료 알림 종료 여부
 *
 * true:
 *   LED 전체 점멸이 끝나 실험 시작 가능
 *
 * false:
 *   아직 시작하지 않았거나 점멸 진행 중
 */
bool led_hex_ready_is_complete(void);

#endif /* LED_HEX_H */
