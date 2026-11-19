
/*
 * DE1-SoC Audio-out Driver
 *
 * 사용 장치:
 * - DE1-SoC Audio CODEC
 * - 3.5mm LINE OUT
 * - USB 전원 2채널 미니 스피커
 *
 * 기본 샘플링 주파수: 48000Hz
 * 출력 채널: Left + Right
 *
 * 별도 Audio IRQ 없이 main에서 FIFO를 갱신한다.
 */

#include "../../include/alarm_audio_driver.h"
#include <stdint.h>

/* DE1-SoC Audio Core 레지스터 주소 */
#define AUDIO_BASE 0xFF203040U

#define AUDIO_CONTROL \
    (*(volatile uint32_t *)(uintptr_t)(AUDIO_BASE + 0x00U))

#define AUDIO_FIFOSPACE \
    (*(volatile uint32_t *)(uintptr_t)(AUDIO_BASE + 0x04U))

#define AUDIO_LEFTDATA \
    (*(volatile uint32_t *)(uintptr_t)(AUDIO_BASE + 0x08U))

#define AUDIO_RIGHTDATA \
    (*(volatile uint32_t *)(uintptr_t)(AUDIO_BASE + 0x0CU))

/* Audio 출력 FIFO 초기화 비트 */
#define AUDIO_CLEAR_WRITE 0x08U

/* 기본 Audio CODEC 샘플링 주파수 */
#define AUDIO_SAMPLE_RATE_HZ 48000U

/* 오디오 출력 진폭: 처음에는 낮은 음량으로 시작 */
#define AUDIO_OUTPUT_AMPLITUDE 0x10000000

/* 좌우 채널별 FIFO 최대 크기 */
#define AUDIO_FIFO_CAPACITY 128U

/* 현재 경고음 주파수 */
static uint32_t s_frequency_hz = 0U;

/* 구형파 생성용 위상 */
static uint32_t s_phase = 0U;


/*
 * Audio-out FIFO만 초기화한다.
 *
 * Audio-in FIFO는 초기화하지 않으므로
 * 마이크 입력 기능과 함께 사용할 수 있다.
 */
static void clear_output_fifo(void)
{
    AUDIO_CONTROL = AUDIO_CLEAR_WRITE;
    AUDIO_CONTROL = 0U;
}


/* Audio-out 드라이버 초기화 */
void alarm_audio_init(void)
{
    s_frequency_hz = 0U;
    s_phase = 0U;

    /* 남아 있는 출력 데이터 제거 */
    clear_output_fifo();
}


/*
 * 출력할 경고음 주파수 설정
 *
 * 실제 샘플 출력은 alarm_audio_update()가 수행한다.
 */
void alarm_audio_set_frequency(uint32_t frequency_hz)
{
    /* 지원하지 않는 높은 주파수는 출력하지 않음 */
    if (frequency_hz >= (AUDIO_SAMPLE_RATE_HZ / 2U))
    {
        frequency_hz = 0U;
    }

    /* 주파수가 동일하면 변경하지 않음 */
    if (frequency_hz == s_frequency_hz)
    {
        return;
    }

    s_frequency_hz = frequency_hz;

    /* 주파수 변경 시 파형 시작 위치 초기화 */
    s_phase = 0U;

    /* 0Hz: 현재 출력 중지 및 출력 FIFO 비우기 */
    if (frequency_hz == 0U)
    {
        clear_output_fifo();
    }
}


/*
 * Audio-out FIFO에 경고음 샘플 공급
 *
 * FIFO의 남은 공간만큼만 기록하므로
 * 공간이 날 때까지 기다리지 않는다.
 *
 * main() 반복문에서 계속 호출해야 한다.
 */
void alarm_audio_update(void)
{
    uint32_t fifo_space;
    uint32_t left_space;
    uint32_t right_space;
    uint32_t available;

    /* 현재 출력할 경고음이 없으면 종료 */
    if (s_frequency_hz == 0U)
    {
        return;
    }

    /* 좌우 Audio-out FIFO의 남은 공간 확인 */
    fifo_space = AUDIO_FIFOSPACE;

    left_space = (fifo_space >> 24U) & 0xFFU;
    right_space = (fifo_space >> 16U) & 0xFFU;

    /* 좌우 채널에 동일한 개수의 샘플을 기록 */
    available = (left_space < right_space)
                    ? left_space
                    : right_space;

    if (available > AUDIO_FIFO_CAPACITY)
    {
        available = AUDIO_FIFO_CAPACITY;
    }

    /* 사용 가능한 공간에 오디오 샘플 기록 */
    while (available > 0U)
    {
        int32_t pcm_sample;

        /*
         * 48kHz 샘플링 기준으로 구형파 생성
         *
         * 전반부: 양의 진폭
         * 후반부: 음의 진폭
         *
         * 스피커에서 경고음으로 출력된다.
         */
        if (s_phase < (AUDIO_SAMPLE_RATE_HZ / 2U))
        {
            pcm_sample = AUDIO_OUTPUT_AMPLITUDE;
        }
        else
        {
            pcm_sample = -AUDIO_OUTPUT_AMPLITUDE;
        }

        /* 스테레오 좌우 채널에 동일한 샘플 출력 */
        AUDIO_LEFTDATA = (uint32_t)pcm_sample;
        AUDIO_RIGHTDATA = (uint32_t)pcm_sample;

        /* 다음 오디오 샘플의 위상 계산 */
        s_phase += s_frequency_hz;

        if (s_phase >= AUDIO_SAMPLE_RATE_HZ)
        {
            s_phase -= AUDIO_SAMPLE_RATE_HZ;
        }

        available--;
    }
}
