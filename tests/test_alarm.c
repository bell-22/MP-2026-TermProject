
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "alarm.h"
#include "alarm_audio_driver.h"
#include "system_config.h"

/* Mac 테스트용 가상 Audio-out */
static uint32_t s_mock_frequency_hz;

/* Audio 드라이버 호출 횟수 */
static unsigned s_driver_calls;

/* 테스트 성공 / 실패 개수 */
static unsigned s_pass;
static unsigned s_fail;


/* 실제 Audio-out 대신 동작하는 Mock 함수 */
void alarm_audio_set_frequency(uint32_t frequency_hz)
{
    s_mock_frequency_hz = frequency_hz;
    s_driver_calls++;
}


/* 현재 주파수 검사 */
static void check_freq(
    const char *name,
    uint32_t expected_hz
)
{
    if (s_mock_frequency_hz == expected_hz) {

        printf("[PASS] %-32s freq=%uHz\n",
            name, (unsigned)expected_hz);
        s_pass++;

    } else {

        printf(
            "[FAIL] %-32s expected=%uHz actual=%uHz\n",
            name,
            (unsigned)expected_hz,
            (unsigned)s_mock_frequency_hz
        );

        s_fail++;
    }
}


/* 경고 진행 상태 검사 */
static void check_active(
    const char *name,
    bool expected
)
{
    if (alarm_is_active() == expected) {

        printf("[PASS] %-32s active=%u\n",
            name, (unsigned)expected);
        s_pass++;

    } else {

        printf(
            "[FAIL] %-32s expected=%u actual=%u\n",
            name,
            (unsigned)expected,
            (unsigned)alarm_is_active()
        );

        s_fail++;
    }
}


/* 기타 조건 검사 */
static void check_true(
    const char *name,
    bool condition
)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        s_pass++;
    } else {
        printf("[FAIL] %s\n", name);
        s_fail++;
    }
}


int main(void)
{
    unsigned before;

    printf("=== Alarm Frequency Pattern Mock Test ===\n");

    /* 1. 초기화 */
    alarm_init();

    check_active("init inactive", false);
    check_freq("init muted", 0U);


    /* 2. 정상 이벤트 */
    alarm_start(DISPLAY_NONE);
    alarm_update(0U);

    check_active("NONE ignored", false);


    /* 3. 잘못된 이벤트 코드 */
    alarm_start(99U);

    check_active("invalid code ignored", false);


    /* 4. 충격: 800Hz 200ms 1회 */
    alarm_start(DISPLAY_SHOCK);

    check_active("shock pending", true);

    alarm_update(100U);
    check_freq("shock start", 800U);

    before = s_driver_calls;

    alarm_update(199U);

    check_true("no duplicate driver call",
               s_driver_calls == before);

    alarm_update(299U);
    check_freq("shock 199ms", 800U);

    alarm_update(300U);
    check_freq("shock end", 0U);

    check_active("shock inactive", false);


    /* 5. 소음: 800Hz 200ms 2회 */
    alarm_start(DISPLAY_SOUND);

    alarm_update(1000U);
    check_freq("sound first beep", 800U);

    alarm_update(1200U);
    check_freq("sound gap", 0U);

    check_active("sound gap still active", true);

    alarm_update(1300U);
    check_freq("sound second beep", 800U);

    alarm_update(1500U);
    check_freq("sound end", 0U);

    check_active("sound inactive", false);


    /* 6. 조도: 800Hz 200ms 3회 */
    alarm_start(DISPLAY_LIGHT);

    alarm_update(2000U);
    check_freq("light first beep", 800U);

    alarm_update(2200U);
    check_freq("light first gap", 0U);

    alarm_update(2300U);
    check_freq("light second beep", 800U);

    alarm_update(2500U);
    check_freq("light second gap", 0U);

    alarm_update(2600U);
    check_freq("light third beep", 800U);

    alarm_update(2800U);
    check_freq("light end", 0U);

    check_active("light inactive", false);


    /*
     * 7. 복합 이벤트
     *
     * 600Hz 400ms
     * 1200Hz 400ms
     */
    alarm_start(DISPLAY_MULTIPLE);

    alarm_update(3000U);
    check_freq("multiple low start", 600U);

    alarm_update(3399U);
    check_freq("multiple low 399ms", 600U);

    alarm_update(3400U);
    check_freq("multiple high start", 1200U);

    alarm_update(3799U);
    check_freq("multiple high 399ms", 1200U);

    alarm_update(3800U);
    check_freq("multiple end", 0U);

    check_active("multiple inactive", false);


    /* 8. KEY0 경고음 강제 중지 */
    alarm_start(DISPLAY_LIGHT);

    alarm_update(5000U);
    alarm_stop();

    check_freq("KEY0 stop muted", 0U);
    check_active("KEY0 stop inactive", false);

    alarm_update(5500U);
    check_freq("stopped alarm stays muted", 0U);


    /* 9. 경고 중 재시작 방지 */
    alarm_start(DISPLAY_SOUND);

    alarm_update(6000U);

    /* 진행 중 다른 이벤트 발생 */
    alarm_start(DISPLAY_MULTIPLE);

    alarm_update(6200U);
    check_freq("restart ignored during gap", 0U);

    alarm_update(6300U);
    check_freq("original sound second beep", 800U);

    alarm_update(6500U);
    check_active("original sound end", false);


    /* 10. 긴 시간 경과 */
    alarm_start(DISPLAY_LIGHT);

    alarm_update(7000U);
    alarm_update(10000U);

    check_freq("long jump muted", 0U);
    check_active("long jump ended", false);


    /* 11. Timer 오버플로 */
    alarm_start(DISPLAY_SHOCK);

    alarm_update(UINT32_MAX - 100U);

    alarm_update(98U);
    check_freq("wraparound 199ms", 800U);

    alarm_update(99U);
    check_freq("wraparound 200ms", 0U);

    check_active("wraparound ended", false);


    /* 12. 재초기화 */
    alarm_start(DISPLAY_SHOCK);
    alarm_update(11000U);

    alarm_init();

    check_freq("reinit muted", 0U);
    check_active("reinit inactive", false);

    check_true("driver was called",
            s_driver_calls > 0U);


    /* 최종 결과 */
    printf("\nTest summary: %u PASS, %u FAIL\n",
        s_pass, s_fail);

    return (s_fail == 0U) ? 0 : 1;
}
