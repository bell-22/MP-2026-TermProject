# API Contract

**Project:** MP-2026-TermProject  
**System:** Multi-Sensor Event Detection & Blackbox System  
**Status:** Revised — 팀원 검토 및 통합 테스트 필요  
**Last Updated:** 2026-11-27

## 1. Overview

본 문서는 DE1-SoC 기반 다중 센서 이벤트 감지 및 데이터 블랙박스 시스템의 공통 자료구조, 모듈 간 함수 인터페이스(API), 데이터 처리 순서 및 개발 규칙을 정의한다.

각 팀원은 본 문서에서 정의한 인터페이스를 기준으로 담당 모듈을 독립적으로 개발하며, 모듈 내부 구현에 직접 의존하지 않고 공개 API를 통해 데이터를 교환한다.

기존 API 계약을 유지하면서 현재 확인된 이벤트 감지, 데이터 저장, KEY 입력 및 VGA 구현 내용을 반영한다.

실제 함수 선언은 최신 공개 헤더 파일을 기준으로 하며, 기존 설계와 현재 구현이 다른 부분은 별도의 검토 사항으로 관리한다.

### 1.1 기본 시스템 규격

| 항목 | 규격 |
|---|---|
| 개발 언어 | C |
| Target | DE1-SoC / Nios II |
| Timer Interrupt | 1ms |
| 통합 센서 샘플 주기 | 10ms |
| Ring Buffer | 500 samples |
| 이벤트 이전 기록 | 최대 500 samples (약 5초) |
| 이벤트 이후 기록 | 500 samples (이벤트 발생 샘플 포함) |
| 이벤트 기록 공간 | 최대 1,000 samples |
| 저장 가능한 Event Record | 최신 1건 |
| Event History | 최근 10건 |
| 시작 상태 | CALIBRATING / READY / MONITORING |
| 기록 상태 | 기록 전 / 수집 중 / 저장 완료 |
| 위험 단계 | NORMAL / CAUTION / DANGER |
| 이벤트 출력 | LED / HEX / Audio-out |
| 데이터 조회 | VGA / JTAG UART |
| 사용자 입력 | KEY0 / KEY1 / KEY2 / KEY3 |
| VGA 갱신 주기 | 1000ms |

※ 이벤트 이후 기록은 발생 샘플을 포함하여 500개 저장한다. 따라서 정상적인 10ms 간격의 샘플 시각은 발생 시점부터 4,990ms까지이며, 이를 설계상 5초 기록 구간으로 취급한다.

※ 실제 샘플링 지연이 발생하면 500개 샘플이 정확히 5초를 의미하지 않을 수 있으므로 타임스탬프를 이용하여 검증한다.

※ 실제 Nios II 시스템의 레지스터 주소와 Peripheral 구성은 제공된 BSP 및 하드웨어 설정을 기준으로 확정한다.

### 1.2 개발 담당

| 담당 | 개발 영역 |
|---|---|
| 이윤아 | Sensor, Timer, Interrupt, Ring Buffer, Event Record |
| 임연경 | Event Detection, Classification, Risk Level, LED/HEX, Alarm, KEY, System State, VGA |
| 공동 | 공통 자료구조, 설정값, API 규약, Main Integration |

Event History 및 모듈 간 연결 기능은 담당 모듈의 구현과 공동 통합 규약을 따른다.

### 1.3 공통 개발 원칙

1. 모듈 간 데이터 교환에는 `project_types.h`의 공통 구조체를 사용한다.
2. 다른 모듈의 내부 전역변수 및 `static` 함수에 직접 접근하지 않는다.
3. 모듈 간 함수 호출은 공개 헤더에 선언된 API만 사용한다.
4. 공통 헤더 및 함수 시그니처 변경은 사전 합의 후 진행한다.
5. Peripheral 및 인터럽트 초기화는 지정된 모듈에서 한 번만 수행한다.
6. ISR에서는 인터럽트 확인·해제, 시간 갱신 및 작업 요청 전달 등 최소 작업만 수행한다.
7. 센서 측정, 이벤트 분석, 기록 및 출력 등 주요 처리는 메인 루프에서 수행한다.
8. 각 모듈은 Mock 데이터를 이용해 독립적으로 테스트할 수 있어야 한다.
9. 함수 반환값과 오류 발생 시 동작은 본 문서의 규약을 따른다.
10. API 변경 시 본 문서와 실제 헤더 파일을 함께 수정한다.

---

## 2. Common Data Types

**Header:** `include/project_types.h`  
**Owner:** 공동

### 2.1 EventType

```c
typedef enum {
    EVENT_NONE  = 0,
    EVENT_SHOCK = 1 << 0,
    EVENT_SOUND = 1 << 1,
    EVENT_LIGHT = 1 << 2
} EventType;
```

이벤트 종류는 비트 플래그로 표현한다.

| Flag | 값 | 의미 |
|---|---:|---|
| EVENT_NONE | 0 | 이벤트 없음 |
| EVENT_SHOCK | 1 | 충격 |
| EVENT_SOUND | 2 | 소음 |
| EVENT_LIGHT | 4 | 조도 변화 |
| SHOCK + SOUND | 3 | 충격 + 소음 |
| SHOCK + LIGHT | 5 | 충격 + 조도 |
| SOUND + LIGHT | 6 | 소음 + 조도 |
| SHOCK + SOUND + LIGHT | 7 | 세 이벤트의 복합 발생 |

복합 이벤트는 비트 OR 연산으로 표현한다.

### 2.2 SystemState

기존 공통 시스템 상태 계약은 다음과 같다.

```c
typedef enum {
    STATE_MONITORING = 0,
    STATE_RECORDING,
    STATE_SAVED
} SystemState;
```

| State | 의미 |
|---|---|
| STATE_MONITORING | 센서 측정 및 이벤트 감시 |
| STATE_RECORDING | 이벤트 발생 이후 데이터 기록 |
| STATE_SAVED | 기록 완료 및 보존 |

현재 main.c에서는 초기 시작 상태와 Event Record 수집 상태를 구분하여 관리한다.

기존 SystemState 자료형 및 API의 실제 사용 여부는 최신 헤더와 소스 파일을 기준으로 확인한다.

### 2.3 SensorSample

기존 공통 자료구조 선언을 유지한다.

```c
typedef struct {
    uint32_t timestamp_ms;
    uint16_t vibration;
    uint16_t sound_level;
    uint16_t light;
} SensorSample;
```

| Field | 의미 |
|---|---|
| timestamp_ms | 시스템 시작 후 경과 시간(ms) |
| vibration | 진동 ADC 원시 측정값 |
| sound_level | 오디오 구간의 음량 특징값 |
| light | 조도 ADC 원시 측정값 |

**규칙**

- 10ms마다 SensorSample 1개를 생성하는 것을 목표로 한다.
- 모든 타임스탬프는 동일한 시스템 시간을 기준으로 한다.
- sound_level은 원본 오디오 파형이 아닌 음량 특징값이다.
- 음량 계산 방식 및 정규화 범위는 실제 Audio-in 구현과 검증 결과를 기준으로 한다.
- 실패한 센서 측정값은 유효한 데이터로 처리하지 않는 것을 원칙으로 한다.
- 개별 Audio-in 조회 실패에 대한 현재 처리 방식은 추가 검토한다.
- 원본 센서 샘플은 저장 및 출력에 사용한다.
- 이벤트 감지 및 위험 단계 계산에는 감지용 복사본을 사용할 수 있다.

### 2.4 EventInfo

```c
typedef struct {
    uint32_t timestamp_ms;
    uint8_t event_flags;
} EventInfo;
```

**규칙**

- timestamp_ms는 이벤트 발생 샘플의 타임스탬프이다.
- event_flags는 이벤트 종류의 비트 조합이다.
- 복합 이벤트의 종류는 원본 비트 플래그로 보존한다.
- HEX 출력 코드는 이벤트 기록용 플래그와 구분한다.

### 2.5 EventHistoryEntry

**Header:** `include/event_history.h`

```c
typedef struct {
    uint32_t timestamp_ms;
    uint8_t event_flags;
    uint16_t delta;
} EventHistoryEntry;
```

| Field | 의미 |
|---|---|
| timestamp_ms | 이벤트 발생 시각 |
| event_flags | 이벤트 종류 |
| delta | 이벤트 발생 당시 절댓값 변화량 |

단일 이벤트는 해당 센서의 절댓값 변화량을 저장한다.

복합 이벤트는 포함된 센서들의 절댓값 변화량 중 최대값을 저장한다.

History에는 변화량의 증가 및 감소 방향을 별도로 저장하지 않는다.

### 2.6 RiskLevel

**Header:** `include/risk_level.h` 및 관련 공통 헤더

| 상태 | 의미 |
|---|---|
| RISK_NORMAL | 정상 |
| RISK_CAUTION | 주의 |
| RISK_DANGER | 위험 |

이벤트 발생 플래그와 현재 위험 단계는 서로 다른 정보이다.

### 2.7 VgaScreen

**Header:** `include/vga.h`

| 화면 | 의미 |
|---|---|
| VGA_SCREEN_REALTIME | 실시간 센서 모니터링 |
| VGA_SCREEN_HISTORY | 최근 이벤트 History |

---

## 3. System Configuration

**Header:** `include/system_config.h`  
**Owner:** 공동

### 3.1 Timer & Sampling

| Constant | Value | Description |
|---|---:|---|
| TIMER_INTERVAL_MS | 1 | Timer Interrupt 주기 |
| SAMPLE_INTERVAL_MS | 10 | 통합 센서 샘플 목표 주기 |

### 3.2 Buffer & Recording

| Constant | Value | Description |
|---|---:|---|
| PRE_EVENT_DURATION_MS | 5000 | 이벤트 이전 기록 구간 |
| POST_EVENT_DURATION_MS | 5000 | 이벤트 이후 기록 구간 |
| PRE_EVENT_SAMPLES | 500 | 이전 데이터 최대 개수 |
| POST_EVENT_SAMPLES | 500 | 이후 데이터 개수 |
| RING_BUFFER_SIZE | 500 | Ring Buffer 용량 |
| EVENT_RECORD_SIZE | 1000 | 이벤트 기록 공간 |
| MAX_EVENT_RECORDS | 1 | 최대 저장 이벤트 수 |
| EVENT_HISTORY_CAPACITY | 10 | 최근 이벤트 저장 개수 |

상수 이름은 기존 계약을 유지하며 최신 헤더와 일치하는지 확인한다.

### 3.3 Event Detection

| Constant | Value | Description |
|---|---:|---|
| BASELINE_CALIBRATION_MS | 1000 | 초기 Calibration 최소 시간 |
| SHOCK_CHANGE_THRESHOLD | 800 | 진동 감지 및 복합 판정 기준 |
| SOUND_CHANGE_THRESHOLD | 1800 | 소음 감지 및 복합 판정 기준 |
| LIGHT_CHANGE_THRESHOLD | 150 | 조도 감지 및 복합 판정 기준 |
| SOUND_RESET_CHANGE_THRESHOLD | 400 | 소음 래치 해제 변화량 |
| SOUND_RESET_DURATION_MS | 300 | 소음 래치 해제 확인 시간 |
| LIGHT_RESET_CHANGE_THRESHOLD | 75 | 조도 래치 해제 변화량 |
| LIGHT_RESET_DURATION_MS | 300 | 조도 래치 해제 확인 시간 |
| EVENT_COOLDOWN_MS | 1000 | 동일 센서 재감지 쿨다운 |
| EVENT_COMBINE_WINDOW_MS | 500 | 기존 이벤트 복합 확장 허용 시간 |
| EVENT_CONFIRM_SAMPLES | 2 | 이벤트 확정에 필요한 연속 샘플 수 |

- 복합 이벤트는 새 센서 이벤트가 확정된 순간 다른 센서의 래치 상태와 현재 변화량을 확인하여 판정한다.
- 복합 이벤트 포함 기준은 각 센서의 최초 감지 기준과 동일하다.
- 시간 차이만으로 독립적인 이벤트를 결합하지 않는다.
- 기존 Record 확장에는 최초 이벤트 발생 시각 기준 500ms 제한을 추가로 적용한다.

### 3.4 Input & Alarm

| 설정 | Value | Description |
|---|---:|---|
| KEY_DEBOUNCE_MS | 50 | KEY 디바운싱 시간 |
| LED_BLINK_INTERVAL_MS | 250 | LED 점멸 간격 |
| READY Blink Interval | 150 | READY 점멸 간격 |
| READY Blink Count | 3 | READY 점멸 횟수 |
| ALARM_DURATION_MS | 200 | 기본 경고음 출력시간 설정 |
| Normal Alarm Frequency | 800Hz | 기본 경고음 주파수 |
| Multiple Frequency 1 | 600Hz | 복합 경고음 주파수 |
| Multiple Frequency 2 | 1200Hz | 복합 경고음 주파수 |
| Multiple Gap | 100ms | 복합 경고음 간격 설정 |
| Multiple Duration | 400ms | 복합 경고음 출력시간 설정 |

경고음의 실제 반복 및 종료 동작은 alarm.c 구현을 따른다.

### 3.5 Sensor Channels

| Sensor | Input |
|---|---|
| Vibration | ADC Channel 0 |
| Light | ADC Channel 1 |
| Sound | Audio-in |

### 3.6 VGA & UART

| 항목 | 설정값 |
|---|---:|
| VGA Refresh Interval | 1000ms |
| UART Sensor Output Interval | 1000ms |
| UART History Output Interval | 10000ms |
| Record Debug Print Step | 100 samples |

VGA 및 UART 전용 설정값은 관련 구현 파일에서 관리할 수 있다.

### 3.7 Calibration Completion

초기 Calibration은 다음 조건을 모두 만족할 때 종료한다.

1. BASELINE_CALIBRATION_MS 이상 경과
2. Ring Buffer에 PRE_EVENT_SAMPLES 확보

최소 시간 설정은 현재 1000ms이다.

다만 Ring Buffer에 500개 샘플을 확보해야 하므로 정상적인 10ms 샘플링 기준 실제 Calibration은 약 5초 동안 진행된다.

설정값과 실제 동작 시간의 정합성은 추가 검토 사항으로 남긴다.

---

## 4. Sensor API — 이윤아

**기존 계약 Header:** `include/sensor.h`  
**기존 설계 Source:** `src/sensor/`

### 4.1 기존 Sensor API 계약

```c
bool sensor_init(void);
bool sensor_read(SensorSample *out);
```

위 선언은 기존 문서의 인터페이스 계약이다.

현재 main.c에서 사용하는 Sensor Manager 인터페이스와 구분하며, 기존 함수의 실제 유지 여부는 최신 헤더를 기준으로 확인한다.

### sensor_init()

**Purpose:** 센서 측정 기능 초기화

**Return**

- true: 초기화 성공
- false: 초기화 실패

**Rules**

- ADC 및 센서 입력 인터페이스를 준비한다.
- 실제 센서 초기화 방식은 하드웨어 환경에 따라 구현한다.
- Audio CODEC의 중복 초기화를 방지한다.

### sensor_read()

**Purpose:** 통합 센서 데이터 생성

**Parameter**

- out: 측정 결과를 저장할 SensorSample 포인터

**Return**

- true: 필수 센서 데이터 수집 성공
- false: NULL 인자 또는 센서 수집 실패

**Rules**

- 통합 센서 데이터는 10ms 주기로 생성한다.
- 실제 Audio-in 샘플링 주기는 별도로 관리할 수 있다.
- 각 센서의 측정값을 하나의 SensorSample로 제공한다.
- timestamp_ms를 공통 시스템 시간으로 설정한다.
- 실패 시 out의 내용을 유효한 측정값으로 사용하지 않는다.
- 이 모듈은 이벤트 발생 여부를 직접 판단하지 않는다.

### 4.2 Sensor Manager Integration

**Header:** `include/sensor_manager.h`

현재 main.c는 Sensor Manager를 통해 센서 데이터를 처리한다.

| API | 역할 |
|---|---|
| sensor_manager_init() | 센서 모듈 초기화 |
| sensor_manager_poll() | Audio-in 등 지속적인 입력 처리 |
| sensor_manager_sample(timestamp_ms, &sample) | 통합 센서 샘플 생성 |

sensor_manager_poll()은 메인 루프에서 반복 호출한다.

sensor_manager_sample()은 Timer의 샘플링 요청에 따라 호출한다.

전체 샘플 조회에 실패한 경우 해당 샘플의 이벤트 감지 및 저장 처리를 수행하지 않는다.

개별 Audio-in 데이터 조회 실패에 대한 유효성 처리 정책은 추가 검토한다.

정확한 함수 선언과 반환 자료형은 sensor_manager.h를 기준으로 한다.

---

## 5. Timer API — 이윤아

**Header:** `include/timer.h`  
**Source:** `src/system/timer.c`

```c
bool timer_init(void);
uint32_t timer_get_ms(void);
bool timer_take_sample_tick(void);
```

### timer_init()

- 1ms Timer Interrupt 동작을 설정한다.
- 초기화 성공 시 true, 실패 시 false를 반환한다.
- 실제 IRQ 활성화는 공통 초기화 순서를 따른다.

### timer_get_ms()

- 시스템 시작 후 경과한 시간(ms)을 반환한다.
- Timer ISR에서 갱신되는 시간을 기준으로 한다.
- 시간값 조회 중 동시 접근에 따른 불일치가 발생하지 않도록 구현한다.
- 32비트 시간 카운터의 오버플로 가능성을 고려한다.

### timer_take_sample_tick()

- 10ms 센서 샘플링 요청을 확인한다.
- 처리할 요청이 있으면 요청 하나를 소비하고 true를 반환한다.
- 요청이 없으면 false를 반환한다.
- 처리 지연으로 여러 요청이 쌓일 경우 카운터 방식으로 관리할 수 있다.
- 실제 센서값과 타임스탬프를 과거 시점에 측정한 것처럼 임의 생성하지 않는다.
- UART 및 VGA 출력에 따른 실제 샘플링 지연을 검증한다.

---

## 6. Interrupt API — 이윤아

**Header:** `include/interrupt.h`  
**Source:** `src/system/interrupt.c`

```c
bool interrupt_init(void);
void interrupt_enable(void);
void interrupt_disable(void);
```

### Rules

- Timer 및 KEY IRQ의 공통 초기화와 등록을 담당한다.
- 각 Interrupt Source의 확인 및 해제 처리를 수행한다.
- ISR에서 센서 측정, 이벤트 판단, Audio 출력 등의 장시간 작업을 수행하지 않는다.
- KEY 입력에 대한 요청은 입력 모듈로 전달한다.
- IRQ 설정과 ISR 구현은 실제 Nios II BSP 및 제공 예제에 맞춰 작성한다.
- 다른 모듈에서 공통 IRQ 컨트롤러를 중복 초기화하지 않는다.

### KEY Interrupt Mask

```c
#define KEY_USED_MASK 0xFU
```

KEY0부터 KEY3까지의 입력 인터럽트를 사용한다.

기존 interrupt_disable() 인터페이스는 원문 계약에 따라 유지하며 실제 구현 여부는 최신 헤더에서 확인한다.

---

## 7. Ring Buffer API — 이윤아

**Header:** `include/ring_buffer.h`  
**Source:** `src/storage/ring_buffer.c`

```c
void ring_buffer_init(void);
void ring_buffer_push(const SensorSample *sample);
uint32_t ring_buffer_count(void);
bool ring_buffer_get(uint32_t index, SensorSample *out);
```

### ring_buffer_init()

- Ring Buffer를 초기화한다.
- 유효 데이터 수를 0으로 설정한다.
- 기존 버퍼 데이터는 논리적으로 무효화한다.

### ring_buffer_push()

**Parameter**

- sample: 저장할 센서 샘플

**Rules**

- 유효한 원본 샘플을 버퍼에 추가한다.
- 버퍼 용량은 RING_BUFFER_SIZE이다.
- 버퍼가 가득 차면 가장 오래된 데이터를 덮어쓴다.
- sample == NULL이면 아무 작업도 수행하지 않는다.
- 정상 호출 시 10ms마다 샘플 하나가 추가된다.

### ring_buffer_count()

- 현재 유효한 샘플 개수를 반환한다.
- 반환 범위는 0부터 RING_BUFFER_SIZE까지이다.

### ring_buffer_get()

**Parameters**

- index: 조회할 샘플 위치
- out: 조회 결과 저장 위치

**Return**

- true: 조회 성공
- false: 인덱스 범위 오류 또는 NULL 출력 포인터

**Index Rule**

- index = 0: 가장 오래된 유효 샘플
- index = count - 1: 가장 최근 샘플

### Ring Buffer Rules

- 이벤트 감지 및 분류 기능을 직접 수행하지 않는다.
- 최신 데이터가 들어오면 기존 데이터를 순환 방식으로 갱신한다.
- Event Record는 공개 조회 API를 이용해 이전 데이터에 접근한다.
- VGA는 공개 조회 API를 이용해 최신 센서값을 조회한다.
- 버퍼 내부 저장 배열과 인덱스는 외부 모듈에 노출하지 않는다.
- 현재 샘플은 Event Record 처리 이후 Ring Buffer에 저장한다.

---

## 8. Event Record API — 이윤아

**Header:** `include/event_record.h`  
**Source:** `src/storage/event_record.c`

### 8.1 기존 공개 API 계약

```c
void event_record_init(void);
bool event_record_trigger(const EventInfo *event);
void event_record_update(const SensorSample *sample);
bool event_record_is_saved(void);
bool event_record_get_info(EventInfo *out);
uint32_t event_record_get_count(void);
bool event_record_get_sample(uint32_t index, SensorSample *out);
void event_record_clear(void);
```

### event_record_init()

- 이벤트 기록 공간을 초기화한다.
- 기록 진행 상태와 저장된 샘플 개수를 초기화한다.
- Ring Buffer 자체를 초기화하지 않는다.

### event_record_trigger()

**Purpose:** 이벤트 기록 시작

**Parameter**

- event: 감지된 이벤트 정보

**Return**

- true: 기록 시작 성공
- false: NULL 인자 또는 기록 시작 실패

**Rules**

1. Ring Buffer에서 이전 5초 데이터(최대 500개)를 복사한다.
2. 이벤트 정보와 발생시간을 저장한다.
3. 이후 데이터 수집을 위한 기록 상태를 활성화한다.
4. 이 함수는 시스템 전체 상태를 직접 변경하지 않는다.

기존 계약에서는 RECORDING 및 SAVED 상태에서 새 기록 시작을 거절하고 기존 기록을 보호하도록 정의하였다.

현재 main.c에서는 새로운 독립 이벤트가 발생하면 새 Record 생성을 요청한다.

기존 Record 보호 또는 교체 조건은 최신 구현과의 차이가 있으므로 팀원 간 추가 협의가 필요하다.

### event_record_update()

**Purpose:** 이벤트 발생 이후 센서 데이터 기록

**Parameter**

- sample: 추가할 원본 센서 샘플

**Rules**

1. 기록 진행 중일 때만 샘플을 추가한다.
2. 이벤트 발생 샘플을 이후 기록의 첫 번째 샘플로 포함한다.
3. 이후 샘플 500개가 저장되면 기록을 완료한다.
4. 기록 완료 후 일반 샘플 추가는 기존 기록에 영향을 주지 않는다.
5. sample == NULL이면 아무 작업도 수행하지 않는다.
6. 기록 완료 여부는 event_record_is_saved()로 확인한다.
7. 기록 샘플의 시간 간격을 확인해 기록 누락 여부를 검증한다.

### event_record_is_saved()

**Return**

- true: 이벤트 기록 완료
- false: 기록 전 또는 기록 진행 중

### event_record_get_info()

**Purpose:** 저장된 이벤트 정보를 복사하여 반환

**Return**

- true: 이벤트 정보 조회 성공
- false: 기록 정보가 없거나 out == NULL

**Rules**

- 이벤트 정보가 유효하면 out에 복사한다.
- 기록 모듈 내부 포인터를 반환하지 않는다.
- 기록 완료 전이라도 트리거가 성공하여 이벤트 정보가 생성된 경우 조회할 수 있다.
- 유효하지 않은 출력 포인터는 참조하지 않는다.

### event_record_get_count()

- 현재 기록 공간에 저장된 유효 샘플 수를 반환한다.
- 반환 범위는 0부터 EVENT_RECORD_SIZE까지이다.
- 기록 중에는 현재까지 저장된 개수를 반환한다.
- 기록 완료 후 저장된 개수는 최대 1,000개이다.
- 이벤트 발생 전 확보된 데이터가 500개 미만인 경우 최종 개수도 1,000개 미만일 수 있다.

### event_record_get_sample()

**Return**

- true: 조회 성공
- false: 범위 오류 또는 NULL 출력 포인터

**Index Rule**

- index = 0: 기록된 샘플 중 가장 오래된 데이터
- 이후 인덱스는 시간 순서대로 증가한다.
- 기록 진행 중에는 현재까지 저장된 샘플만 조회할 수 있다.

### event_record_clear()

- 기존 기록 정보와 저장 데이터를 논리적으로 초기화하는 인터페이스이다.
- 진행 중인 기록도 취소할 수 있다.
- 이후 새로운 이벤트 기록을 시작할 수 있다.
- Ring Buffer 초기화 여부는 Main에서 별도로 결정한다.

현재 KEY1은 event_record_clear()를 호출하지 않는다.

따라서 KEY1 입력에 의해 Event Record가 삭제되지 않는다.

기존 clear API 자체는 삭제하지 않는다.

### 8.2 Event Record Upgrade

현재 main.c의 호출 형태:

```c
event_record_upgrade(previous.timestamp_ms, flags);
```

기존 기록의 이벤트 종류를 복합 이벤트 플래그로 확장한다.

**Rules**

- 기존 이벤트 발생 시각을 유지한다.
- 기존 Pre-event 및 Post-event 데이터를 유지한다.
- 현재 수집 중인 Record의 플래그를 갱신한다.
- 실제 함수 선언은 event_record.h를 기준으로 한다.

### 8.3 Event Recording Policy

| 항목 | 현재 정책 |
|---|---|
| 이벤트 이전 데이터 | 최대 500개 |
| 이벤트 이후 데이터 | 500개 |
| 최대 기록 | 1,000개 |
| 저장 기록 | 최대 1건 |
| 복합 이벤트 확장 | 기존 기록 유지 및 플래그 갱신 |
| 기록 중 독립 이벤트 | 현재 새 기록 요청, 보호 정책 추가 협의 |
| SAVED 상태 새 이벤트 | 현재 새 기록 요청, 교체 정책 검토 |
| KEY1 | 경고음 및 표시 초기화, 기록 유지 |
| 이전 데이터 부족 | 실제 확보된 이전 데이터만 저장 |

### 8.4 Sample Boundary Rule

이벤트 발생 샘플이 이전 기록과 이후 기록에 중복 저장되지 않도록 다음 순서를 유지한다.

```c
/* 새로운 이벤트에 대한 Record 시작 또는 확장 처리 */

event_record_update(&sample);

ring_buffer_push(&sample);
```

**Important**

- 현재 샘플은 event_record_trigger() 이전에 Ring Buffer에 추가하지 않는다.
- 이벤트 발생 샘플은 이후 기록의 첫 번째 데이터이다.
- 기존 RECORDING/SAVED 보호 정책과 현재 구현의 차이를 추가 검토한다.
- 샘플 누락 여부는 실제 타임스탬프를 이용하여 검증한다.

---

## 9. Event Detector API — 임연경

**Header:** `include/event_detector.h`  
**Source:** `src/event/event_detector.c`

```c
void event_detector_init(void);

void event_detector_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base
);

uint8_t event_detect(const SensorSample *sample);
```

### event_detector_init()

- 이벤트 감지에 필요한 내부 상태를 초기화한다.
- 초기 기준값 설정 상태를 초기화한다.
- 센서별 연속 감지 카운터를 초기화한다.
- 이벤트 래치 및 재감지 대기시간 관리 상태를 초기화한다.
- SOUND 및 LIGHT 래치 해제시간 관리 상태를 초기화한다.

### event_detector_set_baseline()

**Parameters**

- vibration_base: 초기 진동 평균값
- light_base: 초기 조도 평균값
- sound_base: 초기 소음 평균값

초기 Calibration에서 계산한 센서별 평균값을 전달한다.

### event_detect()

**Parameter**

- sample: 이벤트 감지용 센서 측정 데이터

**Return**

- 이벤트 비트 플래그(EVENT_NONE부터 복합 이벤트 조합까지)

**Rules**

1. 각 센서는 초기 Calibration 평균값 대비 절댓값 변화량을 사용한다.
2. 새로운 이벤트는 임계값 이상이 연속 2회 확인되면 확정 조건을 충족한다.
3. 센서별 래치와 쿨다운 조건을 확인한다.
4. 새 이벤트가 없는 샘플에서는 EVENT_NONE을 반환한다.
5. 새 이벤트가 확정되면 다른 센서의 래치와 현재 변화량을 검사한다.
6. 다른 센서의 래치가 설정되어 있고 최초 감지 임계값 이상이면 해당 센서의 플래그를 비트 OR로 결합한다.
7. 최초 감지 임계값과 래치 해제 임계값은 구분한다.
8. 이벤트 확정 결과는 Event History 및 Event Record로 직접 저장하지 않고 Main으로 전달한다.

### 9.1 SHOCK Detection

감지 임계값: 800

변화량이 감지 임계값 미만으로 돌아오면 연속 감지 카운터 및 래치를 초기화한다.

쿨다운 확인 이후에만 새로운 이벤트 래치를 설정한다.

```c
if (state->has_event &&
    (uint32_t)(now_ms - state->last_event_ms)
        < EVENT_COOLDOWN_MS)
{
    return false;
}

state->latched = true;
state->has_event = true;
state->last_event_ms = now_ms;
```

쿨다운 중 시작된 이상 상태가 쿨다운 종료 이후까지 지속되면 필요한 조건을 만족하는 경우 새로운 이벤트로 확정할 수 있다.

### 9.2 SOUND Detection

- 감지 임계값: 1800
- 래치 해제 임계값: 400
- 래치 해제 유지시간: 300ms
- 재감지 쿨다운: 1000ms

별도의 래치 해제 조건을 사용한다.

### 9.3 LIGHT Detection

- 감지 임계값: 150
- 래치 해제 임계값: 75
- 래치 해제 유지시간: 300ms
- 재감지 쿨다운: 1000ms

감지 임계값과 래치 해제 임계값을 분리한다.

### 9.4 Multiple Event Detection

새로운 이벤트가 확정된 순간 다른 센서의 래치 상태와 현재 변화량을 확인한다.

시간 차이만으로 이벤트를 복합 이벤트로 판정하지 않는다.

500ms 확장 시간 제한은 Event Detector 내부가 아닌 main.c의 기존 Record 확장 과정에서 적용한다.

---

## 10. Event Classifier API — 임연경

**Header:** `include/event_classifier.h`  
**Source:** `src/event/event_classifier.c`

```c
uint8_t event_classify(uint8_t event_flags);
```

### Purpose

이벤트 비트 플래그를 HEX 출력 및 경고음 제어용 이벤트 코드로 변환한다.

### Return Rule

| Event Flags | Display Code |
|---|---:|
| EVENT_NONE | 0 |
| EVENT_SHOCK | 1 |
| EVENT_SOUND | 2 |
| EVENT_LIGHT | 3 |
| 복합 이벤트 | 4 |

### Rules

- 이벤트 플래그에 2개 이상의 유효한 이벤트가 설정되면 코드 4를 반환한다.
- 정의되지 않은 비트는 무시하는 기존 계약을 유지한다.
- 이벤트 정보 저장에는 HEX 코드가 아닌 원본 비트 플래그를 사용한다.
- 이 모듈은 실제 HEX 레지스터를 제어하지 않는다.

---

## 11. LED / HEX API — 임연경

**Header:** `include/led_hex.h`  
**Source:** `src/display/led_hex.c`

```c
void led_hex_init(void);
void led_hex_show(uint8_t event_flags);
void led_hex_update(uint32_t timestamp_ms);
void led_hex_clear(void);
```

### led_hex_init()

- LED와 HEX 출력 기능을 초기화한다.
- 초기 출력 상태는 정상 상태로 설정한다.

### led_hex_show()

**Parameter**

- event_flags: 이벤트 비트 플래그

**Rules**

- 입력받은 이벤트 플래그를 기준으로 LED 및 HEX 출력을 결정한다.
- HEX는 event_classify()의 반환값인 0~4를 표시한다.
- LED0은 충격, LED1은 소음, LED2는 조도 이벤트를 나타낸다.
- 복합 이벤트 발생 시 해당하는 LED를 함께 점등 또는 점멸한다.
- event_flags의 유효하지 않은 비트는 무시하는 기존 계약을 유지한다.

**Example**

| Event | HEX | LED |
|---|---|---|
| SHOCK | 01 | LED0 |
| SOUND | 02 | LED1 |
| LIGHT | 03 | LED2 |
| SHOCK + SOUND | 04 | LED0 + LED1 |
| SHOCK + LIGHT | 04 | LED0 + LED2 |
| SOUND + LIGHT | 04 | LED1 + LED2 |
| 세 센서 복합 | 04 | LED0 + LED1 + LED2 |

### led_hex_update()

- timestamp_ms를 이용해 LED 점멸 간격을 관리한다.
- 메인 루프에서 반복 호출한다.
- 호출 시 장시간 대기하거나 블로킹하지 않는 것을 원칙으로 한다.

### led_hex_clear()

- LED를 끄고 HEX를 정상 상태(00)로 설정한다.
- 저장된 이벤트 기록은 변경하지 않는다.

### READY Signal

초기 Calibration 완료 이후 READY 표시를 수행한다.

- 점멸 간격: 150ms
- 점멸 횟수: 3회

READY 관련 추가 API의 정확한 선언은 최신 led_hex.h를 따른다.

---

## 12. Alarm API — 임연경

**Header:** `include/alarm.h`  
**Source:** `src/display/alarm.c`

```c
void alarm_init(void);
void alarm_start(uint8_t event_code);
void alarm_update(uint32_t timestamp_ms);
void alarm_stop(void);
bool alarm_is_active(void);
```

### alarm_init()

- Audio-out 경고 기능을 초기화한다.
- 실제 Audio CODEC 설정은 공통 하드웨어 초기화 규약에 따른다.

### alarm_start()

**Parameter**

- event_code: event_classify()가 반환한 0~4 코드

**기존 경고 패턴 계약**

| Code | Alarm Pattern |
|---|---|
| 0 | 경고음 시작하지 않음 |
| 1 | 짧은 경고음 1회 |
| 2 | 짧은 경고음 2회 |
| 3 | 짧은 경고음 3회 |
| 4 | 별도 복합 이벤트 경고 패턴 |

실제 반복 횟수와 자동 종료 동작은 최신 alarm.c 및 보드 테스트 결과를 확인한다.

### alarm_update()

- 시스템 시간에 따라 경고음 시작, 종료 및 반복 상태를 관리한다.
- 메인 루프에서 반복 호출한다.
- 경고음 출력 중에도 센서 측정과 인터럽트 처리가 가능하도록 구성한다.

### alarm_stop()

- 현재 경고 출력을 중지한다.
- KEY0 및 KEY1 경고 해제 요청과 연동한다.
- 이벤트 기록 데이터는 삭제하지 않는다.

### alarm_is_active()

- 현재 경고 기능이 활성 상태이면 true를 반환한다.
- 그렇지 않으면 false를 반환한다.

현재 감지용 SOUND 값 대체 정책은 이 함수의 반환값을 사용한다.

### Audio Access Rule

- Audio-in과 Audio-out은 공통 Audio CODEC 설정을 사용할 수 있다.
- 동일한 Audio CODEC을 여러 모듈이 중복 초기화하지 않는다.
- CODEC 초기화 담당 및 입출력 FIFO 접근 규칙은 실제 보드 환경을 기준으로 공동 확정한다.
- 실제 Audio 출력은 메인 루프에서 처리한다.

---

## 13. KEY API — 임연경

**Header:** `include/key_switch.h`  
**Source:** `src/input/key_switch.c`

```c
#define KEY_EVENT_NONE         0U
#define KEY_EVENT_STOP         (1U << 0)
#define KEY_EVENT_CLEAR        (1U << 1)
#define KEY_EVENT_VIEW_LIVE    (1U << 2)
#define KEY_EVENT_VIEW_HISTORY (1U << 3)

void key_init(void);
uint8_t key_take_events(void);
```

### key_init()

- KEY 입력 처리 상태를 초기화한다.
- 실제 IRQ 등록과 공통 인터럽트 활성화는 Interrupt 모듈 규약을 따른다.
- KEY0부터 KEY3까지의 디바운싱 상태를 준비한다.

### key_take_events()

**Return**

| Event | 의미 |
|---|---|
| KEY_EVENT_NONE | 처리할 KEY 입력 없음 |
| KEY_EVENT_STOP | KEY0 경고 해제 요청 |
| KEY_EVENT_CLEAR | KEY1 경고 및 표시 초기화 요청 |
| KEY_EVENT_VIEW_LIVE | KEY2 실시간 화면 선택 |
| KEY_EVENT_VIEW_HISTORY | KEY3 History 화면 선택 |

여러 이벤트가 누적되면 비트 OR 조합으로 반환한다.

### Rules

- KEY ISR에서는 입력 이벤트를 기록하고 실제 기능을 실행하지 않는다.
- key_take_events()는 누적된 이벤트를 반환한 뒤 해당 요청을 소비한다.
- KEY0부터 KEY3까지 각각 독립적으로 50ms 디바운싱을 적용한다.
- 경고 종료, 표시 초기화 및 VGA 화면 선택은 메인 루프에서 처리한다.

### Main Processing Rule

```c
uint8_t keys = key_take_events();

if (keys & KEY_EVENT_STOP)
{
    alarm_stop();
}

if (keys & KEY_EVENT_CLEAR)
{
    alarm_stop();
    led_hex_clear();
}

if (keys & KEY_EVENT_VIEW_LIVE)
{
    vga_set_screen(VGA_SCREEN_REALTIME);
}

if (keys & KEY_EVENT_VIEW_HISTORY)
{
    vga_set_screen(VGA_SCREEN_HISTORY);
}
```

위 코드는 KEY별 기능을 설명하기 위한 연동 예시이다.

### KEY Policy

| KEY | 기능 |
|---|---|
| KEY0 | 경고음만 중지 |
| KEY1 | 경고음 중지, LED OFF, HEX 00 |
| KEY2 | SENSOR MONITOR 화면 |
| KEY3 | LAST EVENT 화면 |

KEY1은 Ring Buffer, Event Record 및 Event History를 삭제하지 않는다.

KEY2 및 KEY3은 데이터 조회 화면만 선택한다.

---

## 14. System State API — 임연경

**기존 계약 Header:** `include/system_state.h`  
**기존 설계 Source:** `src/system/system_state.c`

### 14.1 기존 System State API

```c
void system_state_init(void);
SystemState system_state_get(void);
void system_state_set(SystemState next);
```

### system_state_init()

기존 계약에서는 시스템 상태를 STATE_MONITORING으로 초기화한다.

### system_state_get()

현재 시스템 상태를 반환한다.

### system_state_set()

- 요청된 상태 전환의 유효성을 검사한다.
- 유효한 상태 전환만 적용한다.
- 유효하지 않은 요청은 무시한다.
- 상태 전환은 메인 루프에서 처리한다.

### 14.2 기존 상태 전환 계약

| Current State | Next State | 기존 설계 조건 |
|---|---|---|
| MONITORING | RECORDING | 이벤트 기록 시작 성공 |
| RECORDING | SAVED | 이후 샘플 500개 기록 완료 |
| RECORDING | MONITORING | 기록 취소 및 초기화 |
| SAVED | MONITORING | 기록 초기화 |

기존 계약에서는 KEY1에 기록 초기화 기능을 연결하도록 정의하였다.

현재 KEY1은 경고음 및 표시만 초기화하므로 기존 상태 전환 정책과 차이가 있다.

### 14.3 현재 Startup State

현재 main.c는 다음 시작 상태를 구분한다.

| State | 역할 |
|---|---|
| CALIBRATING | 초기 센서 평균값 확보 |
| READY | 준비 완료 표시 |
| MONITORING | 정상 센서 감시 및 이벤트 감지 |

### 14.4 현재 Record State

Record 수집 여부는 main.c의 기록 상태 변수와 event_record_is_saved()를 이용하여 확인한다.

- 기록 전
- 기록 수집 중
- 기록 완료

Record 상태와 VGA 화면 선택 상태는 독립적으로 관리한다.

### 14.5 State Policy Review

기존 SystemState API의 실제 사용 여부 및 기존 RECORDING/SAVED 보호 정책은 최신 소스와 대조하여 확정한다.

기존 상태 관리 계약은 임의로 삭제하지 않는다.

---

## 15. Main Integration Rules

**Source:** `src/main.c`  
**Owner:** 공동

### 15.1 초기화 순서

현재 main.c에서 사용하는 주요 초기화 대상은 다음과 같다.

1. KEY
2. Event Detector
3. Risk Level
4. LED / HEX
5. Sensor Manager
6. Ring Buffer
7. Event Record
8. Event History
9. VGA
10. Alarm Audio Driver
11. Alarm
12. Interrupt
13. Timer
14. Interrupt 활성화
15. 메인 루프 시작

실제 초기화 순서는 최신 main.c와 하드웨어 구성에 따른다.

공통 Peripheral 중복 초기화는 금지한다.

### 15.2 Calibration

초기 센서 데이터를 수집하고 평균값을 계산한다.

계산된 초기 평균값은 다음 API로 전달한다.

```c
event_detector_set_baseline(
    vibration_average,
    light_average,
    sound_average
);

risk_level_set_baseline(
    vibration_average,
    light_average,
    sound_average
);

vga_set_baseline(
    vibration_average,
    light_average,
    sound_average
);
```

초기 평균값 설정 및 READY 표시 완료 이후 정상 Monitoring 상태로 전환한다.

### 15.3 10ms 샘플 처리 순서

1. Timer 샘플 작업 요청 확인
2. Sensor Manager로 SensorSample 생성
3. 센서 샘플 조회 성공 여부 확인
4. 시작 상태에 따라 Calibration 또는 Monitoring 처리
5. Monitoring 상태에서 감지용 샘플 생성
6. Event Detector 및 Risk Level 계산
7. 새로운 이벤트 발생 시 History 및 Record 처리
8. 이벤트 출력 및 경고음 제어
9. event_record_update()로 현재 샘플 처리
10. ring_buffer_push()로 최신 원본 샘플 반영
11. Record 완료 여부 확인 및 관련 로그 처리

세부 로그 및 상태 갱신 위치는 실제 main.c 구현을 따른다.

### 15.4 Alarm Feedback Prevention

```c
SensorSample detect_sample = sample;

if (alarm_is_active())
{
    detect_sample.sound_level = sound_average;
}

flags = event_detect(&detect_sample);

current_risk =
    risk_level_update(&detect_sample);
```

경고음 활성 상태에서는 감지용 소음값을 초기 소음 평균값으로 대체한다.

원본 센서 샘플은 변경하지 않는다.

| 기능 | 사용 데이터 |
|---|---|
| Event Detector | detect_sample |
| Risk Level | detect_sample |
| Event History DELTA | sample |
| Event Record | sample |
| Ring Buffer | sample |
| UART | sample |
| VGA VALUE | Ring Buffer의 원본 sample |

경고음 활성 중 실제 외부 소음 이벤트도 감지에서 제외되는 제한이 있다.

경고음 종료 직후 잔류 소음에 대한 별도 차단 시간은 적용하지 않는다.

### 15.5 Multiple Event Expansion

다음 조건을 모두 만족할 때 기존 Record의 복합 이벤트 확장을 시도한다.

```c
if (record_collecting &&
    !event_record_is_saved() &&
    event_record_get_info(&previous) &&
    event_history_get(0U, &latest) &&
    previous.timestamp_ms ==
        latest.timestamp_ms &&
    previous.event_flags ==
        latest.event_flags &&
    flags != previous.event_flags &&
    (uint8_t)(flags & previous.event_flags) ==
        previous.event_flags &&
    (uint32_t)(sample.timestamp_ms -
               previous.timestamp_ms)
        <= EVENT_COMBINE_WINDOW_MS)
```

확장에 성공하면 기존 Record 및 최신 History의 플래그를 갱신한다.

History DELTA는 기존 저장 규칙에 따라 갱신한다.

기존 이벤트 발생 시각, 센서 데이터 및 History 개수를 유지한다.

확장 조건을 충족하지 않는 경우 새로운 독립 이벤트로 처리한다.

500ms는 시간 기반 확장 조건이며 물리적 이벤트의 연속성을 완전히 보장하지 않는다.

### 15.6 Main Loop Responsibility

- 각 모듈을 정해진 순서대로 호출한다.
- 실제 센서 측정과 이벤트 감지 코드를 Main에 직접 구현하지 않는다.
- 모듈 간 내부 데이터를 직접 참조하지 않는다.
- KEY 입력 및 Audio 출력 처리를 지속적으로 수행한다.
- 이벤트 기록 완료 여부를 확인한다.
- 센서 데이터 및 History UART 출력을 관리한다.
- VGA 화면 갱신 함수를 반복 호출한다.
- KEY1에 의한 표시 초기화 시 기존 저장 데이터를 유지한다.
- 기존 Record 보호 정책과 현재 새 기록 요청 방식의 차이는 추가 협의한다.

### 15.7 VGA Update

```c
vga_update(
    timer_get_ms(),
    current_risk
);
```

메인 루프에서 반복 호출한다.

일반적인 VGA 화면 갱신 주기는 모듈 내부에서 1000ms로 관리한다.

### 15.8 Common Error Handling

| 상황 | 처리 규칙 |
|---|---|
| 초기화 실패 | 정상 시스템 실행을 시작하지 않는 것을 원칙으로 함 |
| 센서 읽기 실패 | 해당 샘플을 유효한 데이터로 사용하지 않음 |
| NULL 인자 | API별 정의된 실패 반환 또는 무동작 |
| Ring Buffer 범위 오류 | false 반환 |
| 기록 중 새 이벤트 | 기존 보호 설계와 현재 동작의 차이 검토 |
| SAVED 상태 새 이벤트 | 기존 보호 설계와 현재 교체 동작 검토 |
| 기록 샘플 누락 | 타임스탬프로 누락 여부 확인 |
| 잘못된 상태 전환 | 기존 상태 전환 규약에 따라 처리 |
| KEY 중복 입력 | 50ms 디바운싱 적용 |
| 경고음 재감지 | 경고 중 감지용 소음값 대체 |
| VGA 최신 샘플 조회 실패 | NO SAMPLE 및 이전 위험 픽셀 제거 |
| History 조회 실패 | 해당 항목을 ---로 표시 |
| UART 출력 누락 | FIFO 및 타임아웃 추가 검토 |

---

## 16. Independent Testing Rules

각 팀원은 상대방의 실제 구현 코드 없이도 담당 모듈을 테스트할 수 있어야 한다.

### 16.1 이윤아 — Sensor & Storage

**Test 1: Ring Buffer**

- Mock SensorSample 600개 생성
- Ring Buffer에 순서대로 입력
- ring_buffer_count()가 500인지 확인
- 최신 500개 데이터가 시간순으로 유지되는지 확인
- 범위 오류 및 NULL 입력 처리 확인

**Test 2: Event Record**

- Mock EventInfo 생성
- 이전 데이터 500개 저장
- event_record_trigger() 호출
- 이벤트 발생 샘플을 포함하여 이후 데이터 500개 추가
- event_record_is_saved()가 true인지 확인
- 총 기록 1,000개와 이벤트 발생시간 검증
- Pre-event와 Post-event의 중복 샘플 여부 확인

**Test 3: Boundary Conditions**

- 프로그램 시작 후 5초 이전 이벤트
- Ring Buffer가 가득 차기 전 이벤트
- RECORDING 중 중복 이벤트
- SAVED 상태에서 추가 트리거
- 명시적 기록 초기화 후 새 이벤트 시작

기존 보호 정책과 현재 구현의 차이를 구분하여 테스트한다.

### 16.2 임연경 — Event & UI

**Test 1: Event Detection**

- Mock SensorSample 생성
- 충격, 소음, 조도 조건 개별 입력
- 이벤트 플래그 확인
- 변경된 임계값 800 / 1800 / 150 확인
- 연속 2회 감지 조건 확인
- SHOCK 쿨다운 종료 후 재감지 확인
- SOUND 및 LIGHT 래치 해제 확인

**Test 2: Event Classification**

- 플래그 0~7 입력
- HEX 코드 0~4 변환 결과 검증
- 복합 이벤트 분류 확인

**Test 3: Output**

- 단일 이벤트 LED/HEX 출력
- 복합 이벤트 LED 조합 출력
- 이벤트별 Audio 경고 패턴 확인
- 경고음 활성 및 종료 동작 확인

**Test 4: KEY & System State**

- KEY0 경고 해제
- KEY1 경고 및 LED/HEX 초기화
- KEY1 저장 데이터 유지
- KEY2 및 KEY3 화면 전환
- 버튼별 디바운싱 확인
- 기존 SystemState 전환 규약 확인
- 현재 Startup 및 Record 상태 전환 확인

**Test 5: VGA**

- 기본 SENSOR MONITOR 화면
- VALUE / LIMIT / DELTA 표시
- 실시간 DELTA 부호 표시
- 위험 단계 색상
- History 최신 10개 표시
- History DELTA 부호 제거
- HISTORY 문구 및 글꼴
- NO SAMPLE 픽셀 초기화
- 화면 전환 및 새로고침
- 1초 화면 갱신
- 저장 데이터 보존

### 16.3 Integration Test — 공동

1. Mock 데이터 기반 전체 처리 흐름 테스트
2. 실제 센서값을 이용한 이벤트 감지
3. 이벤트 기록 전후 데이터 정확성 확인
4. LED/HEX/Audio 경고 연동 테스트
5. Timer 및 KEY Interrupt 동작 확인
6. 연속 이벤트 및 버퍼 경계 조건 검증
7. 실제 센서와 Audio-out 간 재감지 간섭 확인
8. 동일 조건에서 핵심 기능 10회 이상 반복 검증
9. 경고음 활성 상태에서 원본 센서 데이터 보존 확인
10. 복합 이벤트 500ms 경계 조건 검증
11. VGA 실제 모니터 출력 확인
12. UART 및 VGA 출력 중 실제 센서 샘플링 간격 확인

---

## 17. Event History API

**Header:** `include/event_history.h`  
**Source:** `src/storage/event_history.c`

### 17.1 Public API

```c
void event_history_init(void);

bool event_history_push(
    uint32_t timestamp_ms,
    uint8_t flags
);

bool event_history_upgrade_latest(
    uint32_t timestamp_ms,
    uint8_t new_flags
);

bool event_history_push_with_delta(
    uint32_t timestamp_ms,
    uint8_t flags,
    uint16_t delta
);

bool event_history_upgrade_latest_with_delta(
    uint32_t timestamp_ms,
    uint8_t new_flags,
    uint16_t delta
);

uint32_t event_history_count(void);

bool event_history_get(
    uint32_t index,
    EventHistoryEntry *out
);

void event_history_clear(void);
```

기존 API 계약 및 DELTA를 포함하는 추가 API를 함께 유지한다.

### event_history_init()

History 내부 상태를 초기화한다.

### event_history_push()

기존 이벤트 저장 인터페이스이다.

### event_history_upgrade_latest()

기존 최신 이벤트 갱신 인터페이스이다.

### event_history_push_with_delta()

이벤트 발생 시각, 이벤트 플래그 및 절댓값 DELTA를 함께 저장한다.

### event_history_upgrade_latest_with_delta()

최신 이벤트의 플래그와 DELTA를 갱신한다.

기존 timestamp 및 History 개수는 유지한다.

### event_history_count()

현재 저장된 이벤트 개수를 반환한다.

반환 범위는 0부터 10까지이다.

### event_history_get()

저장된 이벤트를 인덱스로 조회한다.

- index 0: 가장 최근 이벤트
- true: 조회 성공
- false: 조회 실패

### event_history_clear()

기존 History 초기화 인터페이스이다.

현재 KEY1 입력 및 VGA 화면 전환에서는 호출하지 않는다.

### 17.2 Event History Policy

- 최근 이벤트 최대 10개를 저장한다.
- 10개 초과 시 가장 오래된 이벤트부터 교체한다.
- 최초 이벤트 발생 시 새 History 항목을 생성한다.
- 기존 이벤트가 복합 이벤트로 확장되면 최신 항목의 Flags 및 DELTA를 갱신한다.
- 복합 확장 시 최초 timestamp와 History 개수를 유지한다.
- 독립적인 새 이벤트는 새로운 History 항목으로 저장한다.
- 동일한 복합 Flags가 다시 확정되면 별개의 이벤트로 저장될 수 있다.
- 기존 Record 확장에는 500ms 조건을 적용한다.
- 화면 조회 및 새로고침은 새 History를 생성하지 않는다.

### 17.3 DELTA Policy

단일 이벤트:

    DELTA = abs(Event Sensor Value - Baseline)

복합 이벤트:

    DELTA = max(Included Sensor Differences)

복합 확장 시 기존 DELTA와 새로운 DELTA 중 큰 값을 유지하는 기존 정리 내용을 적용한다.

해당 갱신 규칙은 최신 event_history.c와 대조하여 최종 확인한다.

History에는 변화량의 부호를 저장하지 않는다.

VGA는 부호 없이 절댓값을 출력한다.

---

## 18. Risk Level API — 임연경

**Header:** `include/risk_level.h`  
**Source:** `src/event/risk_level.c`

### Public API

```c
void risk_level_init(void);

void risk_level_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base
);

RiskLevel risk_level_update(
    const SensorSample *sample
);
```

### risk_level_init()

위험 단계 계산 관련 내부 상태를 초기화한다.

### risk_level_set_baseline()

초기 Calibration에서 계산한 센서별 평균값을 전달한다.

### risk_level_update()

현재 센서 상태를 기준으로 위험 단계를 계산한다.

반환 상태:

- RISK_NORMAL
- RISK_CAUTION
- RISK_DANGER

현재 main.c는 반환된 위험 단계를 current_risk에 저장한다.

VGA는 current_risk를 전달받아 출력한다.

경고음 활성 중에는 이벤트 감지에 사용하는 detect_sample을 위험 단계 계산에도 사용한다.

구체적인 위험 단계 판정 기준은 risk_level.c의 실제 구현을 따른다.

---

## 19. VGA API — 임연경

**Header:** `include/vga.h`  
**Source:** `src/display/vga.c`

### 19.1 Public API

```c
void vga_init(void);

void vga_set_screen(VgaScreen screen);

void vga_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base
);

void vga_update(
    uint32_t now_ms,
    RiskLevel risk
);
```

### vga_init()

- VGA 내부 상태 및 화면을 초기화한다.
- 기본 화면은 VGA_SCREEN_REALTIME이다.
- 초기 기준값은 아직 설정되지 않은 상태이다.

### vga_set_screen()

**Parameter**

- screen: 선택할 VGA 화면

다른 화면을 선택하면 전체 화면 재출력을 요청한다.

같은 화면을 다시 선택하면 데이터 갱신을 요청한다.

실제 갱신은 이후 vga_update()에서 수행한다.

### vga_set_baseline()

**Parameters**

- vibration_base: 초기 진동 평균값
- light_base: 초기 조도 평균값
- sound_base: 초기 소음 평균값

전달된 기준값은 실시간 화면의 LIMIT 및 DELTA 계산에 사용한다.

### vga_update()

**Parameters**

- now_ms: 현재 시스템 시간
- risk: 현재 위험 단계

현재 선택된 VGA 화면의 데이터를 갱신한다.

일반 갱신 주기는 1000ms이다.

화면 전환 또는 강제 갱신 요청 시 다음 호출에서 즉시 갱신한다.

### 19.2 Hardware Configuration

| 항목 | 설정값 |
|---|---|
| Pixel Buffer Base | 0x08000000 |
| Character Buffer Base | 0x09000000 |
| Pixel Resolution | 320 x 240 |
| Character Resolution | 80 x 60 |
| Pixel Format | RGB565 |
| Pixel Row Stride | 1024 bytes |
| Character Row Stride | 128 bytes |

### 19.3 Internal Functions

| Function | 역할 |
|---|---|
| pixel_write() | 단일 픽셀 출력 |
| pixels_rect() | 픽셀 영역 채우기 |
| text_at() | 일반 문자열 출력 |
| text_field() | 고정 폭 문자열 출력 및 잔상 제거 |
| chars_clear() | 문자 버퍼 초기화 |
| color_word() | 색상 문자열 출력 |
| signed_delta_u16() | 부호 있는 변화량 계산 |
| background_for_screen() | 화면 기본 레이아웃 출력 |
| live_update() | 실시간 화면 갱신 |
| event_type_name() | 이벤트 종류 문자열 변환 |
| history_update() | History 화면 갱신 |

위 함수들은 vga.c 내부 구현 함수이며 외부 모듈에서 직접 호출하지 않는다.

### 19.4 Real-time Screen

화면 제목: SENSOR MONITOR

우측 상단 표시: LIVE

| 항목 | 표시 내용 |
|---|---|
| UPTIME | 시스템 경과 시간 |
| SENSOR | VIBRATION / SOUND / LIGHT |
| VALUE | 최신 원본 센서값 |
| LIMIT | 초기 Calibration 평균값 |
| DELTA | VALUE - LIMIT |
| RISK | 현재 위험 단계 |
| THRESHOLD | 센서별 이벤트 감지 임계값 |

Ring Buffer의 최신 샘플을 조회한다.

실시간 DELTA는 부호를 유지한다.

```c
return (int32_t)value - (int32_t)limit;
```

출력 형식은 `%+ld`를 사용한다.

LIMIT은 초기 센서 평균값이며 이벤트 감지 임계값과 구분한다.

### 19.5 Risk Display

| RiskLevel | 문자열 | 색상 |
|---|---|---|
| RISK_NORMAL | NORMAL | 녹색 |
| RISK_CAUTION | CAUTION | 노란색 |
| RISK_DANGER | DANGER | 빨간색 |

위험 단계는 main.c에서 전달받은 값을 사용한다.

### 19.6 Calibration Display

유효한 샘플이 존재하지만 초기 기준값이 설정되지 않은 경우:

- VALUE: 현재 센서값
- LIMIT: CAL...
- DELTA: ---
- RISK: CALIBRATING

### 19.7 No Sample Handling

Ring Buffer가 비어 있거나 최신 샘플 조회에 실패한 경우:

1. VALUE / LIMIT / DELTA를 ---로 표시한다.
2. 이전 위험 단계의 픽셀 영역을 배경색으로 초기화한다.
3. NO SAMPLE을 출력한다.
4. 현재 화면 갱신을 종료한다.

픽셀 영역 초기화 코드:

```c
pixels_rect(
    100U, 204U,
    230U, 225U,
    C_BG
);
```

정상적으로 샘플을 다시 조회하면 기존 NO SAMPLE 표시를 제거하고 현재 위험 단계 또는 Calibration 상태를 표시한다.

NO SAMPLE은 최신 저장 샘플을 조회하지 못한 상태를 의미하며, 개별 센서 입력 실패나 오래된 샘플 여부를 직접 판단하는 기능은 아니다.

### 19.8 History Screen

화면 제목: LAST EVENT

우측 상단 표시: HISTORY

| 항목 | 표시 내용 |
|---|---|
| NO. | 이벤트 순서 |
| TIME | 이벤트 발생 시각 |
| TYPE | 이벤트 종류 |
| DELTA | 저장된 절댓값 변화량 |
| STORED | History 저장 개수 |

- 최근 이벤트 최대 10개를 최신순으로 표시한다.
- 단일 이벤트는 SHOCK, SOUND, LIGHT로 표시한다.
- 복합 이벤트는 MULTIPLE로 표시한다.
- 비어 있는 항목의 데이터는 ---로 표시한다.
- History DELTA는 `%u` 형식으로 출력한다.
- HISTORY는 현재 화면 종류를 나타내며 Record 수집 완료를 의미하지 않는다.

### 19.9 Colored Text

색상 문자열은 5x7 글꼴을 2배 확대하여 픽셀 버퍼에 출력한다.

HISTORY 출력에 필요한 H 및 Y 글꼴을 포함한다.

글자 수 증가에 맞추어 시작 좌표를 조정한다.

```c
color_word(
    226U, 12U,
    "HISTORY", C_GREEN
);
```

실제 글꼴 출력과 화면 잘림 여부는 VGA 모니터에서 검증한다.

### 19.10 Refresh & Data Preservation

- 일반적인 화면 데이터 갱신 주기는 1000ms이다.
- 화면 전환 시 픽셀 및 문자 영역을 초기화한다.
- 동일 화면 재선택 시 데이터 새로고침을 요청한다.
- VGA는 센서를 직접 측정하지 않는다.
- VGA는 이벤트를 직접 생성하거나 삭제하지 않는다.
- Ring Buffer 및 Event History의 공개 조회 API를 사용한다.

### 19.11 Build Rule

Makefile의 SRCS 목록에 다음 구현 파일을 포함한다.

```makefile
src/display/vga.c
```

vga.h를 포함하는 것만으로는 VGA 함수 구현이 링크되지 않는다.

Intel FPGA Monitor Program에서 Makefile을 다시 생성할 경우 VGA 구현 파일 포함 여부를 확인한다.

---

## 20. Alarm Audio Driver API

**Header:** `include/alarm_audio_driver.h`

Audio-out 하드웨어 초기화 및 경고음 출력 처리를 담당한다.

### Integration

- Alarm 모듈과 연동하여 Audio-out을 제어한다.
- 메인 루프에서 필요한 Audio FIFO 처리를 수행한다.
- Audio CODEC 중복 초기화를 방지한다.
- 센서 측정과 인터럽트 처리를 장시간 방해하지 않도록 구성한다.

정확한 함수 선언과 Audio FIFO 접근 방식은 최신 alarm_audio_driver.h 및 구현 파일을 기준으로 한다.

---

## 21. UART Integration

JTAG UART는 시스템 상태 및 센서 데이터를 확인하기 위한 출력 인터페이스이다.

| 출력 | 주기 또는 조건 |
|---|---|
| 시스템 초기화 | 초기화 시 |
| Calibration 결과 | 초기 평균값 설정 시 |
| Sensor Sample | 1000ms |
| Event Information | 이벤트 발생 시 |
| Record BEFORE | Record 시작 시 |
| Record AFTER | Post-event 대표 샘플 |
| Record SAVED | Record 수집 완료 시 |
| Event History | 10000ms |

Event Record의 대표 출력 샘플과 실제 저장 샘플 개수는 구분한다.

UART History는 오래된 이벤트부터 출력한다.

VGA History는 최신 이벤트부터 출력한다.

UART FIFO 대기와 출력 타임아웃에 의한 문자열 누락 및 샘플링 지연 가능성은 추가 검토한다.

---

## 22. Current Limitations & Verification

### 추가 검토 사항

- Event Record 수집 중 기존 기록 보호 및 교체 정책
- 기존 SystemState 계약과 현재 main.c 상태 관리 방식
- 기존 Sensor API와 Sensor Manager API의 실제 호환 관계
- UART 출력 누락 및 샘플링 지연
- Audio-in 조회 실패 시 데이터 유효성 처리
- Calibration 설정값과 실제 보정 시간의 정합성
- Moving Average 필터 적용 여부
- VGA 화면 출력에 따른 샘플링 지연
- 경고음 활성 중 외부 소음 감지 제한
- 경고음 종료 직후 잔류 소음
- 위험 단계의 구체적인 판정 조건
- 기존 Alarm 경고 패턴 및 자동 종료 동작

### Verification Rule

기존 문서에 정의된 API 계약과 현재 확인된 구현 내용을 구분한다.

본 문서의 수정 내용은 기존 문서 원문 및 개발 기록을 대조하여 작성하였다.

최신 전체 헤더 및 소스 코드의 일치 여부는 추가 확인한다.

최근 수정 코드의 실제 컴파일, 링크 및 DE1-SoC 보드 테스트는 별도로 수행한다.
