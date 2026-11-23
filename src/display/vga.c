
#include "../../include/vga.h"
#include "../../include/project_types.h"
#include "../../include/system_config.h"
#include "../../include/ring_buffer.h"
#include "../../include/event_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/* =====================================
 * DE1-SoC Computer / Nios II VGA
 * ===================================== */

#define VGA_PIXEL_BASE 0x08000000U
#define VGA_CHAR_BASE  0x09000000U

#define VGA_PIXEL_W 320U
#define VGA_PIXEL_H 240U

#define VGA_CHAR_W 80U
#define VGA_CHAR_H 60U

/* VGA 화면 갱신 주기: 1초 */
#define VGA_REFRESH_INTERVAL_MS 1000U

/* RGB565 Colors */
#define C_BG      0x0023U
#define C_LINE    0x5AEBU
#define C_GREEN   0x07E0U
#define C_YELLOW  0xFFE0U
#define C_RED     0xF800U

/* =====================================
 * VGA Internal State
 * ===================================== */

static VgaScreen s_screen;

static bool s_full_redraw;
static bool s_have_update;

static uint32_t s_last_update_ms;

static bool s_have_baseline;

static uint16_t s_base_v;
static uint16_t s_base_l;
static uint16_t s_base_s;

/* =====================================
 * Pixel Buffer Functions
 * ===================================== */

static void pixel_write(uint32_t x,
                        uint32_t y,
                        uint16_t color)
{
    volatile uint16_t *addr;

    if (x >= VGA_PIXEL_W || y >= VGA_PIXEL_H)
        return;

    addr = (volatile uint16_t *)(uintptr_t)(
        VGA_PIXEL_BASE +
        (y << 10) +
        (x << 1)
    );

    *addr = color;
}

static void pixels_rect(uint32_t x0,
                        uint32_t y0,
                        uint32_t x1,
                        uint32_t y1,
                        uint16_t color)
{
    uint32_t x;
    uint32_t y;

    if (x1 > VGA_PIXEL_W)
        x1 = VGA_PIXEL_W;

    if (y1 > VGA_PIXEL_H)
        y1 = VGA_PIXEL_H;

    for (y = y0; y < y1; ++y)
    {
        for (x = x0; x < x1; ++x)
        {
            pixel_write(x, y, color);
        }
    }
}

/* =====================================
 * Character Buffer Functions
 * ===================================== */

static void text_at(uint32_t x,
                    uint32_t y,
                    const char *text)
{
    if (text == NULL || y >= VGA_CHAR_H)
        return;

    while (*text && x < VGA_CHAR_W)
    {
        volatile uint8_t *addr =
            (volatile uint8_t *)(uintptr_t)(
                VGA_CHAR_BASE +
                (y << 7) +
                x
            );

        *addr = (uint8_t)*text++;

        ++x;
    }
}

/* 기존 숫자 잔상을 지우면서 갱신 */
static void text_field(uint32_t x,
                       uint32_t y,
                       uint32_t width,
                       const char *text)
{
    uint32_t i;

    if (y >= VGA_CHAR_H || x >= VGA_CHAR_W)
        return;

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
 * Colored Text Font (5x7)
 *
 * LIVE / SAVED
 * NORMAL / CAUTION / DANGER
 * ===================================== */

static const uint8_t font_letters[][8] = {
    {'A', 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {'C', 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {'D', 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {'E', 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {'G', 0x0F,0x10,0x10,0x17,0x11,0x11,0x0E},
    {'I', 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
    {'L', 0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {'M', 0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {'N', 0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {'O', 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {'R', 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {'S', 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {'T', 0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {'U', 0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {'V', 0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}
};

/* Pixel Buffer에 색상 있는 글자 출력 */
static void color_word(uint32_t x,
                       uint32_t y,
                       const char *word,
                       uint16_t color)
{
    uint32_t char_index = 0U;

    if (word == NULL)
        return;

    while (*word != '\0')
    {
        const uint8_t *glyph = NULL;

        uint32_t i;
        uint32_t px;
        uint32_t py;

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

        if (glyph != NULL)
        {
            for (py = 0U; py < 7U; ++py)
            {
                for (px = 0U; px < 5U; ++px)
                {
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

        ++char_index;
        ++word;
    }
}

/* =====================================
 * Sensor Delta
 * ===================================== */

static uint16_t diff_u16(uint16_t a,
                         uint16_t b)
{
    return (a >= b)
        ? (uint16_t)(a - b)
        : (uint16_t)(b - a);
}

/* =====================================
 * Draw Screen Layout
 * ===================================== */

static void background_for_screen(void)
{
    pixels_rect(
        0U, 0U,
        VGA_PIXEL_W, VGA_PIXEL_H,
        C_BG
    );

    chars_clear();

    if (s_screen == VGA_SCREEN_REALTIME)
    {
        char th[80];

        text_at(3U, 4U, "SENSOR MONITOR");
        text_at(3U, 10U, "UPTIME:");

        text_at(3U, 17U, "SENSOR");
        text_at(26U, 17U, "VALUE");
        text_at(44U, 17U, "LIMIT");
        text_at(62U, 17U, "EXCESS");

        text_at(3U, 24U, "VIBRATION");
        text_at(3U, 34U, "SOUND");
        text_at(3U, 44U, "LIGHT");

        text_at(3U, 53U, "RISK:");

        (void)snprintf(
            th,
            sizeof(th),
            "THRESHOLD: V%u S%u L%u",
            (unsigned)SHOCK_CHANGE_THRESHOLD,
            (unsigned)SOUND_CHANGE_THRESHOLD,
            (unsigned)LIGHT_CHANGE_THRESHOLD
        );

        text_at(3U, 57U, th);

        text_at(
            3U,
            59U,
            "EXCESS=max(abs(VALUE-LIMIT)-THRESHOLD,0)"
        );

        pixels_rect(
            8U, 56U,
            312U, 57U,
            C_LINE
        );

        pixels_rect(
            8U, 194U,
            312U, 195U,
            C_LINE
        );

        color_word(
            258U, 12U,
            "LIVE", C_GREEN
        );
    }
    else
    {
        text_at(3U, 4U, "LAST EVENT");

        text_at(3U, 13U, "NO.");
        text_at(17U, 13U, "TIME");
        text_at(39U, 13U, "TYPE");
        text_at(62U, 13U, "DELTA");

        text_at(
            3U, 55U,
            "Newest first / Stores up to 10 events"
        );

        pixels_rect(
            8U, 43U,
            312U, 44U,
            C_LINE
        );

        color_word(
            250U, 12U,
            "SAVED", C_GREEN
        );
    }
}

/* =====================================
 * SCREEN 1: Real-time Monitoring
 * ===================================== */

static void live_update(uint32_t now_ms,
                        RiskLevel risk)
{
    SensorSample sample;

    uint32_t count = ring_buffer_count();

    char msg[64];

    uint16_t vals[3];
    uint16_t bases[3];
    uint16_t thresholds[3];

    uint32_t rows[3] = {
        24U, 34U, 44U
    };

    uint32_t i;

    /* UPTIME */
    (void)snprintf(
        msg,
        sizeof(msg),
        "%lu s",
        (unsigned long)(now_ms / 1000U)
    );

    text_field(13U, 10U, 16U, msg);

    /* 측정값이 아직 없는 경우 */
    if (count == 0U ||
        !ring_buffer_get(count - 1U, &sample))
    {
        for (i = 0U; i < 3U; ++i)
        {
            text_field(26U, rows[i], 12U, "---");
            text_field(44U, rows[i], 12U, "---");
            text_field(62U, rows[i], 15U, "---");
        }

        text_field(22U, 53U, 20U, "NO SAMPLE");
        return;
    }

    /* Sensor VALUE */
    vals[0] = sample.vibration;
    vals[1] = sample.sound_level;
    vals[2] = sample.light;

    /* 초기 5초 평균값: LIMIT */
    bases[0] = s_base_v;
    bases[1] = s_base_s;
    bases[2] = s_base_l;

    /* 이벤트 감지 임계값 */
    thresholds[0] = SHOCK_CHANGE_THRESHOLD;
    thresholds[1] = SOUND_CHANGE_THRESHOLD;
    thresholds[2] = LIGHT_CHANGE_THRESHOLD;

    for (i = 0U; i < 3U; ++i)
    {
        uint32_t diff =
            diff_u16(vals[i], bases[i]);

        uint32_t excess =
            (diff > thresholds[i])
                ? (diff - thresholds[i])
                : 0U;

        /* VALUE */
        (void)snprintf(
            msg,
            sizeof(msg),
            "%u",
            (unsigned)vals[i]
        );

        text_field(26U, rows[i], 12U, msg);

        if (s_have_baseline)
        {
            /* LIMIT */
            (void)snprintf(
                msg,
                sizeof(msg),
                "%u",
                (unsigned)bases[i]
            );

            text_field(44U, rows[i], 12U, msg);

            /* EXCESS */
            (void)snprintf(
                msg,
                sizeof(msg),
                "+%lu",
                (unsigned long)excess
            );

            text_field(62U, rows[i], 15U, msg);
        }
        else
        {
            text_field(
                44U, rows[i], 12U, "CAL..."
            );

            text_field(
                62U, rows[i], 15U, "---"
            );
        }
    }

    /* =====================================
     * RISK: Color Display
     * ===================================== */

    /* 이전 위험도 색상 글자 제거 */
    pixels_rect(
        100U, 204U,
        230U, 225U,
        C_BG
    );

    text_field(22U, 53U, 17U, "");

    if (!s_have_baseline)
    {
        text_at(
            22U, 53U,
            "CALIBRATING"
        );
    }
    else if (risk == RISK_DANGER)
    {
        color_word(
            108U, 207U,
            "DANGER", C_RED
        );
    }
    else if (risk == RISK_CAUTION)
    {
        color_word(
            108U, 207U,
            "CAUTION", C_YELLOW
        );
    }
    else
    {
        color_word(
            108U, 207U,
            "NORMAL", C_GREEN
        );
    }
}

/* =====================================
 * SCREEN 2: Event History
 * ===================================== */

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

/* History 저장 내용을 다시 조회하여 출력 */
static void history_update(void)
{
    uint32_t count = event_history_count();
    uint32_t i;

    char msg[40];

    for (i = 0U;
         i < EVENT_HISTORY_CAPACITY;
         ++i)
    {
        uint32_t row = 18U + i * 3U;

        EventHistoryEntry entry;

        /* 1~10 번호 */
        (void)snprintf(
            msg,
            sizeof(msg),
            "%2lu",
            (unsigned long)(i + 1U)
        );

        text_field(3U, row, 4U, msg);

        /* 실제 저장된 이벤트 조회 */
        if (i < count &&
            event_history_get(i, &entry))
        {
            /* TIME */
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

            /* TYPE */
            text_field(
                39U, row, 18U,
                event_type_name(entry.event_flags)
            );

            /* DELTA */
            (void)snprintf(
                msg,
                sizeof(msg),
                "+%u",
                (unsigned)entry.delta
            );

            text_field(
                62U, row, 15U, msg
            );
        }
        else
        {
            /* 아직 저장되지 않은 항목 */
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
 * Public VGA API
 * ===================================== */

void vga_init(void)
{
    s_screen = VGA_SCREEN_REALTIME;

    s_full_redraw = true;
    s_have_update = false;

    s_last_update_ms = 0U;

    s_have_baseline = false;

    s_base_v = 0U;
    s_base_l = 0U;
    s_base_s = 0U;

    background_for_screen();

    s_full_redraw = false;

    live_update(0U, RISK_NORMAL);
}

/* KEY2 / KEY3 화면 선택 */
void vga_set_screen(VgaScreen screen)
{
    if (screen != VGA_SCREEN_REALTIME &&
        screen != VGA_SCREEN_HISTORY)
    {
        return;
    }

    /*
     * 현재 화면을 다시 선택한 경우에도
     * 저장된 정보를 즉시 새로고침
     */
    if (screen == s_screen)
    {
        s_have_update = false;
        return;
    }

    s_screen = screen;
    s_full_redraw = true;
}

/* 초기 5초 평균값 설정 */
void vga_set_baseline(
    uint16_t vibration_base,
    uint16_t light_base,
    uint16_t sound_base)
{
    s_base_v = vibration_base;
    s_base_l = light_base;
    s_base_s = sound_base;

    s_have_baseline = true;
    s_full_redraw = true;
}

/* 메인 루프에서 계속 호출 */
void vga_update(uint32_t now_ms,
                RiskLevel risk)
{
    if (s_full_redraw)
    {
        background_for_screen();

        s_full_redraw = false;
        s_have_update = false;
    }

    /* 아직 갱신 주기가 지나지 않았으면 종료 */
    if (s_have_update &&
        (uint32_t)(
            now_ms - s_last_update_ms
        ) < VGA_REFRESH_INTERVAL_MS)
    {
        return;
    }

    s_last_update_ms = now_ms;
    s_have_update = true;

    if (s_screen == VGA_SCREEN_REALTIME)
    {
        live_update(now_ms, risk);
    }
    else
    {
        history_update();
    }
}
