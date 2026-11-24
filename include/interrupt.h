
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdbool.h>

bool interrupt_init(void);

void interrupt_enable(void);

void interrupt_disable(void);

void interrupt_handler(void);

#endif
