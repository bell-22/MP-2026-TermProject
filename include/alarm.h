
#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>
#include <stdint.h>

/* Audio 경고 시스템 초기화 */
void alarm_init(void);

/* 이벤트 코드에 따른 경고 시작 */
void alarm_start(uint8_t event_code);

/* 시스템 시간에 따른 경고 출력 갱신 */
void alarm_update(uint32_t timestamp_ms);

/* 경고음 즉시 중지 */
void alarm_stop(void);

/*
 * 경고 패턴이 실행 중이면 true 반환
 * 경고음 사이의 무음 구간도 실행 중으로 취급
 */
bool alarm_is_active(void);

#endif /* ALARM_H */
