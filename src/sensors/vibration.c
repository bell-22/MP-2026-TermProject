#include "../../include/vibration.h"
#include "../../include/adc.h"
#include "../../include/system_config.h"

void vibration_init(void)
{
    /* 초기화 작업 X */
}

bool vibration_read(uint16_t *out)
{
    /* 지정된 채널의 값을 읽고 *out에 저장 및 반환 */
    // vibration ADC channel = 0
    return adc_read(VIBRATION_ADC_CHANNEL, out);
}