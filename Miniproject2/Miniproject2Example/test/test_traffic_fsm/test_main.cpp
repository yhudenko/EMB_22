#include <unity.h>

#include "traffic_fsm.h"

static TrafficFsm fsm;

void setUp() { fsm.reset(); }
void tearDown() {}

void test_starts_in_green() {
    TEST_ASSERT_EQUAL_STRING("GREEN", fsm.phaseName());
    TEST_ASSERT_EQUAL_UINT8(phases::kMaskGreen, fsm.lampMask());
    // GREEN + GREEN_BLINK share one countdown (9s + 3s).
    TEST_ASSERT_EQUAL_UINT16(12, fsm.remainingSeconds());
}

void test_full_order_and_durations() {
    const char* expected[] = {"GREEN_BLINK", "YELLOW", "RED", "RED_YELLOW", "GREEN"};
    const uint32_t durations[] = {
        phases::kPhases[0].durationMs,  // leave GREEN
        phases::kPhases[1].durationMs,  // leave GREEN_BLINK
        phases::kPhases[2].durationMs,  // leave YELLOW
        phases::kPhases[3].durationMs,  // leave RED
        phases::kPhases[4].durationMs,  // leave RED_YELLOW
    };
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(fsm.advance(durations[i]));
        TEST_ASSERT_EQUAL_STRING(expected[i], fsm.phaseName());
    }
}

void test_shared_green_countdown() {
    TEST_ASSERT_EQUAL_UINT16(12, fsm.remainingSeconds());
    fsm.advance(9000);  // into GREEN_BLINK
    TEST_ASSERT_EQUAL_STRING("GREEN_BLINK", fsm.phaseName());
    TEST_ASSERT_EQUAL_UINT16(3, fsm.remainingSeconds());  // continues 3..1
}

void test_blink_toggles() {
    fsm.advance(9000);  // start of GREEN_BLINK, lamp on
    TEST_ASSERT_EQUAL_UINT8(phases::kMaskGreen, fsm.lampMask());
    fsm.advance(500);  // first off window
    TEST_ASSERT_EQUAL_UINT8(phases::kMaskNone, fsm.lampMask());
    fsm.advance(500);  // on again
    TEST_ASSERT_EQUAL_UINT8(phases::kMaskGreen, fsm.lampMask());
}

void test_red_yellow_mask() {
    fsm.advance(9000 + 3000 + 3000 + 9000);  // into RED_YELLOW
    TEST_ASSERT_EQUAL_STRING("RED_YELLOW", fsm.phaseName());
    TEST_ASSERT_EQUAL_UINT8(phases::kMaskRed | phases::kMaskYellow, fsm.lampMask());
}

void test_cycle_wraps() {
    const uint32_t cycle = 9000 + 3000 + 3000 + 9000 + 2000;
    fsm.advance(cycle);
    TEST_ASSERT_EQUAL_STRING("GREEN", fsm.phaseName());
    TEST_ASSERT_EQUAL_UINT16(12, fsm.remainingSeconds());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_green);
    RUN_TEST(test_full_order_and_durations);
    RUN_TEST(test_shared_green_countdown);
    RUN_TEST(test_blink_toggles);
    RUN_TEST(test_red_yellow_mask);
    RUN_TEST(test_cycle_wraps);
    return UNITY_END();
}
