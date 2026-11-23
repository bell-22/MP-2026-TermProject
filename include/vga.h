
#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include "risk_level.h"

typedef enum {
    VGA_SCREEN_REALTIME = 0,
    VGA_SCREEN_HISTORY = 1
} VgaScreen;

void vga_init(void);

void vga_set_screen(VgaScreen screen);

void vga_set_baseline(uint16_t vibration_base,
                      uint16_t light_base,
                      uint16_t sound_base);

void vga_update(uint32_t now_ms, RiskLevel risk);

#endif /* VGA_H */
