# MP-2026-TermProject

**마이크로프로세서응용 텀프로젝트 (2026)**

**프로젝트:** 다중 센서 기반 실시간 이벤트 감지 및 데이터 블랙박스 시스템  
**플랫폼:** DE1-SoC  
**개발 언어:** C  
**개발 환경:** Nios II 기반 시스템 (실제 BSP 및 Peripheral 구성 확인 필요)

## 1. Project Overview

본 프로젝트는 DE1-SoC 보드를 이용하여 **진동·소음·조도 데이터를 실시간으로 수집하고, 이상 이벤트 발생 시 해당 시점의 전후 데이터를 자동으로 기록하는 블랙박스 시스템**을 구현하는 것을 목표로 한다.

센서별 임계값 및 변화량을 기반으로 충격·소음·조도 변화 이벤트를 감지하고, 여러 이벤트가 동시에 발생하는 경우 복합 이벤트로 분류한다.

Ring Buffer를 활용하여 최근 5초간의 센서 데이터를 유지하며, 이벤트 발생 시 이전 5초와 이후 5초의 데이터를 보존한다. 이벤트 종류는 LED 및 7-Segment에 표시하고, Audio-out을 통해 이벤트별 경고음을 출력한다.

사용자는 KEY 입력을 통해 경고음을 해제하거나 저장된 이벤트 기록을 초기화할 수 있다.

> **Development Status:** 설계 완료 / 구현 및 개별 모듈 테스트 진행 예정  
> 실제 하드웨어 연결 및 Peripheral 동작은 DE1-SoC 설정과 수업 제공 개발환경을 기준으로 검증한다.

## 2. Main Features

| ID | Feature | Description |
|---|---|---|
| F1 | Sensor Monitoring | ADC 및 Audio-in을 이용한 진동·조도·소음 데이터 수집 |
| F2 | Event Detection | 센서별 임계값·변화량을 이용한 이상 이벤트 감지 |
| F3 | Event Recording | Ring Buffer 기반 이벤트 전후 데이터 자동 기록 |
| F4 | Event Classification | 충격·소음·조도 변화 및 복합 이벤트 분류 |
| F5 | Event Display | LED 및 7-Segment를 이용한 이벤트 종류·상태 표시 |
| F6 | User Input | KEY0 경고 해제, KEY1 기록 초기화 |
| F7 | Alarm System | Audio-out 및 LED 점멸 기반 이벤트별 경고 |
| F8 | System Management | MONITORING / RECORDING / SAVED 상태 관리 및 오류 처리 |

상세 기능 및 검증 기준은 [Requirements](docs/requirements.md)를 참고한다.

## 3. System Specifications

| Item | Specification |
|---|---|
| System Timer | 1ms Interrupt |
| Sensor Sampling | 10ms |
| Sensor Inputs | Vibration (ADC), Light (ADC), Sound (Audio-in) |
| Data Filtering | Moving Average |
| Event Types | SHOCK / SOUND / LIGHT / MULTIPLE |
| Pre-event Data | 5 seconds / 500 samples |
| Post-event Data | 5 seconds / 500 samples |
| Ring Buffer Capacity | 500 samples |
| Event Record Capacity | 1,000 samples |
| Stored Events | 1 event |
| Output | LED / HEX / Audio-out |
| User Input | KEY Interrupt |
| System States | MONITORING / RECORDING / SAVED |

마이크의 원본 음성 데이터 전체를 저장하는 대신, 일정 구간에서 계산한 **음량 특징값(`sound_level`)**을 저장하여 메모리 사용량과 처리 부담을 줄인다.

샘플링 주기와 이벤트 감지 임계값은 초기 설계값이며, 실제 센서 측정 결과에 따라 조정할 수 있다.

## 4. System Architecture

```text
[Vibration Sensor] ─┐
[Light Sensor] ─────┼──> [ADC / Audio-in]
[Microphone] ───────┘            |
                                 v
                       [Sensor Sampling]
                                 |
                                 v
                        [Sensor Filtering]
                                 |
                                 v
                         [SensorSample]
                                 |
                    +------------+------------+
                    |                         |
                    v                         v
              [Ring Buffer]            [Event Detection]
                    |                         |
                    |                         v
                    |                [Event Classification]
                    |                         |
                    +------------+------------+
                                 |
                                 v
                         [Event Recording]
                      (5s Before + 5s After)
                                 |
                                 v
                       [LED / HEX / Audio-out]

[Timer Interrupt] ---> Sampling / System Time
[KEY Interrupt] ----> Alarm Stop / Record Clear
```

전체 소프트웨어 구조 및 모듈 간 연결 방식은 [Architecture](docs/architecture.md)를 참고한다.

## 5. Project Structure

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
│   │   ├── led_hex.c
│   │   └── alarm.c
│   └── input/
│       └── key_switch.c
│
├── include/
│   ├── project_types.h
│   ├── system_config.h
│   └── ...                  # 모듈별 API 헤더
│
├── tests/
├── docs/
│   ├── architecture.md
│   ├── requirements.md
│   ├── api-contract.md
│   ├── meetings/
│   └── dev-logs/
│
└── README.md
```

※ 위 구조는 목표 디렉터리 구성으로, 구현 진행 상황에 따라 실제 생성된 파일이 다를 수 있다.

## 6. Team Responsibilities

본 프로젝트는 두 팀원이 독립적인 모듈을 병렬로 개발하고, 공통 API를 통해 통합하는 방식으로 진행한다.

| Developer | Area | Responsibilities |
|---|---|---|
| **이윤아** | Sensor & Storage | ADC 입력, Audio-in, 센서 필터링, Timer/Interrupt, Ring Buffer, 이벤트 기록 |
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
| 11/03~11/09 | 센서 입력 및 기본 이벤트 처리 병렬 개발 | 센서·Timer 테스트, 이벤트 감지 및 기본 출력 |
| 11/10~11/15 | 핵심 블랙박스 기능 구현 및 최초 통합 | Ring Buffer, 이벤트 전후 기록, 주요 기능 통합 |
| 11/16~11/22 | 통합 디버깅 및 안정화 | 센서 보정, 기록·인터럽트·출력 검증 |
| 11/23~11/30 | 최종 검증 및 결과보고서 작성 | 최종 코드, 반복 테스트 결과, 데모 준비 |
| **12/01** | **Final Demo** | **최종 시연 및 결과보고서 제출** |

## 8. Development Status

현재 개발 상태는 GitHub의 Commits, Pull Requests 및 개발 로그를 기준으로 관리한다.

- [x] 프로젝트 주제 선정
- [x] 설계계획서 작성 및 제출
- [x] GitHub Repository 구성
- [ ] 공통 API 명세 팀원 검토 및 확정
- [ ] DE1-SoC 개발환경 및 Peripheral 동작 검증
- [ ] 센서 데이터 수집 및 필터링 구현
- [ ] Timer / Interrupt 구현
- [ ] 이벤트 감지 및 분류 구현
- [ ] Ring Buffer 및 이벤트 기록 구현
- [ ] LED / HEX / Audio 경고 구현
- [ ] KEY 입력 및 상태 관리 구현
- [ ] 전체 시스템 통합 및 반복 테스트
- [ ] 최종 데모 및 결과보고서

## 9. Documentation

| Document | Description |
|---|---|
| [Architecture](docs/architecture.md) | 전체 시스템 구조 및 모듈 구성 |
| [Requirements](docs/requirements.md) | 기능 요구사항 및 개발 상태 |
| [API Contract](docs/api-contract.md) | 공통 자료구조 및 함수 인터페이스 규약 |
| [Meeting Notes](docs/meetings/) | 회의 내용 및 주요 의사결정 |
| [Development Logs](docs/dev-logs/) | 팀원별 개발 과정 및 테스트 결과 |

## 10. Git Workflow

### Branch Strategy

| Branch | Purpose |
|---|---|
| `main` | 통합 및 검증 완료 코드 |
| `dev/yoona` | 이윤아 개인 개발 브랜치 |
| `dev/yeongyeong` | 임연경 개인 개발 브랜치 |

### Collaboration Rules

1. 공통 헤더와 API 명세는 팀원 검토 후 확정한다.
2. 각자 개인 브랜치에서 담당 모듈을 개발한다.
3. 기능 구현 후 개별 테스트를 수행한다.
4. Pull Request를 통해 코드를 검토하고 `main`에 병합한다.
5. API 변경 사항은 `docs/api-contract.md`에 함께 반영한다.
6. 개발 및 검증 결과는 `docs/dev-logs/`에 기록한다.

## 11. Final Validation Goals

최종 데모에서는 다음 항목을 검증한다.

- 진동·소음·조도 데이터 수집 및 이벤트 감지
- 단일 이벤트와 복합 이벤트의 분류 정확성
- 이벤트 발생 이전 5초 및 이후 5초 데이터 기록
- Ring Buffer의 순환 저장 및 데이터 보존
- LED·HEX 및 Audio-out 경고 동작
- KEY0 경고 해제 및 KEY1 기록 초기화
- Timer/KEY Interrupt 정상 동작
- 연속 이벤트 및 오류 상황 처리
- 주요 기능의 반복 동작 안정성
- 이벤트 임계값, 샘플링 주기 등 즉석 코드 수정 대응

**Final Goal:** 준비된 시연 조건에서 핵심 기능을 10회 연속 정상 작동시키는 시스템 구현.

---

**Korea Aerospace University — Microprocessor Applications (2026)**