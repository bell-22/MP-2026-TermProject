
#include "../../include/sensor_manager.h"

#include <stddef.h>

#include "../../include/adc.h"
#include "../../include/vibration.h"
#include "../../include/light.h"
#include "../../include/sound.h"

void sensor_manager_init(void)
{
    adc_init();
    vibration_init();
    light_init();
    sound_init();
}

/* Audio FIFO 수집 */
void sensor_manager_poll(void)
{
    sound_update();
}

bool sensor_manager_sample(uint32_t timestamp_ms,
                           SensorSample *out)
{
    if (out == NULL)
    {
        return false;
    }

    // 센서 샘플 구조체
    SensorSample sample = {0};
    // 함수 인자로 받은 시간
    sample.timestamp_ms = timestamp_ms;

    /* ADC 갱신 */
    adc_update();

    /* &sample.vibration : 샘플 구조체 내부 vibration 멤버 주소
     * 값 읽고 저장
     */
    if (!vibration_read(&sample.vibration))
    {
        return false;
    }

    if (!light_read(&sample.light))
    {
        return false;
    }

    /*
     * false 반환해도 샘플 생성 계쏙 유지하도록
     * 새로운 오디오 샘플 없으면 False 반환함 -> 진동/조도값은 보호
     */
    if (!sound_read(&sample.sound_level))
    {
        sample.sound_level = 0;
    }

    *out = sample; // out = 호출한 쪽에서 전달한 구조체 주소
    return true;
}
