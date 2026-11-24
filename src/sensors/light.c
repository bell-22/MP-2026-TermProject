#include "../../include/light.h"
#include "../../include/adc.h"
#include "../../include/system_config.h"

void light_init(void)
{
    /* 초기화 작업 X */
}

bool light_read(uint16_t *out)
{
    return adc_read(LIGHT_ADC_CHANNEL, out);
}