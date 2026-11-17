# MP-2026-TermProject

**마이크로프로세서응용 텀프로젝트 (2026)**

**프로젝트:** 다중 센서 기반 실시간 이벤트 감지 및 데이터 블랙박스 시스템  
**플랫폼:** DE1-SoC  
**개발 언어:** C  
**개발 환경:** Nios II 기반 시스템 / macOS Clang (단위 테스트)  
**최종 시연:** 2026-12-01

## 1. Project Overview

본 프로젝트는 DE1-SoC 보드를 이용하여 **진동·소음·조도 데이터를 실시간으로 수집하고, 이상 이벤트 발생 시 해당 시점의 전후 데이터를 자동으로 기록하는 블랙박스 시스템**을 구현하는 것을 목표로 한다.

센서별 임계값 및 변화량을 기반으로 충격·소음·조도 변화 이벤트를 감지하고, 여러 이벤트가 동시에 발생하는 경우 복합 이벤트로 분류한다.

Ring Buffer를 활용하여 최근 5초간의 센서 데이터를 유지하며, 이벤트 발생 시 이전 최대 5초와 이후 5초의 데이터를 보존한다. 이벤트 종류 및 시스템 상태는 LED와 7-Segment에 표시하고, Audio-out을 통해 이벤트별 경고음을 출력할 예정이다.

사용자는 KEY 입력을 통해 경고음을 해제하거나 저장된 이벤트 기록을 초기화할 수 있도록 설계하였다.

> **Development Status (2026-11-17)**
>
> - Sensor & Storage 모듈 기본 구현 완료
> - Ring Buffer, Event Record, ADC, Sound, Sensor Manager 단위 테스트 완료
> - **총 34개 단위 테스트 PASS**
> - Timer / Interrupt 코드 작성 완료 (Nios II 컴파일 미검증)
> - 실제 DE1-SoC 보드 테스트 및 팀원 모듈 통합 예정
>
> 단위 테스트는 macOS Clang 및 Mock MMIO 기반으로 수행하였다. 실제 하드웨어 동작을 검증한 결과는 아니다.

## 2. Main Features

| ID | Feature | Description |
|---|---|---|
| F1 | Sensor Monitoring | ADC 및 Audio-in 기반 진동·조도·소음 데이터 수집 |
| F2 | Event Detection | 센서별 임계값·변화량을 이용한 이벤트 감지 |
| F3 | Event Recording | Ring Buffer 기반 이벤트 전후 데이터 자동 기록 |
| F4 | Event Classification | 충격·소음·조도 변화 및 복합 이벤트 분류 |
| F5 | Event Display | LED 및 7-Segment를 이용한 이벤트 종류·상태 표시 |
| F6 | User Input | KEY0 경고 해제, KEY1 기록 초기화 |
| F7 | Alarm System | Audio-out 및 LED 점멸 기반 이벤트별 경고 |
| F8 | System Management | MONITORING / RECORDING / SAVED 상태 관리 및 오류 처리 |

상세 기능 및 검증 기준은 [Requirements](docs/requirements.md)를 참고한다.

## 3. System Specifications

| Item | Specification | Status |
|---|---|---|
| System Timer | 1ms Interrupt | 코드 작성 |
| Sensor Sampling | 10ms | 기본 로직 구현 |
| Vibration Input | ADC, 12-bit Raw Data | Mock 테스트 완료 |
| Light Input | ADC, 12-bit Raw Data | Mock 테스트 완료 |
| Sound Input | Audio-in FIFO, Peak Amplitude | Mock 테스트 완료 |
| Sensor Integration | SensorSample 구조체 | 단위 테스트 완료 |
| Data Filtering | Moving Average 등 | 추후 구현 검토 |
| Event Types | SHOCK / SOUND / LIGHT / MULTIPLE | 공통 플래그 정의 |
| Pre-event Data | 최대 5초 / 500 samples | 단위 테스트 완료 |
| Post-event Data | 500 samples (이벤트 발생 샘플 포함) | 단위 테스트 완료 |
| Ring Buffer Capacity | 500 samples | 단위 테스트 완료 |
| Event Record Capacity | 최대 1,000 samples | 단위 테스트 완료 |
| Stored Events | 1 event | 구현 완료 |
| Output | LED / HEX / Audio-out | 팀원 담당 |
| User Input | KEY Interrupt | 팀원 담당 |
| System States | MONITORING / RECORDING / SAVED | 통합 예정 |

### Sensor Data Format

센서 데이터는 10ms 주기로 `SensorSample` 구조체에 통합한다.

```c
typedef struct {
    uint32_t timestamp_ms;
    uint16_t vibration;
    uint16_t sound_level;
    uint16_t light;
} SensorSample;
```

- `timestamp_ms`: 시스템 시작 이후 경과시간 (ms)
- `vibration`: 진동 센서 ADC Raw 값
- `sound_level`: Audio-in 샘플에서 추출한 Peak Amplitude
- `light`: 조도 센서 ADC Raw 값

마이크의 원본 음성 데이터 전체를 저장하는 대신, 일정 구간에서 계산한 **음량 특징값(`sound_level`)**을 저장하여 메모리 사용량과 처리 부담을 줄인다.

샘플링 주기와 이벤트 감지 임계값은 초기 설계값이며, 실제 센서 측정 결과에 따라 조정할 수 있다.

## 4. System Architecture

```text
[Vibration Sensor] ──> [ADC] ───────┐
[Light Sensor] ──────> [ADC] ───────┤
[Microphone] ────────> [Audio-in] ──┤
                                   |
                                   v
                           [Sensor Manager]
                                   |
                                   v
                             [SensorSample]
                                   |
                         [Event Detection]
                                   |
                         [Event Classification]
                                   |
                                   v
                           [Event Recording]
                        (5s Before + 5s After)
                                   |
                                   v
                            [System Output]
                         (LED / HEX / Alarm)

[SensorSample] ──────────> [Ring Buffer]
                                  |
                                  v
                      [Pre-event Data Copy]
                                  |
                                  v
                           [Event Recording]

[Timer Interrupt] ──────> Sampling / System Time
[KEY Interrupt] ────────> Alarm Stop / Record Clear
```

### Module Responsibilities

| Module | Description |
|---|---|
| ADC | 하드웨어 ADC 레지스터 접근 및 채널별 데이터 읽기 |
| Vibration / Light | ADC 기반 센서 데이터 수집 |
| Sound | Audio FIFO 데이터 읽기 및 Peak Amplitude 계산 |
| Sensor Manager | 센서 초기화, Audio FIFO Polling, SensorSample 생성 |
| Timer / Interrupt | 1ms 시스템 시간 관리 및 10ms 샘플링 요청 |
| Ring Buffer | 최근 500개 센서 데이터 순환 저장 |
| Event Record | 이벤트 전후 센서 데이터 기록 및 보존 |
| Event Detection | 센서 데이터 기반 이벤트 감지 |
| Event Classification | 단일·복합 이벤트 분류 |
| Display / Alarm | 이벤트 정보 표시 및 경고 출력 |
| KEY / System State | 사용자 입력 및 시스템 상태 관리 |

### Event Recording Flow

1. 10ms마다 통합 센서 샘플 생성
2. 이벤트 감지 모듈에서 이상 이벤트 여부 판단
3. 최초 이벤트 발생 시 `event_record_trigger()` 호출
4. Ring Buffer에 저장된 이벤트 이전 최대 500개 샘플 복사
5. `event_record_update()`를 통해 이벤트 발생 샘플을 포함한 500개 샘플 기록
6. 이후 현재 샘플을 Ring Buffer에 저장
7. 기록 완료 시 `RECORD_SAVED` 상태로 전환
8. 기록 초기화 전까지 완료된 데이터 보존

이벤트 발생 시점의 샘플이 이전 데이터에 중복 저장되지 않도록 **Event Record 처리 이후 Ring Buffer에 현재 샘플을 추가**하는 순서를 적용한다.

전체 소프트웨어 구조 및 모듈 간 연결 방식은 [Architecture](docs/architecture.md)를 참고한다.

## 5. Project Structure

주요 구현 파일을 기준으로 한 프로젝트 구조는 다음과 같다.

```text
MP-2026-TermProject/
├── src/
│   ├── sensors/
│   │   ├── adc.c
│   │   ├── vibration.c
│   │   ├── light.c
│   │   ├── sound.c
│   │   └── sensor_manager.c
│   │
│   ├── system/
│   │   ├── timer.c
│   │   ├── interrupt.c
│   │   └── exception_handler.c
│   │
│   └── storage/
│       ├── ring_buffer.c
│       └── event_record.c
│
├── include/
│   ├── project_types.h
│   ├── system_config.h
│   ├── adc.h
│   ├── vibration.h
│   ├── light.h
│   ├── sound.h
│   ├── sensor_manager.h
│   ├── timer.h
│   ├── interrupt.h
│   ├── ring_buffer.h
│   └── event_record.h
│
├── tests/
│   ├── test_ring_buffer.c
│   ├── test_event_record.c
│   ├── test_sensor.c
│   ├── test_sound.c
│   ├── test_sensor_manager.c
│   ├── test_timer_board.c
│   └── test_sensor_board.c
│
├── docs/
│   ├── architecture.md
│   ├── requirements.md
│   ├── api-contract.md
│   ├── meetings/
│   └── dev-logs/
│
├── build/                    # Local test binaries (Git ignored)
└── README.md
```

`timer_internal.h`, `nios2_ctrl_reg_macros.h` 등 Nios II 관련 보조 헤더는 별도로 사용한다.

팀원 담당 Event / Display / Input 모듈과 최종 `main.c`는 통합 과정에서 연결한다. 위 구조는 주요 파일 중심으로 정리했으며, 실제 파일 구성과 차이가 있을 수 있다.

## 6. Team Responsibilities

본 프로젝트는 두 팀원이 독립적인 모듈을 병렬로 개발하고, 공통 API를 통해 통합하는 방식으로 진행한다.

| Developer | Area | Responsibilities |
|---|---|---|
| **이윤아** | Sensor & Storage | ADC, 진동·조도·소리 입력, Sensor Manager, Timer/Interrupt, Ring Buffer, Event Record |
| **임연경** | Event & UI | 이벤트 감지·분류, LED/HEX, Audio-out 경고, KEY/SW 입력 및 시스템 상태 관리 |
| **공동** | Integration | 공통 자료구조 및 API, `main.c`, 통합 테스트, 디버깅, 최종 보고서 |

### Independent Development

- Sensor & Storage 담당은 Mock `EventInfo`를 이용하여 이벤트 기록 기능을 독립적으로 검증한다.
- Event & UI 담당은 Mock `SensorSample`을 이용하여 이벤트 감지·분류 기능을 독립적으로 검증한다.
- 담당 모듈의 내부 전역변수에 직접 접근하지 않고 공개 API를 통해 데이터를 교환한다.
- 공통 인터페이스 변경 시 팀원 간 협의 후 수정한다.

## 7. Development Schedule

| Period | Goal | Deliverables |
|---|---|---|
| ~11/02 | 사전 설계 및 인터페이스 정의 | 시스템 구조, 공통 자료구조, API 명세 |
| 11/03~11/09 | 개별 모듈 병렬 개발 | 센서·Timer, 이벤트 감지 및 기본 출력 |
| 11/10~11/15 | 핵심 블랙박스 기능 구현 | Ring Buffer, Event Record, ADC 모듈 |
| 11/16~11/22 | 센서 통합 및 보드 검증 | Sound, Sensor Manager, Nios II 테스트 및 모듈 통합 |
| 11/23~11/30 | 최종 검증 및 결과보고서 작성 | 최종 코드, 반복 테스트 결과, 데모 준비 |
| **12/01** | **Final Demo** | **최종 시연 및 결과보고서 제출** |

## 8. Development Status

현재 개발 상태는 GitHub Commits 및 개발 로그를 기준으로 관리한다.

### Project Setup & Design

- [x] 프로젝트 주제 선정
- [x] 설계계획서 작성 및 제출
- [x] GitHub Repository 구성
- [x] 공통 자료구조 및 API 기본 정의
- [ ] 공통 API 팀원 최종 검토 및 통합 확정
- [ ] DE1-SoC 개발환경 및 Peripheral 동작 검증

### Sensor & Storage — 이윤아

- [x] Ring Buffer 구현 및 단위 테스트
- [x] Event Record 구현 및 단위 테스트
- [x] ADC 인터페이스 구현
- [x] 진동·조도 센서 입력 모듈 구현
- [x] Audio-in 기반 Sound 모듈 구현
- [x] Sensor Manager 구현
- [x] Timer / Interrupt 코드 작성
- [x] 독립 보드 테스트 코드 작성
- [x] Mac 환경 단위 테스트 34개 통과
- [ ] Timer / Interrupt Nios II 컴파일 및 실행 검증
- [ ] 실제 ADC 및 Audio 입력 검증
- [ ] 필요 시 센서 필터링 구현

### Event & UI — 임연경

- [ ] 이벤트 감지 및 분류 통합 검증
- [ ] LED / HEX 출력 통합 검증
- [ ] Audio-out 경고 통합 검증
- [ ] KEY 입력 및 상태 관리 통합 검증

팀원 담당 기능의 개별 구현 완료 여부는 해당 개발 브랜치 및 개발 로그에서 확인한다.

### Integration & Final Validation

- [ ] Sensor Manager와 Timer 연동 검증
- [ ] 이벤트 감지 및 Event Record 연결
- [ ] Main Loop 통합
- [ ] 실제 DE1-SoC 보드 테스트
- [ ] 센서 임계값 및 노이즈 보정
- [ ] 전체 시스템 반복 동작 테스트
- [ ] 최종 데모 및 결과보고서

## 9. Unit Testing

현재 Sensor & Storage 모듈은 macOS 환경에서 Clang을 사용하여 단위 테스트를 수행하였다.

하드웨어 MMIO에 직접 접근하는 모듈은 Mock 레지스터를 사용하여 검증하였다.

### Test Results (2026-11-17)

| Module | Tests | Result |
|---|---:|---|
| Ring Buffer | 6 | PASS |
| Event Record | 7 | PASS |
| ADC / Vibration / Light | 6 | PASS |
| Sound | 7 | PASS |
| Sensor Manager | 8 | PASS |
| **Total** | **34** | **ALL PASS** |

## 10. Documentation

| Document | Description |
|---|---|
| [Architecture](docs/architecture.md) | 전체 시스템 구조 및 모듈 구성 |
| [Requirements](docs/requirements.md) | 기능 요구사항 및 개발 상태 |
| [API Contract](docs/api-contract.md) | 공통 자료구조 및 함수 인터페이스 규약 |
| [Meeting Notes](docs/meetings/) | 회의 내용 및 주요 의사결정 |
| [Development Logs](docs/dev-logs/) | 팀원별 개발 과정 및 테스트 결과 |

## 11. Git Workflow

### Branch Strategy

| Branch | Purpose |
|---|---|
| `main` | 통합 및 공유 코드 |
| `dev/yoona` | 이윤아 개인 개발 브랜치 |
| `dev/yeongyeong` | 임연경 개인 개발 브랜치 |

### Collaboration Rules

1. 공통 헤더와 API 명세는 팀원 검토 후 확정한다.
2. 각자 개인 브랜치에서 담당 모듈을 개발한다.
3. 기능 구현 후 개별 테스트를 수행한다.
4. Pull Request 또는 팀원 간 검토를 통해 `main`에 병합한다.
5. API 변경 사항은 `docs/api-contract.md`에 함께 반영한다.
6. 개발 및 검증 결과는 `docs/dev-logs/`에 기록한다.
7. 하드웨어 미검증 기능은 개발 상태를 명시하여 관리한다.

## 12. Final Validation Goals

최종 데모에서는 다음 항목을 검증한다.

- [ ] 진동·소음·조도 데이터 수집 및 이벤트 감지
- [ ] 단일 이벤트와 복합 이벤트의 분류 정확성
- [ ] 이벤트 발생 이전 최대 5초 및 이후 500개 샘플 기록
- [ ] Ring Buffer의 순환 저장 및 데이터 보존
- [ ] LED·HEX 및 Audio-out 경고 동작
- [ ] KEY0 경고 해제 및 KEY1 기록 초기화
- [ ] Timer/KEY Interrupt 정상 동작
- [ ] 연속 이벤트 및 오류 상황 처리
- [ ] 주요 기능의 반복 동작 안정성
- [ ] 이벤트 임계값, 샘플링 주기 등 설정값 변경 대응

**Final Goal:** 준비된 시연 조건에서 핵심 기능을 10회 연속 정상 작동시키는 시스템 구현.

---

**Korea Aerospace University — Microprocessor Applications (2026)**
