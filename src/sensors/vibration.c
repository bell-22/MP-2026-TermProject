#include "vibration.h"

#include "adc.h"
#include "system_config.h"

void vibration_init(void)
{
    /* No additional hardware initialization required */
}

bool vibration_read(uint16_t *out)
{
    return adc_read(VIBRATION_ADC_CHANNEL, out);
}