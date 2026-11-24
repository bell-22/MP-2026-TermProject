/*
 * Audio-in 으로 들어오는 PCM 샘플이 계속 쌓임
 * 여러 샘플 중 가장 큰 진폭을 골라 소음의 특징값으로 사용
 * 좌/우 채널 PCM 데이터 저장 (FIFO)
 * sound_update() : FIFO에서 PCM 데이터 읽음
 * sound_amplitude() : 부호가 있는 PCM을 양의 진폭으로 변환함
 * peak_amplitude : 수집한 구간의 최대 진폭 유지
 * sound_read() : 최대 진폭 반환 및 초기화
 */
#include "../../include/sound.h"

#include <stdint.h>
#include <stddef.h>

#define AUDIO_BASE 0xFF203040u
#define AUDIO_CONTROL (AUDIO_BASE + 0x00u)
#define AUDIO_FIFOSPACE (AUDIO_BASE + 0x04u) // FIFO 상태 확인
#define AUDIO_LEFTDATA (AUDIO_BASE + 0x08u)
#define AUDIO_RIGHTDATA (AUDIO_BASE + 0x0Cu)

/* FIFO 관련 설정값 */
#define AUDIO_CLEAR_READ 0x04u   // 입력 FIFO를 초기화할 떄 비트 마스크
#define AUDIO_FIFO_CAPACITY 128u // FIFO 용량을 128개 샘플로 정의

#ifndef AUDIO_MMIO_READ
#define AUDIO_MMIO_READ(addr) \
    (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

#ifndef AUDIO_MMIO_WRITE
#define AUDIO_MMIO_WRITE(addr, value) \
    (*(volatile uint32_t *)(uintptr_t)(addr) = (value))
#endif

static uint16_t peak_amplitude; // 최대 진폭
static bool has_samples;        // 샘플 존재 여부

static uint16_t sound_amplitude(uint32_t raw)
{
    int32_t sample = (int32_t)raw; // 양/음수(방향) -> 절댓값(크기)

    uint32_t amplitude =
        (sample < 0)
            ? (uint32_t)(-(int64_t)sample) // 음이면 - (64비트 자료형으로 오버플로 방지)
            : (uint32_t)sample;

    return (uint16_t)(amplitude >> 16); // 16비트로 축소
}

/* Audio 입력 초기화 */
void sound_init(void)
{
    /* 이전 계산한 최대 진폭값 초기화 */
    peak_amplitude = 0;
    has_samples = false;

    /* Audio ctl 레지스터에 Read FIFO 초기화 비트(0100)를 설정 -> 기존 입력 데이터 비우기 */
    AUDIO_MMIO_WRITE(AUDIO_CONTROL, AUDIO_CLEAR_READ);
    /* FIFO 초기화 비트를 다시 0으로 -> 새 입력 데이터를 받을 수 있도록 */
    AUDIO_MMIO_WRITE(AUDIO_CONTROL, 0u);
}

void sound_update(void)
{
    uint32_t fifospace = AUDIO_MMIO_READ(AUDIO_FIFOSPACE); // FIFO 상태

    /* 8비트 쪼개기 */
    uint32_t right_count = fifospace & 0xFFu;
    uint32_t left_count = (fifospace >> 8) & 0xFFu;

    // 왼/오 중 더 적은 수의 샘플을 선택 (짝 맞춤)
    uint32_t count =
        (left_count < right_count) ? left_count : right_count;

    // 128개 제한
    if (count > AUDIO_FIFO_CAPACITY)
    {
        count = AUDIO_FIFO_CAPACITY;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        // 읽기
        uint32_t left = AUDIO_MMIO_READ(AUDIO_LEFTDATA);
        uint32_t right = AUDIO_MMIO_READ(AUDIO_RIGHTDATA);

        // 양 변환
        uint16_t left_amp = sound_amplitude(left);
        uint16_t right_amp = sound_amplitude(right);

        // 더 큰 진폭 선택
        uint16_t amplitude =
            (left_amp > right_amp) ? left_amp : right_amp;

        // 최댓값 갱신 (짧은 소리 감지)
        if (amplitude > peak_amplitude)
        {
            peak_amplitude = amplitude;
        }

        has_samples = true;
    }
}

bool sound_read(uint16_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    if (!has_samples)
    {
        *out = 0;
        return false;
    }

    *out = peak_amplitude;

    /* 내부 상태 초기화 */
    peak_amplitude = 0;
    has_samples = false;

    return true;
}