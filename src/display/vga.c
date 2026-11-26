#include "../../include/vga.h"
#include "../../include/project_types.h"
#include "../../include/system_config.h"
#include "../../include/ring_buffer.h"
#include "../../include/event_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/*
 * ==========================================
 * DE1-SoC Sensor Blackbox - VGA Display
 * ==========================================
 *
 * 기능:
 * 1. 실시간 센서 모니터링 화면 출력
 * 2. 최근 이벤트 10개 History 화면 출력
 * 3. KEY2 / KEY3으로 화면 전환
 * 4. 실시간 센서값 및 위험 단계 표시
 * 5. 이벤트 발생 시각, 종류, DELTA 표시
 *
 * 화면 1: SENSOR MONITOR
 *   VALUE : 현재 센서값
 *   LIMIT : 초기 5초 평균값
 *   DELTA : VALUE - LIMIT (부호 유지)
 *   화면 오른쪽 상단에 LIVE 표시
 *
 * 화면 2: LAST EVENT
 *   최근 이벤트 최대 10개를 최신순으로 표시
 *   History에 저장된 절댓값 DELTA 사용
 *   증가 및 감소 방향을 구분하지 않고
 *   변화량의 크기만 표시한다.
 *
 *   화면 오른쪽 상단에 HISTORY 표시
 *   HISTORY는 저장 완료 상태가 아닌
 *   현재 화면의 종류를 나타낸다.
 *
 * VGA 화면은 기본적으로 1초마다 갱신한다.
 * 화면 전환 시에는 즉시 다시 그린다.
 */

/* =====================================
 * 1. VGA Hardware Configuration
 * ===================================== */

/*
 * DE1-SoC Nios II Computer의
 * VGA 메모리 매핑 주소
 *
 * PIXEL BASE:
 *   RGB565 색상 픽셀 데이터 저장
 *
 * CHAR BASE:
 *   ASCII 문자 데이터 저장
 */
#define VGA_PIXEL_BASE 0x08000000U
#define VGA_CHAR_BASE  0x09000000U

/* VGA 픽셀 해상도: 320 x 240 */
#define VGA_PIXEL_W 320U
#define VGA_PIXEL_H 240U

/* VGA 문자 버퍼: 80열 x 60행 */
#define VGA_CHAR_W 80U
#define VGA_CHAR_H 60U

/* VGA 데이터 갱신 주기: 1000ms = 1초 */
#define VGA_REFRESH_INTERVAL_MS 1000U

/*
 * RGB565 색상 정의
 *
 * RGB565:
 *   Red   : 5bit
 *   Green : 6bit
 *   Blue  : 5bit
 */
#define C_BG      0x0023U  /* 화면 배경색 */
#define C_LINE    0x5AEBU  /* 구분선 색상 */
#define C_GREEN   0x07E0U  /* 정상 상태 */
#define C_YELLOW  0xFFE0U  /* 주의 상태 */
#define C_RED     0xF800U  /* 위험 상태 */

/* =====================================
 * 2. VGA Internal State
 * ===================================== */

/* 현재 선택된 VGA 화면 */
static VgaScreen s_screen;

/*
 * 화면 전체를 다시 그려야 하는지 표시
 *
 * true:
 *   화면 전환 또는 기준값 변경 발생
 *
 * false:
 *   기존 화면의 데이터만 갱신
 */
static bool s_full_redraw;

/* 이전 VGA 갱신 기록이 존재하는지 확인 */
static bool s_have_update;

/* 마지막 VGA 데이터 갱신 시각 */
static uint32_t s_last_update_ms;

/*
 * 초기 5초 센서 보정 완료 여부
 *
 * false: 보정 전 또는 진행 중
 * true : 초기 평균값 설정 완료
 */
static bool s_have_baseline;

/*
 * 초기 5초 동안 계산한 센서 평균값
 *
 * 진동, 조도, 소음 순서로 저장
 */
static uint16_t s_base_v;
static uint16_t s_base_l;
static uint16_t s_base_s;

/* =====================================
 * 3. Pixel Buffer Functions
 * ===================================== */

/*
 * 지정 좌표에 픽셀 하나 출력
 *
 * x, y  : 픽셀 좌표
 * color : RGB565 색상
 *
 * 픽셀 하나는 2바이트를 사용한다.
 *
 * VGA 메모리에서 한 줄의 간격은
 * 1024바이트이므로 y << 10을 사용한다.
 */
static void pixel_write(uint32_t x,
                        uint32_t y,
                        uint16_t color)
{
    volatile uint16_t *addr;

    /* 화면 범위를 벗어난 좌표는 무시 */
    if (x >= VGA_PIXEL_W || y >= VGA_PIXEL_H)
        return;

    /*
     * 픽셀 메모리 주소 계산
     *
     * VGA_PIXEL_BASE : 픽셀 버퍼 시작 주소
     * (y << 10)      : y * 1024
     * (x << 1)       : x * 2
     */
    addr = (volatile uint16_t *)(uintptr_t)(
        VGA_PIXEL_BASE +
        (y << 10) +
        (x << 1)
    );

    /* 해당 픽셀에 색상 기록 */
    *addr = color;
}

/*
 * 지정 영역을 하나의 색상으로 채우는 함수
 *
 * (x0, y0) : 시작 좌표
 * (x1, y1) : 종료 경계 좌표
 *
 * x1, y1 좌표 자체는 포함하지 않는다.
 *
 * 사용 목적:
 *   - 전체 배경 초기화
 *   - 화면 구분선 출력
 *   - 기존 위험 단계 글자 제거
 *   - NO SAMPLE 출력 전 위험 단계 영역 제거
 *   - 색상 글자 그리기
 */
static void pixels_rect(uint32_t x0,
                        uint32_t y0,
                        uint32_t x1,
                        uint32_t y1,
                        uint16_t color)
{
    uint32_t x;
    uint32_t y;

    /* 화면 너비를 넘지 않도록 제한 */
    if (x1 > VGA_PIXEL_W)
        x1 = VGA_PIXEL_W;

    /* 화면 높이를 넘지 않도록 제한 */
    if (y1 > VGA_PIXEL_H)
        y1 = VGA_PIXEL_H;

    /* 지정 영역의 모든 픽셀에 색상 기록 */
    for (y = y0; y < y1; ++y)
    {
        for (x = x0; x < x1; ++x)
        {
            pixel_write(x, y, color);
        }
    }
}

/* =====================================
 * 4. Character Buffer Functions
 * ===================================== */

/*
 * VGA 문자 버퍼에 문자열 출력
 *
 * x, y : 문자 좌표
 * text : 출력할 문자열
 *
 * 일반 텍스트를 화면에 표시할 때 사용한다.
 *
 * 문자 버퍼에서는 한 줄의 메모리 간격이
 * 128바이트이므로 y << 7을 사용한다.
 */
static void text_at(uint32_t x,
                    uint32_t y,
                    const char *text)
{
    /* 문자열이 없거나 y좌표가 잘못되면 종료 */
    if (text == NULL || y >= VGA_CHAR_H)
        return;

    /* 문자열을 문자 버퍼에 한 글자씩 저장 */
    while (*text && x < VGA_CHAR_W)
    {
        volatile uint8_t *addr =
            (volatile uint8_t *)(uintptr_t)(
                VGA_CHAR_BASE +
                (y << 7) +
                x
            );

        /* 현재 문자 저장 후 다음 문자로 이동 */
        *addr = (uint8_t)*text++;

        ++x;
    }
}

/*
 * 지정된 폭의 문자 영역을 갱신
 *
 * 사용 목적:
 *   이전에 출력한 숫자나 문자열의
 *   잔상이 남지 않도록 처리
 *
 * 예:
 *   이전 값: 12345
 *   현재 값: 120
 *
 * 일반 text_at()만 사용하면
 *   12045
 * 처럼 이전 문자가 남을 수 있다.
 *
 * text_field()는 남는 공간을
 * 공백으로 채워 이를 방지한다.
 *
 * 주의:
 *   문자 버퍼에 출력된 내용만 제거한다.
 *   color_word()로 출력한 색상 글자는
 *   픽셀 버퍼에 존재하므로 pixels_rect()를
 *   사용하여 별도로 제거해야 한다.
 */
static void text_field(uint32_t x,
                       uint32_t y,
                       uint32_t width,
                       const char *text)
{
    uint32_t i;

    /* 잘못된 좌표인 경우 종료 */
    if (y >= VGA_CHAR_H || x >= VGA_CHAR_W)
        return;

    /*
     * 지정 폭만큼 출력
     *
     * 문자열이 종료되면 나머지는
     * 공백으로 채워 이전 내용을 제거한다.
     */
    for (i = 0U;
         i < width && x + i < VGA_CHAR_W;
         ++i)
    {
        volatile uint8_t *addr =
            (volatile uint8_t *)(uintptr_t)(
                VGA_CHAR_BASE +
                (y << 7) +
                x + i
            );

        *addr =
            (text != NULL && text[i] != '\0')
                ? (uint8_t)text[i]
                : (uint8_t)' ';

        if (text != NULL && text[i] == '\0')
            text = NULL;
    }
}

/*
 * VGA 문자 버퍼 전체 초기화
 *
 * 80 x 60 문자 영역을 공백으로 채운다.
 *
 * 화면 전환 시 이전 화면의 문자들이
 * 남아 있는 문제를 방지한다.
 */
static void chars_clear(void)
{
    uint32_t x;
    uint32_t y;

    for (y = 0U; y < VGA_CHAR_H; ++y)
    {
        for (x = 0U; x < VGA_CHAR_W; ++x)
        {
            volatile uint8_t *addr =
                (volatile uint8_t *)(uintptr_t)(
                    VGA_CHAR_BASE +
                    (y << 7) +
                    x
                );

            *addr = (uint8_t)' ';
        }
    }
}

/* =====================================
 * 5. Colored Text Font (5x7)
 * ===================================== */

/*
 * RGB565 픽셀 영역에 색상 있는 글자를
 * 직접 출력하기 위한 5x7 글꼴 데이터
 *
 * 문자 버퍼와 달리 픽셀 버퍼를 사용하므로
 * 각 글자의 색상을 개별적으로 지정할 수 있다.
 *
 * 사용 문자열:
 *   LIVE
 *   HISTORY
 *   NORMAL
 *   CAUTION
 *   DANGER
 *
 * 기존 SAVED 표시를 HISTORY로 변경하면서
 * HISTORY 출력에 필요한 H, Y 글꼴을 추가한다.
 *
 * 기존 글꼴은 다른 문자열 출력 및
 * 향후 사용을 위해 그대로 유지한다.
 *
 * 각 배열의 첫 번째 값은 ASCII 문자이며,
 * 이후 7개의 값은 각 행의 픽셀 패턴이다.
 */
static const uint8_t font_letters[][8] = {
    {'A', 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {'C', 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {'D', 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {'E', 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {'G', 0x0F,0x10,0x10,0x17,0x11,0x11,0x0E},

    /* HISTORY 출력을 위한 H 글꼴 추가 */
    {'H', 0x11,0x11,0x11,0x1F,0x11,0x11,0x11},

    {'I', 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
    {'L', 0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {'M', 0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {'N', 0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {'O', 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {'R', 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {'S', 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {'T', 0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {'U', 0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {'V', 0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},

    /* HISTORY 출력을 위한 Y 글꼴 추가 */
    {'Y', 0x11,0x11,0x0A,0x04,0x04,0x04,0x04}
};

/*
 * 5x7 글꼴로 색상 문자열 출력
 *
 * x, y  : 픽셀 좌표
 * word  : 출력할 문자열
 * color : RGB565 색상
 *
 * 글자 데이터의 각 비트를 확인하여
 * 해당 위치에 작은 색상 사각형을 그린다.
 *
 * 각 글자는 가독성을 위해
 * 픽셀을 2배 크기로 확대하여 출력한다.
 *
 * 확대된 글자 크기:
 *   가로 10픽셀
 *   세로 14픽셀
 *
 * 글자 사이의 간격은 2픽셀이므로
 * 다음 글자는 12픽셀 뒤에 출력한다.
 */
static void color_word(uint32_t x,
                       uint32_t y,
                       const char *word,
                       uint16_t color)
{
    uint32_t char_index = 0U;

    if (word == NULL)
        return;

    /* 문자열의 각 문자를 순서대로 처리 */
    while (*word != '\0')
    {
        const uint8_t *glyph = NULL;

        uint32_t i;
        uint32_t px;
        uint32_t py;

        /*
         * 현재 문자에 해당하는
         * 5x7 글꼴 데이터 검색
         */
        for (i = 0U;
             i < sizeof(font_letters) /
                     sizeof(font_letters[0]);
             ++i)
        {
            if (font_letters[i][0] == (uint8_t)*word)
            {
                glyph = font_letters[i];
                break;
            }
        }

        /* 글꼴 데이터가 존재하는 경우 출력 */
        if (glyph != NULL)
        {
            /* 글꼴 세로 7개 행 */
            for (py = 0U; py < 7U; ++py)
            {
                /* 글꼴 가로 5개 열 */
                for (px = 0U; px < 5U; ++px)
                {
                    /*
                     * 현재 위치의 비트가 1이면
                     * 해당 픽셀을 색상으로 출력
                     */
                    if ((glyph[py + 1U] &
                         (1U << (4U - px))) != 0U)
                    {
                        pixels_rect(
                            x + char_index * 12U + 2U * px,
                            y + 2U * py,
                            x + char_index * 12U +
                                2U * px + 2U,
                            y + 2U * py + 2U,
                            color
                        );
                    }
                }
            }
        }

        /* 다음 문자 위치로 이동 */
        ++char_index;
        ++word;
    }
}

/* =====================================
 * 6. Sensor Delta Calculation
 * ===================================== */

/*
 * 실시간 센서 DELTA 계산
 *
 * DELTA = VALUE - LIMIT
 *
 * VALUE : 현재 센서 측정값
 * LIMIT : 초기 5초 평균값
 *
 * 예:
 *   VALUE = 800, LIMIT = 1220
 *   DELTA = -420
 *
 *   VALUE = 1250, LIMIT = 1220
 *   DELTA = +30
 *
 * 절댓값을 사용하지 않고 부호를 유지한다.
 *
 * 센서값은 uint16_t이지만
 * 음수 계산이 가능하도록 int32_t로 변환한다.
 *
 * 이 함수는 실시간 모니터링 화면에서 사용한다.
 *
 * History 화면에서는 이 함수를 사용하지 않고
 * Event History에 저장된 절댓값 변화량을
 * 그대로 조회하여 출력한다.
 */
static int32_t signed_delta_u16(uint16_t value,
                                uint16_t limit)
{
    return (int32_t)value - (int32_t)limit;
}

/* =====================================
 * 7. Screen Background / Layout
 * ===================================== */

/*
 * 현재 선택된 화면의 기본 레이아웃 출력
 *
 * 공통 작업:
 *   1. 픽셀 배경 전체 초기화
 *   2. 문자 버퍼 전체 초기화
 *
 * SENSOR MONITOR 화면:
 *   현재 센서값, 기준값, DELTA, 위험 단계
 *   오른쪽 상단 LIVE 표시
 *
 * LAST EVENT 화면:
 *   최근 이벤트 발생 시각, 종류, DELTA
 *   오른쪽 상단 HISTORY 표시
 *
 * 이 함수는 전체 화면을 다시 그릴 때 호출된다.
 */
static void background_for_screen(void)
{
    /* 픽셀 버퍼 전체를 배경색으로 채움 */
    pixels_rect(
        0U, 0U,
        VGA_PIXEL_W, VGA_PIXEL_H,
        C_BG
    );

    /* 이전 화면의 문자 제거 */
    chars_clear();

    /* =====================================
     * SCREEN 1: SENSOR MONITOR
     * ===================================== */

    if (s_screen == VGA_SCREEN_REALTIME)
    {
        char th[80];

        /* 화면 제목 */
        text_at(3U, 4U, "SENSOR MONITOR");

        /* 시스템 실행 시간 */
        text_at(3U, 10U, "UPTIME:");

        /*
         * 센서 데이터 표 헤더
         *
         * SENSOR : 센서 종류
         * VALUE  : 현재 측정값
         * LIMIT  : 초기 5초 평균값
         * DELTA  : 현재값 - 평균값
         */
        text_at(3U, 17U, "SENSOR");
        text_at(26U, 17U, "VALUE");
        text_at(44U, 17U, "LIMIT");
        text_at(62U, 17U, "DELTA");

        /* 센서별 행 제목 */
        text_at(3U, 24U, "VIBRATION");
        text_at(3U, 34U, "SOUND");
        text_at(3U, 44U, "LIGHT");

        /* 위험 단계 제목 */
        text_at(3U, 53U, "RISK:");

        /*
         * 이벤트 감지 임계값 표시
         *
         * V: 진동
         * S: 소음
         * L: 조도
         *
         * 이 값은 이벤트 감지 기준이며
         * LIMIT와는 별도로 표시한다.
         */
        (void)snprintf(
            th,
            sizeof(th),
            "THRESHOLD: V%u S%u L%u",
            (unsigned)SHOCK_CHANGE_THRESHOLD,
            (unsigned)SOUND_CHANGE_THRESHOLD,
            (unsigned)LIGHT_CHANGE_THRESHOLD
        );

        text_at(3U, 57U, th);

        /* DELTA 계산식 안내 */
        text_at(
            3U,
            59U,
            "DELTA=VALUE-LIMIT"
        );

        /* 화면 상단 구분선 */
        pixels_rect(
            8U, 56U,
            312U, 57U,
            C_LINE
        );

        /* 하단 구분선 */
        pixels_rect(
            8U, 194U,
            312U, 195U,
            C_LINE
        );

        /* 실시간 화면 상태 표시 */
        color_word(
            258U, 12U,
            "LIVE", C_GREEN
        );
    }

    /* =====================================
     * SCREEN 2: LAST EVENT
     * ===================================== */

    else
    {
        /* 화면 제목 */
        text_at(3U, 4U, "LAST EVENT");

        /*
         * 이벤트 History 표 헤더
         *
         * NO.   : 이벤트 순서
         * TIME  : 이벤트 발생 시간
         * TYPE  : 이벤트 종류
         * DELTA : 이벤트 발생 당시 절댓값 변화량
         */
        text_at(3U, 13U, "NO.");
        text_at(17U, 13U, "TIME");
        text_at(39U, 13U, "TYPE");
        text_at(62U, 13U, "DELTA");

        /* History 출력 순서 및 용량 안내 */
        text_at(
            3U, 55U,
            "Newest first / Stores up to 10 events"
        );

        /* 표 상단 구분선 */
        pixels_rect(
            8U, 43U,
            312U, 44U,
            C_LINE
        );

        /*
         * 이벤트 History 화면 표시
         *
         * [수정] 기존 SAVED를 HISTORY로 변경
         *
         * 기존 SAVED는 저장된 이벤트가 없는
         * 경우에도 표시되어 실제 저장 상태로
         * 오해할 가능성이 있었다.
         *
         * HISTORY는 이벤트 저장 완료 여부가
         * 아닌 현재 화면의 종류를 의미한다.
         *
         * 따라서 저장된 이벤트 개수와 관계없이
         * History 화면에서는 항상 표시한다.
         *
         * 실제 이벤트 저장 개수는 화면 하단의
         * STORED 항목을 통해 확인할 수 있다.
         *
         * 출력 좌표 변경:
         *   기존 SAVED   : x = 250
         *   변경 HISTORY : x = 226
         *
         * 기존 5글자에서 7글자로 증가했으므로
         * 글자 간격 12픽셀을 기준으로
         * 시작 위치를 24픽셀 왼쪽으로 이동한다.
         *
         * 기존 녹색과 오른쪽 여백은 유지한다.
         */
        color_word(
            226U, 12U,
            "HISTORY", C_GREEN
        );
    }
}

/* =====================================
 * 8. SCREEN 1: Real-time Monitoring
 * ===================================== */

/*
 * 실시간 모니터링 화면 데이터 갱신
 *
 * 입력:
 *   now_ms : 현재 시스템 시간
 *   risk   : 현재 위험 단계
 *
 * 처리 순서:
 *   1. UPTIME 출력
 *   2. Ring Buffer에서 최신 센서값 조회
 *   3. 센서별 VALUE 출력
 *   4. 초기 평균값 LIMIT 출력
 *   5. VALUE - LIMIT 계산
 *   6. DELTA 출력
 *   7. 위험 단계에 따라 색상 표시
 *
 * 샘플 조회 실패 시:
 *   1. VALUE / LIMIT / DELTA를 ---로 표시
 *   2. 이전 위험 단계의 색상 글자 제거
 *   3. NO SAMPLE 출력
 *   4. 현재 화면 갱신 종료
 *
 * 주의:
 *   센서 데이터는 10ms마다 수집되지만
 *   화면은 1초마다 갱신한다.
 */
static void live_update(uint32_t now_ms,
                        RiskLevel risk)
{
    SensorSample sample;

    /* Ring Buffer에 저장된 샘플 개수 */
    uint32_t count = ring_buffer_count();

    /* 숫자를 문자열로 변환하기 위한 버퍼 */
    char msg[64];

    /*
     * 센서별 현재값과 기준값
     *
     * 배열 인덱스:
     *   0: VIBRATION
     *   1: SOUND
     *   2: LIGHT
     */
    uint16_t vals[3];
    uint16_t bases[3];

    /* 각 센서 데이터를 표시할 문자 행 */
    uint32_t rows[3] = {
        24U, 34U, 44U
    };

    uint32_t i;

    /* =====================================
     * A. UPTIME 출력
     * ===================================== */

    /*
     * 시스템 시간을 ms에서 초 단위로 변환
     *
     * 예:
     *   15300ms -> 15 s
     */
    (void)snprintf(
        msg,
        sizeof(msg),
        "%lu s",
        (unsigned long)(now_ms / 1000U)
    );

    /* 기존 시간 문자열을 지우면서 갱신 */
    text_field(13U, 10U, 16U, msg);

    /* =====================================
     * B. 최신 센서 샘플 조회
     * ===================================== */

    /*
     * Ring Buffer에는 최근 센서 샘플이
     * 시간 순서로 저장되어 있다.
     *
     * count - 1 위치에서
     * 가장 최근 샘플을 조회한다.
     */
    if (count == 0U ||
        !ring_buffer_get(count - 1U, &sample))
    {
        /*
         * 샘플이 없는 경우
         * 센서값을 ---로 표시한다.
         */
        for (i = 0U; i < 3U; ++i)
        {
            text_field(
                26U, rows[i], 12U, "---"
            );

            text_field(
                44U, rows[i], 12U, "---"
            );

            text_field(
                62U, rows[i], 15U, "---"
            );
        }

        /*
         * [추가] 이전 위험 단계의 색상 글자 제거
         *
         * NORMAL / CAUTION / DANGER는
         * color_word()를 사용하여 픽셀 버퍼에
         * 직접 출력한 색상 글자이다.
         *
         * NO SAMPLE은 문자 버퍼에 출력되므로
         * text_field()만 호출해서는 기존의
         * 색상 글자를 제거할 수 없다.
         *
         * 기존 코드에서는 샘플 조회 실패 시
         * NO SAMPLE을 출력하고 즉시 return하여
         * 함수 아래쪽의 위험 단계 픽셀 초기화
         * 코드가 실행되지 않았다.
         *
         * 따라서 기존 위험 단계 글자가 남아
         * NO SAMPLE과 겹칠 가능성이 있었다.
         *
         * 이를 방지하기 위해 return 이전에
         * 기존 위험 단계 출력 영역을
         * 화면 배경색으로 초기화한다.
         *
         * 초기화 좌표는 아래 F. RISK Color
         * Display의 기존 좌표와 동일하다.
         */
        pixels_rect(
            100U, 204U,
            230U, 225U,
            C_BG
        );

        /*
         * 센서 샘플 없음 표시
         *
         * 기존 문자 영역을 공백으로 채우면서
         * NO SAMPLE을 출력한다.
         *
         * 다음 갱신에서 정상적으로 샘플을
         * 조회하면 기존 위험 단계 출력 과정에서
         * NO SAMPLE이 제거된다.
         */
        text_field(
            22U, 53U, 20U, "NO SAMPLE"
        );

        /*
         * 유효한 샘플이 없으므로
         * VALUE / LIMIT / DELTA 계산 및
         * 위험 단계 출력은 수행하지 않는다.
         */
        return;
    }

    /* =====================================
     * C. VALUE 설정
     * ===================================== */

    /*
     * 최신 SensorSample에서
     * 센서별 현재 측정값을 가져온다.
     */
    vals[0] = sample.vibration;
    vals[1] = sample.sound_level;
    vals[2] = sample.light;

    /* =====================================
     * D. LIMIT 설정
     * ===================================== */

    /*
     * main.c에서 초기 5초 보정 후
     * vga_set_baseline()으로 전달한 값
     *
     * LIMIT은 이벤트 감지 임계값이 아니라
     * 초기 5초 동안 계산한 평균값이다.
     */
    bases[0] = s_base_v;
    bases[1] = s_base_s;
    bases[2] = s_base_l;

    /* =====================================
     * E. VALUE / LIMIT / DELTA 출력
     * ===================================== */

    /*
     * 세 센서를 순서대로 처리한다.
     *
     * i = 0: 진동
     * i = 1: 소음
     * i = 2: 조도
     */
    for (i = 0U; i < 3U; ++i)
    {
        /* -----------------------------
         * 1. VALUE 출력
         * ----------------------------- */

        /* 센서 현재값을 문자열로 변환 */
        (void)snprintf(
            msg,
            sizeof(msg),
            "%u",
            (unsigned)vals[i]
        );

        /* VALUE 열에 출력 */
        text_field(
            26U, rows[i], 12U, msg
        );

        /*
         * 초기 보정이 완료된 경우에만
         * LIMIT 및 DELTA를 계산한다.
         */
        if (s_have_baseline)
        {
            int32_t delta;

            /* -------------------------
             * 2. LIMIT 출력
             * ------------------------- */

            /* 초기 평균값을 문자열로 변환 */
            (void)snprintf(
                msg,
                sizeof(msg),
                "%u",
                (unsigned)bases[i]
            );

            /* LIMIT 열에 출력 */
            text_field(
                44U, rows[i], 12U, msg
            );

            /* -------------------------
             * 3. DELTA 계산
             * ------------------------- */

            /*
             * DELTA = 현재값 - 초기 평균값
             *
             * 양수:
             *   센서값이 초기 평균보다 증가
             *
             * 음수:
             *   센서값이 초기 평균보다 감소
             *
             * 0:
             *   센서값이 초기 평균과 동일
             *
             * 이벤트 임계값을 차감하지 않는다.
             */
            delta = signed_delta_u16(
                vals[i],
                bases[i]
            );

            /* -------------------------
             * 4. DELTA 출력
             * ------------------------- */

            /*
             * %+ld 형식을 사용하여
             * 양수에는 +, 음수에는 -를 표시
             *
             * 예:
             *   +30
             *   -420
             *   +0
             *
             * 실시간 모니터링 화면에서는
             * 센서값의 증가 및 감소 방향을
             * 확인할 수 있도록 부호를 유지한다.
             */
            (void)snprintf(
                msg,
                sizeof(msg),
                "%+ld",
                (long)delta
            );

            /* DELTA 열에 출력 */
            text_field(
                62U, rows[i], 15U, msg
            );
        }
        else
        {
            /* -------------------------
             * 초기 보정 진행 중
             * ------------------------- */

            /*
             * 아직 기준값이 없으므로
             * LIMIT와 DELTA를 계산하지 않는다.
             */
            text_field(
                44U, rows[i], 12U, "CAL..."
            );

            text_field(
                62U, rows[i], 15U, "---"
            );
        }
    }

    /* =====================================
     * F. RISK Color Display
     * ===================================== */

    /*
     * 위험 단계는 main.c에서 계산한
     * RiskLevel 값을 사용한다.
     *
     * NORMAL  : 녹색
     * CAUTION : 노란색
     * DANGER  : 빨간색
     */

    /*
     * 이전 위험 단계의 색상 글자를 제거
     *
     * 위험 단계가 변경될 때
     * 이전 글자가 겹치는 현상을 방지한다.
     *
     * 샘플 조회에 실패한 경우에도
     * 동일한 픽셀 영역을 초기화하도록
     * B. 최신 센서 샘플 조회 부분에
     * 별도의 초기화 코드를 추가하였다.
     */
    pixels_rect(
        100U, 204U,
        230U, 225U,
        C_BG
    );

    /*
     * 기존 문자 영역 초기화
     *
     * 이전에 표시된 CALIBRATING 또는
     * NO SAMPLE 문구를 제거한다.
     *
     * 이후 현재 상태에 따라
     * 새로운 위험 단계 표시를 출력한다.
     */
    text_field(22U, 53U, 17U, "");

    /* 아직 보정 중인 경우 */
    if (!s_have_baseline)
    {
        text_at(
            22U, 53U,
            "CALIBRATING"
        );
    }

    /* 위험 단계: DANGER */
    else if (risk == RISK_DANGER)
    {
        color_word(
            108U, 207U,
            "DANGER", C_RED
        );
    }

    /* 주의 단계: CAUTION */
    else if (risk == RISK_CAUTION)
    {
        color_word(
            108U, 207U,
            "CAUTION", C_YELLOW
        );
    }

    /* 정상 단계: NORMAL */
    else
    {
        color_word(
            108U, 207U,
            "NORMAL", C_GREEN
        );
    }
}

/* =====================================
 * 9. SCREEN 2: Event History
 * ===================================== */

/*
 * 이벤트 플래그를 화면용 문자열로 변환
 *
 * EVENT_SHOCK -> SHOCK
 * EVENT_SOUND -> SOUND
 * EVENT_LIGHT -> LIGHT
 *
 * 여러 센서가 함께 포함된 복합 이벤트는
 * MULTIPLE로 표시한다.
 *
 * 실제 이벤트 플래그 값은
 * Event History 내부에 그대로 유지된다.
 */
static const char *event_type_name(uint8_t flags)
{
    if (flags == EVENT_SHOCK)
        return "SHOCK";

    if (flags == EVENT_SOUND)
        return "SOUND";

    if (flags == EVENT_LIGHT)
        return "LIGHT";

    return "MULTIPLE";
}

/*
 * 저장된 Event History를 VGA에 출력
 *
 * 처리 순서:
 *   1. History 저장 개수 조회
 *   2. 최신 이벤트부터 최대 10개 조회
 *   3. 이벤트 번호 출력
 *   4. 발생 시각 출력
 *   5. 이벤트 종류 출력
 *   6. 저장된 절댓값 DELTA 출력
 *   7. 총 저장 개수 출력
 *
 * 이 함수에서는 이벤트를 새로 저장하지 않는다.
 * 저장되어 있는 데이터만 조회한다.
 *
 * 화면 오른쪽 상단의 HISTORY는
 * 현재 화면 종류를 나타내는 고정 표시이다.
 *
 * 실제 이벤트 저장 개수는
 * 화면 하단의 STORED 항목에서 확인한다.
 */
static void history_update(void)
{
    /* 현재 History에 저장된 이벤트 개수 */
    uint32_t count = event_history_count();

    uint32_t i;

    /* 숫자 및 시간 변환용 문자열 버퍼 */
    char msg[40];

    /*
     * 최대 10개 이벤트 출력
     *
     * event_history_get(0)은
     * 가장 최근 이벤트를 반환한다.
     */
    for (i = 0U;
         i < EVENT_HISTORY_CAPACITY;
         ++i)
    {
        /*
         * 이벤트별 출력 위치
         *
         * 시작 행: 18
         * 이벤트마다 3행 간격 사용
         */
        uint32_t row = 18U + i * 3U;

        EventHistoryEntry entry;

        /* -----------------------------
         * A. 이벤트 번호 출력
         * ----------------------------- */

        /*
         * 화면 번호는 1~10으로 표시한다.
         * 가장 최근 이벤트가 1번이다.
         */
        (void)snprintf(
            msg,
            sizeof(msg),
            "%2lu",
            (unsigned long)(i + 1U)
        );

        text_field(3U, row, 4U, msg);

        /* -----------------------------
         * B. 이벤트 데이터 조회
         * ----------------------------- */

        /*
         * 현재 인덱스에 저장된 이벤트가
         * 존재하는 경우만 데이터를 출력한다.
         */
        if (i < count &&
            event_history_get(i, &entry))
        {
            /* -------------------------
             * 1. TIME
             * ------------------------- */

            /*
             * 이벤트 발생 시각을
             * 초.밀리초 형식으로 출력
             *
             * 예:
             *   12345ms -> 12.345 s
             */
            (void)snprintf(
                msg,
                sizeof(msg),
                "%lu.%03lu s",
                (unsigned long)(
                    entry.timestamp_ms / 1000U
                ),
                (unsigned long)(
                    entry.timestamp_ms % 1000U
                )
            );

            text_field(
                17U, row, 18U, msg
            );

            /* -------------------------
             * 2. TYPE
             * ------------------------- */

            /*
             * EVENT_SHOCK / SOUND / LIGHT
             * 플래그를 문자열로 변환한다.
             */
            text_field(
                39U, row, 18U,
                event_type_name(entry.event_flags)
            );

            /* -------------------------
             * 3. DELTA
             * ------------------------- */

            /*
             * History DELTA는 이벤트 발생 시
             * 저장된 절댓값 변화량을 사용한다.
             *
             * 실시간 화면과 달리 센서값의
             * 증가 및 감소 방향을 구분하지 않고
             * 변화량의 크기만 표시한다.
             *
             * 단일 이벤트:
             *   해당 센서의 절댓값 변화량
             *
             * 복합 이벤트:
             *   해당 센서들의 절댓값 변화량 중
             *   가장 큰 값
             *
             * 기존에는 절댓값 변화량 앞에
             * + 부호를 붙여 출력하였다.
             *
             * 이 경우 센서값이 초기 평균보다
             * 감소한 이벤트도 증가한 것으로
             * 오해할 가능성이 있었다.
             *
             * 따라서 + 부호를 제거하고
             * %u 형식으로 숫자만 출력한다.
             *
             * 예:
             *   실시간 DELTA: -420
             *   History DELTA: 420
             *
             *   실시간 DELTA: +350
             *   History DELTA: 350
             *
             * History의 기존 저장 자료형 및
             * 변화량 계산 방식은 변경하지 않는다.
             */
            (void)snprintf(
                msg,
                sizeof(msg),
                "%u",
                (unsigned)entry.delta
            );

            text_field(
                62U, row, 15U, msg
            );
        }
        else
        {
            /* -------------------------
             * 저장되지 않은 항목
             * ------------------------- */

            /*
             * 아직 이벤트가 10개 미만일 경우
             * 빈 항목을 ---로 표시한다.
             */
            text_field(
                17U, row, 18U, "---"
            );

            text_field(
                39U, row, 18U, "---"
            );

            text_field(
                62U, row, 15U, "---"
            );
        }
    }

    /* =====================================
     * C. History 저장 개수 표시
     * ===================================== */

    /*
     * 현재 이벤트 개수와 최대 저장 개수 출력
     *
     * 예:
     *   STORED: 0 / 10
     *   STORED: 7 / 10
     *
     * 오른쪽 상단의 HISTORY 문구는
     * 현재 화면 종류를 나타내므로
     * 저장 개수와 관계없이 유지된다.
     *
     * 실제 History 저장 상태는
     * STORED 값을 통해 확인할 수 있다.
     */
    (void)snprintf(
        msg,
        sizeof(msg),
        "STORED: %lu / %u",
        (unsigned long)count,
        (unsigned)EVENT_HISTORY_CAPACITY
    );

    text_field(3U, 57U, 30U, msg);
}

/* =====================================
 * 10. Public VGA API
 * ===================================== */

/*
 * VGA 초기화 함수
 *
 * main.c에서 시스템 시작 시 한 번 호출
 *
 * 초기 상태:
 *   - 실시간 모니터링 화면 선택
 *   - VGA 갱신 상태 초기화
 *   - 초기 센서 기준값 없음
 *   - 화면 배경 출력
 *   - 최초 화면 데이터 출력
 */
void vga_init(void)
{
    /* 기본 화면: 실시간 센서 모니터링 */
    s_screen = VGA_SCREEN_REALTIME;

    /* 전체 화면을 다시 그릴 상태로 초기화 */
    s_full_redraw = true;

    /* 아직 주기적인 화면 갱신 이력 없음 */
    s_have_update = false;

    /* 마지막 갱신 시각 초기화 */
    s_last_update_ms = 0U;

    /* 초기 센서 보정값 없음 */
    s_have_baseline = false;

    /* 센서 기준값 초기화 */
    s_base_v = 0U;
    s_base_l = 0U;
    s_base_s = 0U;

    /* 실시간 모니터링 화면 기본 구성 출력 */
    background_for_screen();

    /* 초기 배경 출력 완료 */
    s_full_redraw = false;

    /*
     * 최초 실시간 화면 데이터 출력
     *
     * 보정 전이므로 LIMIT / DELTA는
     * 아직 정상 수치로 표시되지 않는다.
     *
     * Ring Buffer에 샘플이 없다면
     * 센서값을 ---로 표시하고
     * 위험 단계 위치에는 NO SAMPLE을 출력한다.
     */
    live_update(0U, RISK_NORMAL);
}

/*
 * VGA 화면 선택 함수
 *
 * main.c에서 KEY 입력에 따라 호출
 *
 * KEY2 -> VGA_SCREEN_REALTIME
 * KEY3 -> VGA_SCREEN_HISTORY
 *
 * 다른 화면을 선택하면
 * 전체 화면 다시 그리기 요청
 *
 * 같은 화면을 다시 선택하면
 * 데이터 즉시 새로고침 요청
 */
void vga_set_screen(VgaScreen screen)
{
    /* 지원하지 않는 화면 번호는 무시 */
    if (screen != VGA_SCREEN_REALTIME &&
        screen != VGA_SCREEN_HISTORY)
    {
        return;
    }

    /*
     * 이미 선택된 화면인 경우
     *
     * 전체 배경을 다시 그리지 않고
     * 데이터만 새로고침하도록 설정한다.
     */
    if (screen == s_screen)
    {
        s_have_update = false;
        return;
    }

    /* 새 화면 선택 */
    s_screen = screen;

    /* 화면 전환 시 전체 레이아웃 다시 출력 */
    s_full_redraw = true;
}

/*
 * 초기 센서 기준값 설정
 *
 * main.c에서 초기 5초 보정 완료 후 호출
 *
 * 매개변수:
 *   vibration_base : 진동 평균값
 *   light_base     : 조도 평균값
 *   sound_base     : 소음 평균값
 *
 * 전달된 값은 실시간 화면에서
 * LIMIT 값으로 사용한다.
 */
void vga_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base)
{
    /* 초기 센서 평균값 저장 */
    s_base_v = vibration_base;
    s_base_l = light_base;
    s_base_s = sound_base;

    /* 보정 완료 상태 설정 */
    s_have_baseline = true;

    /*
     * 보정 완료 후 기존 CAL... 표시를
     * 실제 평균값으로 갱신하도록 요청
     */
    s_full_redraw = true;
}

/*
 * VGA 메인 갱신 함수
 *
 * main.c의 while(1)에서 반복 호출
 *
 * 입력:
 *   now_ms : 현재 시스템 시간
 *   risk   : 현재 위험 단계
 *
 * 처리 순서:
 *   1. 전체 화면 재출력 필요 여부 확인
 *   2. 1초 갱신 주기 확인
 *   3. 현재 선택된 화면의 데이터 갱신
 *
 * 실시간 화면:
 *   live_update()
 *
 * History 화면:
 *   history_update()
 *
 * 화면 전환 시에는 1초를 기다리지 않고
 * 바로 화면 데이터를 갱신한다.
 */
void vga_update(uint32_t now_ms,
                RiskLevel risk)
{
    /* =====================================
     * A. 전체 화면 다시 그리기
     * ===================================== */

    /*
     * 화면 전환 또는 기준값 설정으로 인해
     * 전체 레이아웃 갱신이 필요한 경우
     */
    if (s_full_redraw)
    {
        /* 현재 화면의 배경과 제목 다시 출력 */
        background_for_screen();

        /* 전체 갱신 완료 */
        s_full_redraw = false;

        /*
         * 화면 전환 직후 데이터를
         * 즉시 출력할 수 있도록 설정
         */
        s_have_update = false;
    }

    /* =====================================
     * B. 1초 갱신 주기 확인
     * ===================================== */

    /*
     * 이전 갱신 이후 1000ms가 지나지 않았고
     * 강제 갱신 요청도 없다면 종료한다.
     *
     * 센서 샘플링은 10ms 주기로 계속되지만
     * VGA 데이터 갱신은 1초 주기로 수행한다.
     */
    if (s_have_update &&
        (uint32_t)(
            now_ms - s_last_update_ms
        ) < VGA_REFRESH_INTERVAL_MS)
    {
        return;
    }

    /* 마지막 갱신 시각 저장 */
    s_last_update_ms = now_ms;

    /* 이번 갱신 완료 상태 표시 */
    s_have_update = true;

    /* =====================================
     * C. 현재 화면 갱신
     * ===================================== */

    /*
     * KEY2로 선택한 실시간 모니터링 화면
     */
    if (s_screen == VGA_SCREEN_REALTIME)
    {
        live_update(now_ms, risk);
    }

    /*
     * KEY3로 선택한 과거 이벤트 History 화면
     */
    else
    {
        history_update();
    }
}