# System Architecture

## 1. Project Overview

**Real-Time Multi-Sensor Event Detection & Blackbox System**

DE1-SoC 기반으로 진동, 소음, 조도 데이터를 수집하고 이상 이벤트를 감지하여 이벤트 발생 전후 데이터를 기록하는 시스템.

## 2. Project Structure

```text
MP-2026-TermProject/
├── src/
│   ├── main.c
│   ├── sensor/
│   │   ├── vibration.c
│   │   ├── sound.c
│   │   ├── light.c
│   │   └── sensor_filter.c
│   ├── system/
│   │   ├── timer.c
│   │   ├── interrupt.c
│   │   └── system_state.c
│   ├── event/
│   │   ├── event_detector.c
│   │   └── event_classifier.c
│   ├── storage/
│   │   ├── ring_buffer.c
│   │   └── event_record.c
│   ├── display/
│   │   ├── vga.c
│   │   ├── led_hex.c
│   │   └── alarm.c
│   └── input/
│       └── key_switch.c
├── include/
│   ├── hardware.h
│   ├── system_config.h
│   └── *.h
├── tests/
├── docs/
├── Makefile
└── README.md
```

## 3. Module Responsibilities

| Module | Description |
|---|---|
| Sensor | 진동·소음·조도 데이터 수집 및 필터링 |
| System | Timer, Interrupt, 시스템 상태 관리 |
| Event | 이상 이벤트 감지 및 복합 이벤트 분류 |
| Storage | Ring Buffer 및 이벤트 기록 |
| Display | VGA, LED, HEX, Audio 경고 출력 |
| Input | KEY, SW 입력 및 사용자 설정 |
| Main | 전체 초기화 및 모듈 실행 흐름 관리 |

## 4. Data Flow

1. Timer Interrupt로 센서 측정 시점 관리
2. ADC 및 Audio-in을 통해 센서 데이터 수집
3. 센서 데이터 필터링 및 타임스탬프 부여
4. Ring Buffer에 최근 데이터 저장
5. 이벤트 감지 및 분류
6. 이벤트 발생 전후 데이터 보존
7. VGA 및 LED·HEX에 상태 출력
8. KEY·SW 입력으로 시스템 설정 변경

## 5. Implementation Notes

- 각 모듈은 `.c`와 `.h` 파일로 분리
- 공통 하드웨어 주소는 `hardware.h`에서 관리
- 센서 측정 주기와 이벤트 임계값은 `system_config.h`에서 관리
- 모듈 간 공통 데이터 구조체를 먼저 정의
- 개발 진행에 따라 구조 변경 가능

**Status:** Initial Design (구현 전 설계안)

### 6. 설계도
Timer Interrupt (1ms)
        |
        v
Sensor Sampling (10ms)
        |
        v
Event Detection
        |
        v
새로운 이벤트 발생?
        |
        +--- NO ---> Record Post 수집
        |
        +--- YES
              |
              v
        기존 이벤트의 복합 확장?
              |
        +-----+-----+
        |           |
       YES          NO
        |           |
        v           v
    History      History
    Flags 갱신   새 항목 저장
        |           |
        v           v
    Record       Record
    Flags 갱신   새 기록 시작
        |           |
        +-----+-----+
              |
              v
        Record Post 수집
              |
              v
        Ring Buffer 갱신
              |
              v
        LED / HEX / Alarm