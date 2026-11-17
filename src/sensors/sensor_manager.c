
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

void sensor_manager_poll(void)
{
    /* Audio FIFO must be drained more often than 10 ms. */
    sound_update();
}

bool sensor_manager_sample(uint32_t timestamp_ms,
                           SensorSample *out)
{
    if (out == NULL)
    {
        return false;
    }

    SensorSample sample = {0};
    sample.timestamp_ms = timestamp_ms;

    /* Request new ADC conversion. */
    adc_update();

    if (!vibration_read(&sample.vibration))
    {
        return false;
    }

    if (!light_read(&sample.light))
    {
        return false;
    }

    /*
     * No new audio data is not a sensor read failure.
     * Keep sound_level at zero in that case.
     */
    if (!sound_read(&sample.sound_level))
    {
        sample.sound_level = 0;
    }

    *out = sample; // out = 호출한 쪽에서 전달한 구조체 주소
    return true;   // 데이터 수집 성공
}
