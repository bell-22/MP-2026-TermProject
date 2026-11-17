#ifndef KEY_SWITCH_H
#define KEY_SWITCH_H

#include <stdint.h>

/* KEY 입력 이벤트 */
#define KEY_EVENT_NONE  0U
#define KEY_EVENT_STOP  (1U << 0)  /* KEY0: 경고 해제 */
#define KEY_EVENT_CLEAR (1U << 1)  /* KEY1: 기록 초기화 */

/* KEY 입력 처리 상태 초기화 */
void key_init(void);

/* 누적된 KEY 이벤트를 가져온 후 삭제 */
uint8_t key_take_events(void);

/*
 * 공동 KEY ISR에서 호출할 이벤트 전달 함수
 *
 * pressed_mask:
 *   bit0 = KEY0 눌림
 *   bit1 = KEY1 눌림
 *
 * timestamp_ms:
 *   Timer 기반 시스템 시간(ms)
 *
 * 실제 하드웨어 IRQ 등록은 Interrupt 모듈 담당
 */
void key_irq_notify(uint8_t pressed_mask,
                    uint32_t timestamp_ms);

#endif /* KEY_SWITCH_H */