
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "project_types.h"

/* Initialize ADC, vibration, light and sound modules. */
void sensor_manager_init(void);

/* Call frequently from the main loop to drain Audio FIFO. */
void sensor_manager_poll(void);

/* Collect one integrated sensor sample every 10 ms. */
bool sensor_manager_sample(uint32_t timestamp_ms,
                           SensorSample *out);

#endif
