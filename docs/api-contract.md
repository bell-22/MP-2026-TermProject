# API Contract

**Project:** MP-2026-TermProject  
**System:** Multi-Sensor Event Detection & Blackbox System  
**Status:** Draft — 팀원 검토 및 합의 필요  
**Last Updated:** 2026-11-12

## 1. Overview

본 문서는 DE1-SoC 기반 다중 센서 이벤트 감지 및 데이터 블랙박스 시스템의 공통 자료구조, 모듈 간 함수 인터페이스(API), 데이터 처리 순서 및 개발 규칙을 정의한다.

각 팀원은 본 문서에서 정의한 인터페이스를 기준으로 담당 모듈을 독립적으로 개발하며, 모듈 내부 구현에 직접 의존하지 않고 공개 API를 통해 데이터를 교환한다.

### 1.1 기본 시스템 규격

| 항목 | 규격 |
|---|---|
| 개발 언어 | C |
| Target | DE1-SoC / Nios II |
| Timer Interrupt | 1ms |
| 통합 센서 샘플 주기 | 10ms |
| Ring Buffer | 500 samples |
| 이벤트 이전 기록 | 500 samples (5초) |
| 이벤트 이후 기록 | 500 samples (이벤트 발생 샘플 포함) |
| 이벤트 기록 공간 | 1,000 samples |
| 저장 가능한 이벤트 | 최신 1건 |
| 시스템 상태 | MONITORING / RECORDING / SAVED |
| 이벤트 출력 | LED / HEX / Audio-out |
| 사용자 입력 | KEY0 / KEY1 |

※ 이벤트 이후 기록은 발생 샘플을 포함하여 500개 저장한다. 따라서 10ms 간격의 샘플 시각은 발생 시점부터 4,990ms까지이며, 이를 설계상 5초 기록 구간으로 취급한다.

※ 실제 Nios II 시스템의 레지스터 주소와 Peripheral 구성은 제공된 BSP 및 하드웨어 설정을 기준으로 확정한다.

### 1.2 개발 담당

| 담당 | 개발 영역 |
|---|---|
| 이윤아 | Sensor, Timer, Interrupt, Ring Buffer, Event Record |
| 임연경 | Event Detection, Classification, LED/HEX, Alarm, KEY, System State |
| 공동 | 공통 자료구조, 설정값, API 규약, Main Integration |

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
|---|---|---|
| EVENT_NONE | 0 | 이벤트 없음 |
| EVENT_SHOCK | 1 | 충격 |
| EVENT_SOUND | 2 | 소음 |
| EVENT_LIGHT | 4 | 조도 변화 |
| SHOCK + SOUND | 3 | 충격 + 소음 |
| SHOCK + LIGHT | 5 | 충격 + 조도 |
| SOUND + LIGHT | 6 | 소음 + 조도 |
| SHOCK + SOUND + LIGHT | 7 | 세 이벤트 동시 발생 |

복합 이벤트는 비트 OR 연산을 이용한다.

### 2.2 SystemState

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

### 2.3 SensorSample

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
| sound_level | 10ms 오디오 구간의 음량 특징값 |
| light | 조도 ADC 원시 측정값 |

**규칙**
- 10ms마다 `SensorSample` 1개를 생성한다.
- 모든 타임스탬프는 동일한 시스템 시간을 기준으로 한다.
- `sound_level`은 원본 오디오 샘플이 아닌 음량 특징값이다.
- 음량 계산 방식 및 정규화 범위는 실제 Audio-in 검증 후 확정한다.
- 실패한 센서 측정값은 유효한 샘플로 처리하지 않는다.

### 2.4 EventInfo

```c
typedef struct {
    uint32_t timestamp_ms;
    uint8_t event_flags;
} EventInfo;
```

**규칙**
- `timestamp_ms`는 이벤트 발생 샘플의 타임스탬프이다.
- `event_flags`는 이벤트 종류의 비트 조합이다.
- 복합 이벤트의 종류는 원본 비트 플래그로 보존한다.
- HEX 출력 코드는 이벤트 기록용 플래그와 구분한다.

---

## 3. System Configuration

**Header:** `include/system_config.h`  
**Owner:** 공동

### 3.1 Timer & Sampling

| Constant | Value | Description |
|---|---|---|
| TIMER_INTERVAL_MS | 1 | Timer Interrupt 주기 |
| SAMPLE_INTERVAL_MS | 10 | 통합 센서 샘플 주기 |

### 3.2 Buffer & Recording

| Constant | Value | Description |
|---|---|---|
| PRE_EVENT_DURATION_MS | 5000 | 이벤트 이전 기록 구간 |
| POST_EVENT_DURATION_MS | 5000 | 이벤트 이후 기록 구간 |
| PRE_EVENT_SAMPLES | 500 | 이전 데이터 최대 개수 |
| POST_EVENT_SAMPLES | 500 | 이후 데이터 개수 |
| RING_BUFFER_SIZE | 500 | Ring Buffer 용량 |
| EVENT_RECORD_SIZE | 1000 | 이벤트 기록 공간 |
| MAX_EVENT_RECORDS | 1 | 최대 저장 이벤트 수 |

### 3.3 Event Detection

| Constant | Description |
|---|---|
| SHOCK_THRESHOLD | 충격 감지 임계값 |
| SOUND_THRESHOLD | 소음 감지 임계값 |
| LIGHT_CHANGE_THRESHOLD | 조도 변화량 임계값 |
| EVENT_COOLDOWN_MS | 이벤트 재감지 대기시간 |
| EVENT_CONFIRM_SAMPLES | 이벤트 확정에 필요한 연속 샘플 수 |

※ 임계값은 초기 테스트값이며 센서 데이터 확인 후 조정한다.

※ 충격 감지는 순간적인 피크가 사라지지 않도록 원시 측정값 또는 변화량을 활용한다.

### 3.4 Input & Alarm

| Constant | Description |
|---|---|
| KEY_DEBOUNCE_MS | KEY 디바운싱 시간 |
| LED_BLINK_INTERVAL_MS | LED 점멸 간격 |
| ALARM_DURATION_MS | 기본 경고음 출력시간 |

---

## 4. Sensor API — 이윤아

**Header:** `include/sensor.h`  
**Source:** `src/sensor/`

```c
bool sensor_init(void);
bool sensor_read(SensorSample *out);
```

### sensor_init()

**Purpose:** 센서 측정 기능 초기화

**Return**
- `true`: 초기화 성공
- `false`: 초기화 실패

**Rules**
- ADC 및 센서 입력 인터페이스를 준비한다.
- 실제 센서 초기화 방식은 하드웨어 환경에 따라 구현한다.
- Audio CODEC의 중복 초기화를 방지한다.

### sensor_read()

**Purpose:** 통합 센서 데이터 생성

**Parameter**
- `out`: 측정 결과를 저장할 `SensorSample` 포인터

**Return**
- `true`: 필수 센서 데이터 수집 성공
- `false`: NULL 인자 또는 센서 수집 실패

**Rules**
- 통합 센서 데이터는 10ms 주기로 생성한다.
- 실제 Audio-in 샘플링 주기는 별도로 관리할 수 있다.
- 각 센서의 측정값을 하나의 `SensorSample`로 제공한다.
- `timestamp_ms`를 공통 시스템 시간으로 설정한다.
- 실패 시 `out`의 내용을 유효한 측정값으로 사용하지 않는다.
- 이 모듈은 이벤트 발생 여부를 직접 판단하지 않는다.

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
- 초기화 성공 시 `true`, 실패 시 `false`를 반환한다.
- 실제 IRQ 활성화는 공통 초기화 순서를 따른다.

### timer_get_ms()

- 시스템 시작 후 경과한 시간(ms)을 반환한다.
- Timer ISR에서 갱신되는 시간을 기준으로 한다.
- 시간값 조회 중 동시 접근에 따른 불일치가 발생하지 않도록 구현한다.
- 32비트 시간 카운터의 오버플로 가능성을 고려한다.

### timer_take_sample_tick()

- 10ms 센서 샘플링 요청을 확인한다.
- 처리할 요청이 있으면 요청 하나를 소비하고 `true`를 반환한다.
- 요청이 없으면 `false`를 반환한다.
- 처리 지연으로 여러 요청이 쌓일 경우 누락되지 않도록 카운터 방식으로 구현할 수 있다.
- 실제 센서값과 타임스탬프는 과거 시점에 측정한 것으로 임의 생성하지 않는다.

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
- ISR에서 센서 측정·이벤트 판단·Audio 출력 등의 장시간 작업을 수행하지 않는다.
- KEY 입력에 대한 요청은 입력 모듈로 전달한다.
- IRQ 설정과 ISR 구현은 실제 Nios II BSP 및 제공 예제에 맞춰 작성한다.
- 다른 모듈에서 공통 IRQ 컨트롤러를 중복 초기화하지 않는다.

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
- `sample`: 저장할 센서 샘플

**Rules**
- 유효한 샘플을 버퍼에 추가한다.
- 버퍼 용량은 `RING_BUFFER_SIZE`이다.
- 버퍼가 가득 차면 가장 오래된 데이터를 덮어쓴다.
- `sample == NULL`이면 아무 작업도 수행하지 않는다.
- 정상 호출 시 10ms마다 샘플 하나가 추가된다.

### ring_buffer_count()

- 현재 유효한 샘플 개수를 반환한다.
- 반환 범위는 0부터 `RING_BUFFER_SIZE`까지이다.

### ring_buffer_get()

**Parameters**
- `index`: 조회할 샘플 위치
- `out`: 조회 결과 저장 위치

**Return**
- `true`: 조회 성공
- `false`: 인덱스 범위 오류 또는 NULL 출력 포인터

**Index Rule**
- `index = 0`: 가장 오래된 유효 샘플
- `index = count - 1`: 가장 최근 샘플

### Ring Buffer Rules

- 이벤트 감지 및 분류 기능을 직접 수행하지 않는다.
- 최신 데이터가 들어오면 기존 데이터를 순환 방식으로 갱신한다.
- 이벤트 기록 모듈은 공개 조회 API를 이용해 이전 데이터에 접근한다.
- 버퍼 내부 저장 배열과 인덱스는 외부 모듈에 노출하지 않는다.

---

## 8. Event Record API — 이윤아

**Header:** `include/event_record.h`  
**Source:** `src/storage/event_record.c`

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
- 이 함수는 Ring Buffer 자체를 초기화하지 않는다.

### event_record_trigger()

**Purpose:** 이벤트 기록 시작

**Parameter**
- `event`: 감지된 이벤트 정보

**Return**
- `true`: 기록 시작 성공
- `false`: NULL 인자, 기록 중 또는 저장된 기록이 있어 시작 불가

**Rules**
1. 기록이 없는 감시 상태에서만 새 기록을 시작한다.
2. Ring Buffer에서 이전 5초 데이터(최대 500개)를 복사한다.
3. 이벤트 정보와 발생시간을 저장한다.
4. 이후 데이터 수집을 위한 기록 상태를 활성화한다.
5. 기존 기록이 있으면 해당 데이터를 덮어쓰지 않는다.
6. 이 함수는 시스템 전체 상태를 직접 변경하지 않는다.

### event_record_update()

**Purpose:** 이벤트 발생 이후 센서 데이터 기록

**Parameter**
- `sample`: 추가할 센서 샘플

**Rules**
1. 기록 진행 중일 때만 샘플을 추가한다.
2. 이벤트 발생 샘플을 이후 기록의 첫 번째 샘플로 포함한다.
3. 이후 샘플 500개가 저장되면 기록을 완료한다.
4. 기록 완료 후 추가 샘플은 기존 기록에 영향을 주지 않는다.
5. `sample == NULL`이면 아무 작업도 수행하지 않는다.
6. 기록 완료 여부는 `event_record_is_saved()`로 확인한다.
7. 이후 기록 샘플의 시간 간격을 확인해 기록 누락 여부를 검증한다.

### event_record_is_saved()

**Return**
- `true`: 이벤트 기록 완료
- `false`: 기록 전 또는 기록 진행 중

### event_record_get_info()

```c
bool event_record_get_info(EventInfo *out);
```

**Purpose:** 저장된 이벤트 정보를 복사하여 반환

**Return**
- `true`: 이벤트 정보 조회 성공
- `false`: 기록 정보가 없거나 `out == NULL`

**Rules**
- 이벤트 정보가 유효하면 `out`에 복사한다.
- 기록 모듈 내부 포인터를 반환하지 않는다.
- 기록 완료 전이라도 트리거가 성공하여 이벤트 정보가 생성된 경우 조회할 수 있다.
- 유효하지 않은 출력 포인터는 참조하지 않는다.

### event_record_get_count()

- 현재 기록 공간에 저장된 유효 샘플 수를 반환한다.
- 반환 범위는 0부터 `EVENT_RECORD_SIZE`까지이다.
- 기록 중에는 현재까지 저장된 개수를 반환한다.
- 기록 완료 후 저장된 개수는 최대 1,000개이다.
- 이벤트 발생 전 확보된 데이터가 500개 미만인 경우 최종 개수도 1,000개 미만일 수 있다.

### event_record_get_sample()

```c
bool event_record_get_sample(uint32_t index, SensorSample *out);
```

**Return**
- `true`: 조회 성공
- `false`: 범위 오류 또는 NULL 출력 포인터

**Index Rule**
- `index = 0`: 기록된 샘플 중 가장 오래된 데이터
- 이후 인덱스는 시간 순서대로 증가한다.
- 기록 진행 중에는 현재까지 저장된 샘플만 조회할 수 있다.

### event_record_clear()

- 현재 기록 정보와 저장 데이터를 논리적으로 초기화한다.
- 진행 중인 기록도 취소한다.
- 이후 새로운 이벤트 기록을 시작할 수 있다.
- Ring Buffer 초기화 여부는 시스템 정책에 따라 Main에서 별도로 결정한다.

### Event Recording Policy

| 항목 | 정책 |
|---|---|
| 이벤트 이전 데이터 | 최대 500개 |
| 이벤트 이후 데이터 | 500개 |
| 최대 기록 | 1,000개 |
| 저장 기록 | 최신 이벤트 1건 |
| 기록 중 새 이벤트 | 기존 기록 우선 보호 |
| SAVED 상태 새 이벤트 | 기존 기록이 있으므로 새 기록 시작 거절 |
| KEY1 | 기록 초기화 및 MONITORING 복귀 |
| 초기 5초 이내 이벤트 | 실제 확보된 이전 데이터만 저장 |

### Sample Boundary Rule

이벤트 발생 샘플이 이전 기록과 이후 기록에 중복 저장되지 않도록 다음 호출 순서를 유지한다.

```c
uint8_t flags = event_detect(&sample);

if (flags != EVENT_NONE && system_state_get() == STATE_MONITORING) {
    EventInfo event = {
        .timestamp_ms = sample.timestamp_ms,
        .event_flags = flags
    };

    if (event_record_trigger(&event)) {
        system_state_set(STATE_RECORDING);
    }
}

event_record_update(&sample);
ring_buffer_push(&sample);

if (system_state_get() == STATE_RECORDING &&
    event_record_is_saved()) {
    system_state_set(STATE_SAVED);
}
```

**Important**
- 현재 샘플은 `event_record_trigger()` 이전에 Ring Buffer에 추가하지 않는다.
- 이벤트 발생 샘플은 이후 기록의 첫 번째 데이터이다.
- 매 샘플마다 이벤트 검출 결과가 나올 수 있으므로, RECORDING 및 SAVED 상태에서는 새로운 기록을 시작하지 않는다.
- 샘플 누락이 발생하면 500개 데이터가 반드시 정확히 5초를 의미하지 않을 수 있으므로 타임스탬프를 이용해 검증한다.

---

## 9. Event Detector API — 임연경

**Header:** `include/event_detector.h`  
**Source:** `src/event/event_detector.c`

```c
void event_detector_init(void);
uint8_t event_detect(const SensorSample *sample);
```

### event_detector_init()

- 이벤트 감지에 필요한 내부 상태를 초기화한다.
- 이전 센서값 및 연속 감지 카운터를 초기화한다.
- 재감지 대기시간 관리 상태를 초기화한다.

### event_detect()

**Parameter**
- `sample`: 센서 측정 데이터

**Return**
- 이벤트 비트 플래그 (`EVENT_NONE`~복합 이벤트 조합)

**Rules**
1. `sample == NULL`이면 `EVENT_NONE`을 반환한다.
2. 센서값과 임계값·변화량을 비교한다.
3. 충격·소음·조도 변화 이벤트를 독립적으로 판단한다.
4. 복합 이벤트는 비트 OR로 표현한다.
5. 연속 측정 및 재감지 대기시간 조건을 적용한다.
6. 재감지 대기시간 계산에는 `sample->timestamp_ms`를 사용한다.
7. ADC와 Audio CODEC에 직접 접근하지 않는다.
8. 이벤트 기록 모듈을 직접 호출하지 않는다.
9. 경고음 재입력에 의한 소음 오감지를 방지하기 위해, 경고 출력 상태와 연동하는 방법을 통합 전에 확정한다.

---

## 10. Event Classifier API — 임연경

**Header:** `include/event_classifier.h`  
**Source:** `src/event/event_classifier.c`

```c
uint8_t event_classify(uint8_t event_flags);
```

### Purpose

이벤트 비트 플래그를 HEX 출력용 이벤트 코드로 변환한다.

### Return Rule

| Event Flags | HEX Code |
|---|---|
| EVENT_NONE | 0 |
| EVENT_SHOCK | 1 |
| EVENT_SOUND | 2 |
| EVENT_LIGHT | 3 |
| 복합 이벤트 | 4 |

### Rules

- 이벤트 플래그에 2개 이상의 이벤트가 설정되면 코드 4를 반환한다.
- 정의되지 않은 비트는 무시한다.
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
- `event_flags`: 이벤트 비트 플래그

**Rules**
- 입력받은 이벤트 플래그를 기준으로 LED 및 HEX 출력을 결정한다.
- HEX는 `event_classify()`의 반환값인 0~4를 표시한다.
- LED0은 충격, LED1은 소음, LED2는 조도 이벤트를 나타낸다.
- 복합 이벤트 발생 시 해당하는 LED를 함께 점등 또는 점멸한다.
- `event_flags`의 유효하지 않은 비트는 무시한다.

**Example**
- SHOCK → HEX 01, LED0
- SOUND → HEX 02, LED1
- LIGHT → HEX 03, LED2
- SHOCK | SOUND → HEX 04, LED0 + LED1

### led_hex_update()

- `timestamp_ms`를 이용해 LED 점멸 간격을 관리한다.
- 메인 루프에서 반복 호출한다.
- 호출 시 장시간 대기하거나 블로킹하지 않는다.

### led_hex_clear()

- LED를 끄고 HEX를 정상 상태(00)로 설정한다.
- 저장된 이벤트 기록은 변경하지 않는다.

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
- `event_code`: `event_classify()`가 반환한 0~4 코드

**Rules**
- 코드 1: 짧은 경고음 1회
- 코드 2: 짧은 경고음 2회
- 코드 3: 짧은 경고음 3회
- 코드 4: 별도 복합 이벤트 경고 패턴
- 코드 0: 경고음 시작하지 않음
- 실제 오디오 출력은 논블로킹 방식으로 처리한다.

### alarm_update()

- 시스템 시간에 따라 경고음 시작·종료 및 반복 횟수를 관리한다.
- 메인 루프에서 반복 호출한다.
- 경고음 출력 중에도 센서 측정과 인터럽트 처리가 가능해야 한다.

### alarm_stop()

- 현재 경고 출력을 중지한다.
- KEY0 경고 해제 요청과 연동한다.
- 이벤트 기록 데이터는 삭제하지 않는다.

### alarm_is_active()

- 현재 경고음이 출력 중이면 `true`를 반환한다.
- 그렇지 않으면 `false`를 반환한다.

### Audio Access Rule

- Audio-in과 Audio-out은 공통 Audio CODEC 설정을 사용할 수 있다.
- 동일한 Audio CODEC을 여러 모듈이 중복 초기화하지 않는다.
- CODEC 초기화 담당 및 입출력 FIFO 접근 규칙은 실제 보드 환경 검증 후 공동 확정한다.

---

## 13. KEY API — 임연경

**Header:** `include/key_switch.h`  
**Source:** `src/input/key_switch.c`

```c
#define KEY_EVENT_NONE   0U
#define KEY_EVENT_STOP   (1U << 0)
#define KEY_EVENT_CLEAR  (1U << 1)

void key_init(void);
uint8_t key_take_events(void);
```

### key_init()

- KEY 입력 처리 상태를 초기화한다.
- 실제 IRQ 등록과 공통 인터럽트 활성화는 Interrupt 모듈 규약을 따른다.
- KEY 입력 디바운싱 상태를 준비한다.

### key_take_events()

**Return**
- `KEY_EVENT_NONE`: 처리할 KEY 입력 없음
- `KEY_EVENT_STOP`: KEY0 경고 해제 요청
- `KEY_EVENT_CLEAR`: KEY1 기록 초기화 요청
- 여러 이벤트가 누적되면 비트 OR 조합 반환

**Rules**
- KEY ISR에서는 입력 이벤트를 기록하고 실제 기능을 실행하지 않는다.
- `key_take_events()`는 누적된 이벤트를 반환한 뒤 해당 요청을 소비한다.
- KEY0과 KEY1 입력은 `KEY_DEBOUNCE_MS`를 기준으로 디바운싱한다.
- 경고 종료 및 기록 초기화는 메인 루프에서 처리한다.

### Main Processing Rule

```c
uint8_t keys = key_take_events();

if (keys & KEY_EVENT_STOP) {
    alarm_stop();
}

if (keys & KEY_EVENT_CLEAR) {
    alarm_stop();
    event_record_clear();
    system_state_set(STATE_MONITORING);
}
```

※ KEY1을 누르면 진행 중인 기록도 취소하고 감시 상태로 돌아가는 정책을 적용한다. 두 KEY 요청이 함께 들어오면 KEY1 초기화가 최종적으로 우선한다.

---

## 14. System State API — 임연경

**Header:** `include/system_state.h`  
**Source:** `src/system/system_state.c`

```c
void system_state_init(void);
SystemState system_state_get(void);
void system_state_set(SystemState next);
```

### system_state_init()

- 시스템 상태를 `STATE_MONITORING`으로 초기화한다.

### system_state_get()

- 현재 시스템 상태를 반환한다.

### system_state_set()

- 요청된 상태 전환의 유효성을 검사한다.
- 유효한 상태 전환만 적용한다.
- 유효하지 않은 요청은 무시한다.
- 상태 전환은 메인 루프에서 처리한다.

### State Transition Rules

| Current State | Next State | Condition |
|---|---|---|
| MONITORING | RECORDING | 이벤트 기록 시작 성공 |
| RECORDING | SAVED | 이후 샘플 500개 기록 완료 |
| RECORDING | MONITORING | KEY1 기록 취소·초기화 |
| SAVED | MONITORING | KEY1 기록 초기화 |

### State Behavior

**MONITORING**
- 센서 데이터 수집
- 이벤트 감지
- Ring Buffer 갱신
- 신규 이벤트 기록 시작 가능

**RECORDING**
- 센서 데이터 수집 지속
- 이후 데이터 기록 진행
- 새로운 이벤트로 기존 기록을 덮어쓰지 않음
- Ring Buffer 갱신 가능

**SAVED**
- 센서 데이터 수집은 지속 가능
- 저장된 이벤트 기록 유지
- 새 이벤트 기록 시작은 비활성
- KEY1 입력 시 기록 초기화 후 감시 상태 복귀

---

## 15. Main Integration Rules

**Source:** `src/main.c`  
**Owner:** 공동

### 15.1 초기화 순서

1. 공통 Audio CODEC 및 Peripheral 초기화
2. Timer 및 Interrupt 초기화
3. Sensor 초기화
4. Ring Buffer 및 Event Record 초기화
5. Event Detector 초기화
6. LED/HEX 및 Alarm 초기화
7. KEY 및 System State 초기화
8. Interrupt 활성화
9. 메인 루프 시작

※ 초기화 순서는 실제 Nios II BSP 및 주변장치 설정에 따라 변경할 수 있다. 공통 Peripheral 중복 초기화는 금지한다.

### 15.2 10ms 샘플 처리 순서

1. Timer 샘플 작업 요청 확인
2. `sensor_read()`로 `SensorSample` 생성
3. `event_detect()`로 이벤트 감지
4. MONITORING 상태에서 이벤트 발생 시 `event_record_trigger()` 호출
5. 기록 시작 성공 시 RECORDING 전환
6. `event_record_update()`로 현재 샘플 저장
7. `ring_buffer_push()`로 최신 샘플 반영
8. 기록 완료 시 SAVED 전환
9. 이벤트 발생 시 LED/HEX 및 Audio 경고 출력
10. KEY 요청, LED 점멸, Audio 경고 상태 갱신

### 15.3 Main Loop Responsibility

- 각 모듈을 정해진 순서대로 호출한다.
- 실제 센서 측정과 이벤트 감지 코드를 Main에 직접 구현하지 않는다.
- 모듈 간 내부 데이터를 직접 참조하지 않는다.
- 10ms 샘플 작업 이외에도 KEY 입력 및 Audio 출력 처리를 지속적으로 수행한다.
- 이벤트 기록 완료 여부를 확인하고 시스템 상태를 갱신한다.
- `SAVED` 상태에서 새 기록을 시작하지 않는다.
- KEY1 초기화 시 진행 중인 기록이나 저장 기록을 지운 뒤 MONITORING으로 복귀한다.

### 15.4 Common Error Handling

| 상황 | 처리 규칙 |
|---|---|
| 초기화 실패 | 정상 시스템 실행을 시작하지 않음 |
| 센서 읽기 실패 | 해당 샘플을 유효한 데이터로 사용하지 않음 |
| NULL 인자 | API별 정의된 실패 반환 또는 무동작 |
| Ring Buffer 범위 오류 | `false` 반환 |
| 기록 중 새 이벤트 | 기존 기록 보호, 새 기록 시작 거절 |
| SAVED 상태 새 이벤트 | 기존 기록 유지 |
| 기록 샘플 누락 | 타임스탬프로 누락 여부 확인 및 테스트 기록 |
| 잘못된 상태 전환 | 상태 변경 요청 무시 |
| KEY 중복 입력 | 디바운싱 적용 |
| 경고음 재감지 | 경고 중 소음 이벤트 재감지 제한 |

---

## 16. Independent Testing Rules

각 팀원은 상대방의 실제 구현 코드 없이도 담당 모듈을 테스트할 수 있어야 한다.

### 16.1 이윤아 — Sensor & Storage

**Test 1: Ring Buffer**
- Mock `SensorSample` 600개 생성
- Ring Buffer에 순서대로 입력
- `ring_buffer_count()`가 500인지 확인
- 최신 500개 데이터가 시간순으로 유지되는지 확인

**Test 2: Event Record**
- Mock `EventInfo` 생성
- 이전 데이터 500개 저장
- `event_record_trigger()` 호출
- 이후 데이터 500개 추가
- `event_record_is_saved()`가 `true`인지 확인
- 총 기록 1,000개와 이벤트 발생시간 검증

**Test 3: Boundary Conditions**
- 프로그램 시작 후 5초 이전 이벤트
- Ring Buffer가 가득 차기 전 이벤트
- RECORDING 중 중복 이벤트
- SAVED 상태에서 추가 트리거
- 기록 초기화 후 새 이벤트 시작

### 16.2 임연경 — Event & UI

**Test 1: Event Detection**
- Mock `SensorSample` 생성
- 충격·소음·조도 조건을 개별 입력
- 이벤트 플래그가 정확한지 확인

**Test 2: Event Classification**
- 플래그 0~7 입력
- HEX 코드 0~4 변환 결과 검증

**Test 3: Output**
- 단일 이벤트 LED/HEX 출력
- 복합 이벤트 LED 조합 출력
- 이벤트별 Audio 경고 패턴 확인

**Test 4: KEY & System State**
- KEY0 경고 해제
- KEY1 기록 초기화 요청
- MONITORING → RECORDING → SAVED 상태 전환
- 잘못된 상태 전환 방지

### 16.3 Integration Test — 공동

1. Mock 데이터 기반 전체 처리 흐름 테스트
2. 실제 센서값을 이용한 이벤트 감지
3. 이벤트 기록 전후 데이터 정확성 확인
4. LED/HEX/Audio 경고 연동 테스트
5. Timer 및 KEY Interrupt 동작 확인
6. 연속 이벤트 및 버퍼 경계 조건 검증
7. 실제 센서와 Audio-out 간 재감지 간섭 확인
8. 동일 조건에서 핵심 기능 10회 이상 반복 검증