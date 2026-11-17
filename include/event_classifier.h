
#ifndef EVENT_CLASSIFIER_H
#define EVENT_CLASSIFIER_H

#include "project_types.h"  // EVENT_NONE/SHOCK/SOUND/LIGHT, uint8_t 타입 사용

/*
 * 이벤트 비트 플래그를 HEX 표시용 코드(0~4)로 변환한다.
 *
 * 입력 event_flags:
 *   EVENT_NONE                = 0
 *   EVENT_SHOCK               = 1
 *   EVENT_SOUND               = 2
 *   EVENT_LIGHT               = 4
 *   여러 이벤트의 비트 OR 조합 = 3, 5, 6, 7
 *
 * 반환값 (system_config.h의 DISPLAY_*):
 *   정상=0, 충격=1, 소음=2, 조도=3, 복합=4
 *
 * 정의되지 않은 상위 비트는 무시한다.
 * 실제 HEX 하드웨어 출력은 수행하지 않는다.
 */
uint8_t event_classify(uint8_t event_flags);

#endif // EVENT_CLASSIFIER_H
