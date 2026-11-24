
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "project_types.h"

void sensor_manager_init(void);

/*
 * Audio 입력 FIFO에 쌓인 PCM 샘플 수집
 * 메인 루프에서 반복 호출함
 */
void sensor_manager_poll(void);

/*
 * 진동, 조도, 소음값을 하나의 센서 샘플로 통합
 * 메인 루프에서 10ms 샘플링 요청을 처리할 때 호출
 *
 * timestamp_ms : 샘플에 기록할 시간(ms)
 * out          : 통합 센서 샘플을 저장할 구조체 포인터
 *
 * 반환값:
 * true  - 통합 샘플 생성 성공
 * false - 출력 포인터가 NULL이거나 진동/조도 읽기 실패
 *
 * 새 Audio 데이터가 없으면 sound_level을 0으로 설정하고
 * 나머지 센서 데이터는 정상적으로 반환
 */
bool sensor_manager_sample(uint32_t timestamp_ms,
                           SensorSample *out);

#endif
