
#ifndef EVENT_DETECTOR_H
#define EVENT_DETECTOR_H

// SensorSample 구조체와 EventType 사용
#include "project_types.h"

/*
 * 이벤트 감지 모듈 초기화 함수
 *
 * - 이전 센서값 초기화
 * - 연속 감지 횟수 초기화
 * - 재감지 대기 상태 초기화
 *
 * 프로그램 시작 시 한 번 호출한다.
 */
void event_detector_init(void);


/*
 * 센서 데이터 기반 이벤트 감지 함수
 *
 * sample:
 *   10ms 간격으로 수집한 SensorSample의 주소
 *
 * 반환값:
 *   EVENT_NONE  = 0 (정상)
 *   EVENT_SHOCK = 1 (충격)
 *   EVENT_SOUND = 2 (소음)
 *   EVENT_LIGHT = 4 (조도 변화)
 *
 * 여러 이벤트가 동시에 감지되면
 * 비트 OR 조합으로 0~7 범위의 값을 반환한다.
 *
 * NULL 입력 시 EVENT_NONE을 반환한다.
 *
 * 실제 센서 측정 및 데이터 저장은 담당하지 않는다.
 */
uint8_t event_detect(const SensorSample *sample);


#endif // EVENT_DETECTOR_H
