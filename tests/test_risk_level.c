
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "risk_level.h"

/* 테스트 결과 */
static unsigned s_pass;
static unsigned s_fail;


/* 예상 위험 단계와 실제 결과 비교 */
static void check(
    const char *name,
    RiskLevel actual,
    RiskLevel expected
)
{
    if (actual == expected) {

        printf(
            "[PASS] %-32s expected=%d actual=%d\n",
            name,
            (int)expected,
            (int)actual
        );

        s_pass++;

    } else {

        printf(
            "[FAIL] %-32s expected=%d actual=%d\n",
            name,
            (int)expected,
            (int)actual
        );

        s_fail++;
    }
}


/* 가상 SensorSample 생성 및 판정 */
static RiskLevel feed(
    uint32_t ms,
    uint16_t vibration,
    uint16_t sound,
    uint16_t light
)
{
    SensorSample sample = {
        ms,
        vibration,
        sound,
        light
    };

    return risk_level_update(&sample);
}


int main(void)
{
    uint32_t ms;
    RiskLevel level;

    printf("=== Risk Level Mock Test ===\n");

    /* 1. 초기화 및 정상 입력 */
    risk_level_init();

    check("initial normal",
          feed(0U, 100U, 100U, 1000U),
          RISK_NORMAL);

    check("NULL keeps normal",
          risk_level_update(NULL),
          RISK_NORMAL);


    /* 2. 순간 충격 무시 */
    check("single shock spike ignored",
        feed(10U, 2100U, 100U, 1000U),
        RISK_NORMAL);

    check("spike cleared",
        feed(20U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /* 3. 충격 주의 단계 */
    check("shock first sample",
        feed(30U, 2100U, 100U, 1000U),
        RISK_NORMAL);

    check("shock caution",
        feed(40U, 2100U, 100U, 1000U),
        RISK_CAUTION);


    /* 4. 강한 충격 위험 단계 */
    check("strong shock danger",
        feed(50U, 4001U, 100U, 1000U),
        RISK_DANGER);

    check("NULL retains danger",
        risk_level_update(NULL),
        RISK_DANGER);

    check("shock recovered",
        feed(60U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /* 5. 소음 주의 단계 */
    check("sound first sample",
        feed(70U, 100U, 1200U, 1000U),
        RISK_NORMAL);

    check("sound caution",
        feed(80U, 100U, 1200U, 1000U),
        RISK_CAUTION);

    check("sound recovered",
        feed(90U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /* 6. 조도 변화 위험 단계 */
    check("light first sample",
        feed(100U, 100U, 100U, 1700U),
        RISK_NORMAL);

    check("light caution",
        feed(110U, 100U, 100U, 1700U),
        RISK_CAUTION);

    check("strong light danger",
        feed(120U, 100U, 100U, 2200U),
        RISK_DANGER);

    check("light recovered",
        feed(130U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /* 7. 복합 이벤트 위험 판정 */
    check("multiple first sample",
        feed(140U, 2100U, 1200U, 1000U),
        RISK_NORMAL);

    check("multiple danger",
        feed(150U, 2100U, 1200U, 1000U),
        RISK_DANGER);

    check("multiple recovered",
        feed(160U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /*
     * 8. 이상 상태 지속시간 테스트
     *
     * 진동값 2100을 10ms마다 입력한다.
     * 1000ms 이상 유지 시 DANGER
     */
    check("hold first sample",
        feed(1000U, 2100U, 100U, 1000U),
        RISK_NORMAL);

    check("hold caution",
        feed(1010U, 2100U, 100U, 1000U),
        RISK_CAUTION);

    level = RISK_CAUTION;

    for (ms = 1020U; ms <= 1990U; ms += 10U) {
        level = feed(
            ms, 2100U, 100U, 1000U
        );
    }

    check("hold 990ms caution",
        level,
        RISK_CAUTION);

    check("hold 1000ms danger",
        feed(2000U, 2100U, 100U, 1000U),
        RISK_DANGER);

    check("hold recovered",
        feed(2010U, 100U, 100U, 1000U),
        RISK_NORMAL);


    /* 9. 센서 샘플 누락 테스트 */
    check("gap first sample",
        feed(2020U, 2100U, 100U, 1000U),
        RISK_NORMAL);

    check("gap breaks confirmation",
        feed(2100U, 2100U, 100U, 1000U),
        RISK_NORMAL);

    check("after gap new caution",
        feed(2110U, 2100U, 100U, 1000U),
        RISK_CAUTION);


    /* 10. Timer 오버플로 테스트 */
    risk_level_init();

    check("reinit normal",
        feed(UINT32_MAX - 20U,
            100U, 100U, 1000U),
        RISK_NORMAL);

    check("wrap first sample",
        feed(UINT32_MAX - 10U,
            2100U, 100U, 1000U),
        RISK_NORMAL);

    check("wrap caution",
        feed(UINT32_MAX,
            2100U, 100U, 1000U),
        RISK_CAUTION);

    level = RISK_CAUTION;

    for (ms = 9U; ms <= 979U; ms += 10U) {
        level = feed(
            ms, 2100U, 100U, 1000U
        );
    }

    check("wrap 990ms caution",
        level,
        RISK_CAUTION);

    check("wrap 1000ms danger",
        feed(989U, 2100U, 100U, 1000U),
        RISK_DANGER);


    /* 테스트 결과 출력 */
    printf(
        "\nTest summary: %u PASS, %u FAIL\n",
        s_pass,
        s_fail
    );

    return (s_fail == 0U) ? 0 : 1;
}
