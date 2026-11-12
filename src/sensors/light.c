#include "light.h"

#include "adc.h"
#include "system_config.h"

void light_init(void)
{
    /* No additional hardware initialization required */
}

bool light_read(uint16_t *out)
{
    return adc_read(LIGHT_ADC_CHANNEL, out);
}