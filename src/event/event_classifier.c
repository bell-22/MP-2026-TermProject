
#include "event_classifier.h" // 이벤트 분류 함수 선언 및 공통 이벤트 타입
#include "system_config.h"    // DISPLAY_NONE/SHOCK/SOUND/LIGHT/MULTIPLE 정의

/* 이벤트 플래그를 실제 HEX에 표시할 숫자(0~4)로 변환한다. */
uint8_t event_classify(uint8_t event_flags)
{
    // 유효한 센서 이벤트 비트만 남긴다. (bit0: 충격, bit1: 소음, bit2: 조도)
    const uint8_t valid_mask = (uint8_t)(EVENT_SHOCK | EVENT_SOUND | EVENT_LIGHT);
    const uint8_t valid_flags = (uint8_t)(event_flags & valid_mask);

    // 아무 이벤트도 발생하지 않은 경우 -> HEX 00
    if (valid_flags == EVENT_NONE) {
        return DISPLAY_NONE;
    }

    // 충격 이벤트만 발생한 경우 -> HEX 01
    if (valid_flags == EVENT_SHOCK) {
        return DISPLAY_SHOCK;
    }

    // 소음 이벤트만 발생한 경우 -> HEX 02
    if (valid_flags == EVENT_SOUND) {
        return DISPLAY_SOUND;
    }

    // 조도 변화 이벤트만 발생한 경우 -> HEX 03
    if (valid_flags == EVENT_LIGHT) {
        return DISPLAY_LIGHT;
    }

    // 나머지는 두 개 이상의 이벤트가 함께 발생한 경우 -> HEX 04
    return DISPLAY_MULTIPLE;
}
