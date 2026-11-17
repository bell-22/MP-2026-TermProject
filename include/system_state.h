#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "project_types.h"

/* 시스템 상태 초기화 */
void system_state_init(void);

/* 현재 시스템 상태 반환 */
SystemState system_state_get(void);

/* 유효한 시스템 상태 전환 */
void system_state_set(SystemState next);

#endif /* SYSTEM_STATE_H */