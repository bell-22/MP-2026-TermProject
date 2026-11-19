# MP-2026-TermProject

**마이크로프로세서응용 텀프로젝트 (2026)**

- **프로젝트:** 다중 센서 기반 실시간 이벤트 감지 및 데이터 블랙박스 시스템
- **플랫폼:** DE1-SoC
- **개발 언어:** C
- **개발 환경:** Nios II 기반 시스템 / macOS Clang (단위 테스트)
- **최종 시연:** 2026-12-01

## 1. Project Overview

본 프로젝트는 DE1-SoC 보드를 이용하여 **진동·소음·조도 데이터를 실시간으로 수집하고, 이상 이벤트 발생 시 해당 시점의 전후 데이터를 자동으로 기록하는 블랙박스 시스템**을 구현하는 것을 목표로 한다.

센서별 초기 평균값(Baseline)과 변화량 임계값을 기반으로 SHOCK, SOUND, LIGHT 이벤트를 감지한다.

Ring Buffer에 최근 5초간의 센서 데이터를 유지하며, 이벤트 발생 시 이전 500개와 이후 500개의 센서 샘플을 하나의 Event Record로 저장한다. 별도의 Event History에는 최근 이벤트 최대 10건의 발생 시간과 유형을 보관한다.

이벤트 발생 시 LED/HEX를 통해 상태를 표시하고, JTAG UART로 센서 측정값 및 이벤트 기록을 확인할 수 있다. 향후 VGA 기반 모니터링 화면과 실제 스피커 경고음 출력 검증을 진행할 예정이다.

### Development Status (2026-11-19)

- 실제 DE1-SoC에서 진동·조도·소음 센서 데이터 수집 확인
- 초기 5초 Baseline 계산 및 10ms 샘플링 구조 통합
- SHOCK, SOUND, LIGHT 단일 이벤트 감지 확인
- Ring Buffer의 이전 500개 센서 샘플 조회 확인
- Event Record의 이전 500개 + 이후 500개, 총 1,000개 저장 완료 확인
- 최신 이벤트 기준 Event Record 교체 확인
- Event History 최근 10건 순환 저장 및 주기적 출력 확인
- JTAG UART 기반 센서 및 이벤트 디버깅 로그 구현
- macOS Clang / Mock MMIO 기반 기존 단위 테스트 34개 PASS (2026-11-17)

**현재 미해결 및 미검증 항목**

- 복합 이벤트 감지 미동작
- JTAG UART 출력 중 일부 문자열 누락
- Audio-out 실제 스피커 출력 미검증
- VGA 모니터링 화면 구현 예정
- 전체 1,000개 센서 샘플의 시간 간격 및 연속성 추가 검증 필요

---

## 2. Main Features

| ID | Feature | Description | Status |
|---|---|---|---|
| F1 | Sensor Monitoring | ADC 및 Audio-in 기반 진동·조도·소음 데이터 수집 | 보드 동작 확인 |
| F2 | Event Detection | Baseline 및 변화량 기반 단일 이벤트 감지 | 단일 이벤트 확인 |
| F3 | Event Recording | Ring Buffer 기반 이벤트 전후 데이터 저장 | 기본 동작 확인 |
| F4 | Event History | 최근 이벤트 10건 저장 및 조회 | 기본 동작 확인 |
| F5 | Event Classification | SHOCK / SOUND / LIGHT 및 복합 이벤트 분류 | 복합 감지 미동작 |
| F6 | Event Display | LED 및 HEX 기반 이벤트 표시 | 통합 테스트 진행 |
| F7 | User Input | KEY0 알람 정지, KEY1 LED/HEX 초기화 | 보드 동작 확인 |
| F8 | Alarm System | Audio-out 기반 이벤트별 경고음 | 스피커 미검증 |
| F9 | VGA Monitoring | 센서값, 최신 Record, History 10건 표시 | 구현 예정 |

상세 기능 및 검증 기준은 [Requirements](docs/requirements.md)를 참고한다.

---

## 3. System Specifications

| Item | Specification | Status |
|---|---|---|
| System Timer | 1ms Interrupt | 보드 실행 확인 |
| Sensor Sampling | 목표 주기 10ms | 통합 동작 확인 |
| Initial Calibration | 최초 5초 평균값 | 보드 동작 확인 |
| Vibration Input | ADC, 12-bit Raw Data | 보드 측정 확인 |
| Light Input | ADC, 12-bit Raw Data | 보드 측정 확인 |
| Sound Input | Audio-in FIFO, Peak Amplitude | 보드 측정 확인 |
| Sensor Integration | `SensorSample` | 통합 완료 |
| Event Types | SHOCK / SOUND / LIGHT / MULTIPLE | 복합 감지 미동작 |
| Ring Buffer Capacity | 500 samples | 기본 동작 확인 |
| Pre-event Data | 500 samples | 보드 출력 확인 |
| Post-event Data | 500 samples | 보드 저장 완료 확인 |
| Event Record Capacity | 1,000 samples | 보드 저장 완료 확인 |
| Stored Records | 최신 이벤트 1개 | 구현 및 동작 확인 |
| Event History | 최신 이벤트 10건 | 동작 확인 |
| Console Output | Direct JTAG UART | 출력 누락 개선 필요 |
| Event Display | LED / HEX | 통합 테스트 진행 |
| Audio-out | 이벤트 경고음 | 실제 스피커 미검증 |
| VGA Output | Sensor / Record / History | 구현 예정 |

### Sensor Data Format

센서 데이터는 `SensorSample` 구조체로 통합한다.

```c
typedef struct {
    uint32_t timestamp_ms;
    uint16_t vibration;
    uint16_t sound_level;
    uint16_t light;
} SensorSample;
```

- `timestamp_ms`: 시스템 시작 이후 경과 시간(ms)
- `vibration`: 진동 센서 ADC Raw 값
- `sound_level`: Audio-in 데이터에서 계산한 소음 특징값
- `light`: 조도 센서 ADC Raw 값

마이크의 원본 음성 데이터를 전부 기록하지 않고, 계산된 소음 특징값을 저장하여 메모리 사용량을 줄인다.

### Event Detection

이벤트 감지는 최초 5초 동안 계산한 Baseline과 현재 센서값의 절댓값 차이를 기준으로 수행한다.

주요 설정값은 `include/system_config.h`에서 관리한다.

```c
#define SHOCK_CHANGE_THRESHOLD 400U
#define SOUND_CHANGE_THRESHOLD 900U

#define EVENT_COOLDOWN_MS 1000U
#define EVENT_CONFIRM_SAMPLES 2U
```

조도 변화량 임계값은 실제 보드 측정 결과에 따라 조정 중이며, 중복 감지를 줄이기 위한 재감지 해제 조건을 적용하였다. 최종 설정값은 소스코드의 `LIGHT_CHANGE_THRESHOLD`를 기준으로 한다.

**복합 이벤트의 경우 현재 정상 동작하지 않는다.** 공통 이벤트 Flags는 비트 조합을 지원하지만, 실제 복수 센서 이벤트를 하나의 복합 이벤트로 감지하는 기능은 추가 개선이 필요하다.

---

## 4. System Architecture

```text
[Vibration Sensor] ---> [ADC] ---------+
[Light Sensor] -------> [ADC] ---------+
[Microphone] ---------> [Audio-in] ----+
                                       |
                                       v
                               [Sensor Manager]
                                       |
                                       v
                                 [SensorSample]
                                       |
                    +------------------+------------------+
                    |                                     |
                    v                                     v
              [Ring Buffer]                       [Event Detector]
             Latest 500 samples                          |
                    |                                     v
                    |                              [Event Flags]
                    |                                     |
                    |                        +------------+------------+
                    |                        |            |            |
                    |                        v            v            v
                    |                  [History]      [LED/HEX]     [Alarm]
                    |                  Latest 10                     |
                    |                                           [Audio-out]
                    |                                     (Speaker untested)
                    |
                    +----------> [Event Record] <----------+
                                  Pre 500
                                  Post 500
                                      |
                                      v
                                [JTAG UART]
                                      |
                                      v
                               [VGA Planned]

[Timer Interrupt] ---> 1ms Time / 10ms Sampling
[KEY Interrupt] ----> KEY0 Alarm Stop
                      KEY1 Alarm Stop + LED/HEX Clear
```

### Module Responsibilities

| Module | Description |
|---|---|
| ADC | 하드웨어 ADC 레지스터 및 채널별 데이터 읽기 |
| Vibration / Light | 진동·조도 센서 입력 |
| Sound | Audio FIFO 데이터 처리 및 소음 특징값 계산 |
| Sensor Manager | 센서 초기화·Polling·통합 샘플 생성 |
| Timer / Interrupt | 1ms 시스템 시간 및 10ms 샘플링 요청 |
| Event Detector | 센서별 임계값과 재감지 상태 관리 |
| Event Classifier | 이벤트 유형 분류 |
| Ring Buffer | 최근 500개 센서 데이터 순환 저장 |
| Event Record | 최신 이벤트 전후 1,000개 샘플 저장 |
| Event History | 최근 이벤트 발생 시간 및 Flags 10건 저장 |
| LED / HEX | 이벤트 종류 및 상태 표시 |
| Alarm / Audio | 이벤트 경고음 생성 및 출력 처리 |
| KEY | 알람 정지 및 LED/HEX 초기화 |
| VGA | 실시간 센서 및 저장 데이터 표시 예정 |

### Event Recording Flow

1. 센서 데이터를 약 10ms 주기로 수집한다.
2. Event Detector에서 SHOCK, SOUND, LIGHT 감지를 수행한다.
3. 이벤트가 발생하면 즉시 Event History에 시간과 Flags를 저장한다.
4. `event_record_trigger()`를 호출하여 최신 이벤트 기준 Record를 새로 생성한다.
5. Ring Buffer에 저장된 이전 500개 센서 데이터를 Record로 복사한다.
6. 이벤트 발생 샘플부터 이후 500개를 `event_record_update()`로 저장한다.
7. 이후 500개 수집이 완료되면 `RECORD_SAVED` 상태로 전환한다.
8. 새로운 이벤트 발생 시 기존 Record를 교체하고 수집을 다시 시작한다.

**중요:** 현재 이벤트 발생 샘플이 BEFORE 구간에 중복 포함되지 않도록, Record 처리 이후 Ring Buffer에 현재 샘플을 추가한다.

---

## 5. Data Storage Design

### 5.1. Ring Buffer

- 저장 용량: 500 samples
- 목표 저장 구간: 최근 5초
- 저장 방식: Circular Buffer
- 저장 시점: Calibration / Ready / Monitoring 단계 전체
- 데이터 조회: 가장 오래된 데이터부터 인덱스 순서로 접근

### 5.2. Event Record

- 저장 가능 Record: 1개
- BEFORE: 이벤트 이전 500개 샘플
- AFTER: 이벤트 발생 샘플을 포함한 이후 500개 샘플
- 전체 저장량: 1,000 samples
- 새 이벤트 발생 시 기존 Record 전체 교체
- 새 이벤트가 수집 도중 발생하면 이전 Record 수집 중단

### 5.3. Event History

- 최대 저장량: 10건
- 저장 항목: 이벤트 발생 시간, 이벤트 Flags
- 이벤트 발생 시 즉시 저장
- 최대 용량 초과 시 가장 오래된 항목 덮어쓰기
- Event Record와 독립적으로 유지
- JTAG UART에서 10초마다 History 목록 출력
- 목록 출력 순서: 오래된 이벤트 → 최근 이벤트 (시간 오름차순)

---

## 6. JTAG UART Debug Output

시스템 상태 및 센서 데이터 저장 과정을 실제 보드에서 확인하기 위해 Direct JTAG UART 출력을 사용한다.

| Output | Description | Timing |
|---|---|---|
| `[SENSOR]` | 진동·조도·소음 센서값 | 약 1초마다 |
| `[EVENT]` | 이벤트 발생 시간과 Flags | 이벤트 발생 즉시 |
| `[HISTORY]` | 현재 History 저장 개수 | 이벤트 발생 즉시 |
| `[RECORD] BEFORE` | 이벤트 이전 대표 센서 데이터 5개 | 이벤트 발생 즉시 |
| `[RECORD] AFTER` | 이벤트 이후 대표 센서 데이터 5개 | 100샘플 수집마다 |
| `[RECORD] SAVED` | 이전·이후·총 저장 데이터 개수 | 수집 완료 시 |
| `EVENT HISTORY` | 최근 이벤트 최대 10건 | 10초마다 |

Record는 실제로 1,000개 센서 샘플을 보관하며 콘솔에는 대표값만 출력한다.

기존 테스트에서 UART 출력 중 문자열 일부가 누락되는 현상이 발생했다. 전송 버퍼 처리 및 출력량에 대한 추가 개선이 필요하다.

---

## 7. Project Structure

주요 모듈을 기준으로 한 프로젝트 구성은 다음과 같다.

```text
MP-2026-TermProject/
├── src/
│   ├── main.c
│   ├── sensors/
│   │   ├── adc.c
│   │   ├── vibration.c
│   │   ├── light.c
│   │   ├── sound.c
│   │   └── sensor_manager.c
│   ├── system/
│   │   ├── timer.c
│   │   ├── interrupt.c
│   │   └── exception_handler.c
│   ├── storage/
│   │   ├── ring_buffer.c
│   │   └── event_record.c
│   └── [Event / Display / Input 모듈]
├── include/
│   ├── project_types.h
│   ├── system_config.h
│   ├── ring_buffer.h
│   ├── event_record.h
│   └── [기타 모듈 헤더]
├── tests/
├── docs/
│   ├── architecture.md
│   ├── requirements.md
│   ├── api-contract.md
│   ├── meetings/
│   └── dev-logs/
├── build/
└── README.md
```

Event History, Event Detector 및 VGA 관련 구현 파일은 현재 저장소의 실제 디렉터리 구성을 기준으로 관리한다.

---

## 8. Team Responsibilities

| Developer | Area | Responsibilities |
|---|---|---|
| **이윤아** | Sensor & Storage | ADC, 진동·조도·소음 입력, Sensor Manager, Timer/Interrupt, Ring Buffer, Event Record, Event History, 콘솔 디버깅 |
| **임연경** | Event & UI | 이벤트 감지·분류, LED/HEX, Audio-out 경고, KEY 입력, VGA 출력 추가 |
| **공동** | Integration | 공통 자료구조 및 API, Main Loop 통합, 임계값 조정, 복합 이벤트 개선, 보드 테스트 |

### VGA Development Plan

연경 담당으로 다음 기능을 추가할 예정이다.

- **Sensor Monitoring:** 실시간 진동·조도·소음 측정값 표시
- **Event Record:** 최신 이벤트 및 이전·이후 센서 데이터 표시
- **Event History:** 최근 이벤트 최대 10건 표시

기존 저장 모듈의 공개 API를 활용하여 VGA와 연동할 계획이다.

---

## 9. Development Schedule

| Period | Goal | Deliverables |
|---|---|---|
| ~11/02 | 사전 설계 및 인터페이스 정의 | 시스템 구조, 공통 자료구조, API |
| 11/03~11/09 | 개별 모듈 개발 | 센서 입력 및 기본 이벤트 감지 |
| 11/10~11/15 | 핵심 저장 기능 구현 | Ring Buffer, Event Record, ADC |
| 11/16~11/19 | 시스템 통합 및 보드 검증 | 센서 감지, Record/History 통합 테스트 |
| 11/20~11/22 | 주요 기능 보완 | 복합 이벤트 개선, VGA 구현 |
| 11/23~11/30 | 최종 통합 및 검증 | VGA·Audio-out 테스트, 오류 수정, 결과보고서 |
| **12/01** | **Final Demo** | **최종 시연 및 결과보고서 제출** |

향후 일정은 실제 개발 및 통합 진행 상황에 따라 조정할 수 있다.

---

## 10. Development Status

### Project Setup & Design

- [x] 프로젝트 주제 및 시스템 구조 설계
- [x] GitHub Repository 구성
- [x] 공통 자료구조 및 API 기본 정의
- [x] 센서 샘플링·저장 구조 설계
- [x] 실제 보드 기반 모듈 통합 테스트 착수
- [ ] 공통 API 및 문서 최종 정리

### Sensor & Storage — 이윤아

- [x] ADC 인터페이스 구현
- [x] 진동·조도 센서 입력 모듈 구현
- [x] Audio-in 기반 Sound 모듈 구현
- [x] Sensor Manager 구현
- [x] Timer / Interrupt 코드 작성 및 보드 동작 확인
- [x] 최초 5초 Baseline 계산
- [x] Ring Buffer 구현 및 기본 동작 확인
- [x] Event Record 구현 및 1,000개 저장 완료 확인
- [x] Event History 구현 및 기본 동작 확인
- [x] JTAG UART 디버깅 로그 구현
- [x] 기존 macOS 단위 테스트 34개 PASS
- [ ] UART 출력 문자열 누락 개선
- [ ] 샘플링 주기 및 전체 데이터 연속성 추가 검증

### Event & UI — 임연경

- [x] SHOCK·SOUND·LIGHT 단일 이벤트 감지 확인
- [x] 이벤트 감지 및 LED/HEX 기능 통합
- [x] KEY1을 통한 LED/HEX 초기화 확인
- [ ] 복합 이벤트 감지 로직 개선
- [ ] 실제 스피커 Audio-out 동작 검증
- [ ] VGA 실시간 센서 모니터링 구현
- [ ] VGA Event Record 표시 구현
- [ ] VGA Event History 최근 10건 표시 구현

### Integration & Final Validation

- [x] Main Loop 기반 센서·이벤트·저장 모듈 연동
- [x] 실제 DE1-SoC 보드에서 단일 이벤트 테스트
- [x] Event Record 1,000개 수집 완료 확인
- [x] Event History 주기적 출력 확인
- [ ] 복합 이벤트 동작 확인
- [ ] VGA 연동 및 화면 검증
- [ ] Audio-out 스피커 테스트
- [ ] 전체 시스템 반복 동작 테스트
- [ ] 최종 데모 및 결과보고서

---

## 11. Testing Results

### Unit Tests (2026-11-17)

macOS Clang 및 Mock MMIO 기반 테스트 결과

| Module | Tests | Result |
|---|---:|---|
| Ring Buffer | 6 | PASS |
| Event Record | 7 | PASS |
| ADC / Vibration / Light | 6 | PASS |
| Sound | 7 | PASS |
| Sensor Manager | 8 | PASS |
| **Total** | **34** | **ALL PASS** |

단위 테스트 통과는 실제 하드웨어 검증과 구분하여 관리한다.

### Hardware Integration Tests (2026-11-19)

| Test | Result |
|---|---|
| 센서값 수집 및 JTAG UART 출력 | 확인 |
| 초기 5초 Baseline 계산 | 확인 |
| SHOCK 단일 이벤트 | 확인 |
| SOUND 단일 이벤트 | 확인 |
| LIGHT 단일 이벤트 | 확인 |
| Ring Buffer 이전 500개 데이터 | 확인 |
| Event Record 총 1,000개 저장 | 확인 |
| 최신 이벤트 Record 교체 | 확인 |
| Event History 저장 및 주기적 출력 | 확인 |
| 복합 이벤트 감지 | **미동작** |
| JTAG UART 문자열 출력 완전성 | **개선 필요** |
| 실제 스피커 경고음 출력 | **미검증** |
| VGA 출력 | **구현 예정** |

---

## 12. Known Issues & Future Work

### 12.1. 복합 이벤트 감지 미동작

현재 SHOCK, SOUND, LIGHT 단일 이벤트는 감지되지만 복합 이벤트는 감지되지 않는다.

센서별 이벤트 확정 시점과 재감지 조건이 달라 동일 시점에 복수 Flags를 반환하기 어려울 가능성이 있다.

향후 복합 이벤트 판정 시간 범위와 이벤트 통합 정책을 검토하고 구현할 예정이다.

### 12.2. 센서 임계값 및 Baseline 보정

LIGHT 이벤트 반복 발생으로 조도 변화량 임계값과 재감지 조건을 조정하였다.

초기 소음 평균값은 외부 자극에 따라 크게 달라질 수 있어 추가적인 Baseline 안정화 및 센서별 보정이 필요하다.

### 12.3. JTAG UART 출력 누락

JTAG UART 전송 중 문자열 일부가 누락되는 현상이 발생한다.

출력 버퍼 및 타임아웃 처리 방식 개선과 함께 샘플링 주기에 미치는 영향을 확인할 예정이다.

### 12.4. Audio-out 및 VGA

Audio-out 초기화·호출 코드는 통합되어 있으나 실제 스피커 출력 테스트는 수행하지 않았다.

VGA에는 실시간 센서 측정값, 최신 Event Record, 최근 Event History 10건을 표시할 예정이다.

---

## 13. Documentation

| Document | Description |
|---|---|
| [Architecture](docs/architecture.md) | 전체 시스템 구조 및 모듈 구성 |
| [Requirements](docs/requirements.md) | 기능 요구사항 및 검증 기준 |
| [API Contract](docs/api-contract.md) | 모듈별 공통 인터페이스 |
| [Meeting Notes](docs/meetings/) | 팀 회의 내용 및 의사결정 |
| [Development Logs](docs/dev-logs/) | 팀원별 구현 과정 및 테스트 결과 |

---

## 14. Git Workflow

### Branch Strategy

| Branch | Purpose |
|---|---|
| `main` | 팀 공통 통합 코드 |
| `dev/yoona` | 이윤아 개인 개발 브랜치 |
| `yg` | 임연경 개발 브랜치 |

### Collaboration Rules

1. 담당 모듈은 개인 브랜치에서 개발한다.
2. 기능 구현 후 테스트 및 Commit을 수행한다.
3. `git push`로 개인 브랜치를 원격 저장소에 반영한다.
4. 팀원 간 검토 및 통합 후 `main`에 병합한다.
5. 공통 API가 변경되면 관련 헤더와 문서를 함께 갱신한다.
6. 개발 과정과 테스트 결과는 `docs/dev-logs/`에 기록한다.
7. 아직 검증되지 않은 기능은 완료 상태로 표시하지 않는다.

---

## 15. Final Validation Goals

- [x] 진동·소음·조도 데이터 수집
- [x] SHOCK·SOUND·LIGHT 단일 이벤트 감지
- [x] Ring Buffer 이전 500개 데이터 확보
- [x] Event Record 총 1,000개 저장
- [x] Event History 최근 10건 관리
- [ ] 복합 이벤트 정상 감지
- [ ] VGA 센서 데이터 표시
- [ ] VGA Event Record 데이터 표시
- [ ] VGA Event History 10건 표시
- [ ] 실제 스피커 Audio-out 경고음 검증
- [ ] JTAG UART 출력 안정화
- [ ] Timer/샘플링 주기 정밀 검증
- [ ] 연속 이벤트 및 오류 상황 처리
- [ ] 전체 시스템 반복 테스트

**Final Goal:** 준비된 시연 조건에서 센서 입력, 이벤트 감지, 데이터 기록, 화면 출력 및 경고 기능이 안정적으로 연동되는 시스템 구현

---

**Korea Aerospace University — Microprocessor Applications (2026)**