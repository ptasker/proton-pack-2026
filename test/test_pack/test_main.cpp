#include <unity.h>
#include "PackController.h"
#include "Cyclotron.h"

void setUp() {}
void tearDown() {}

void expectState(const PackController &pack, PackState expected)
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(pack.state()));
}

void test_boot_modes()
{
    PackController pack;
    pack.begin(10, false);
    expectState(pack, PackState::Off);
    TEST_ASSERT_FALSE(pack.isOn());

    pack.begin(20, true);
    expectState(pack, PackState::Idle);
    TEST_ASSERT_TRUE(pack.isOn());
}

void test_startup_finishes_without_audio()
{
    PackController pack;
    pack.begin(0, false);
    pack.setPower(true, 100);
    expectState(pack, PackState::Starting);
    pack.setPower(true, 101);
    pack.update(100 + config::STARTUP_MS - 1);
    expectState(pack, PackState::Starting);
    pack.update(100 + config::STARTUP_MS);
    expectState(pack, PackState::Idle);
}

void test_shutdown_from_firing()
{
    PackController pack;
    pack.begin(0, true);
    pack.setFiring(true, 10);
    expectState(pack, PackState::Firing);
    pack.setPower(false, 20);
    expectState(pack, PackState::Stopping);
    TEST_ASSERT_FALSE(pack.isOn());
    pack.setPower(false, 21);
    pack.update(20 + config::SHUTDOWN_MS - 1);
    expectState(pack, PackState::Stopping);
    pack.update(20 + config::SHUTDOWN_MS);
    expectState(pack, PackState::Off);
}

void test_fire_requires_idle_and_a_new_press()
{
    PackController pack;
    pack.begin(0, false);
    pack.setFiring(true, 1);
    expectState(pack, PackState::Off);
    pack.setPower(true, 10);
    pack.update(10 + config::STARTUP_MS);
    pack.setFiring(true, 20 + config::STARTUP_MS);
    expectState(pack, PackState::Idle);
    pack.setFiring(false, 21 + config::STARTUP_MS);
    pack.setFiring(true, 22 + config::STARTUP_MS);
    expectState(pack, PackState::Firing);
    pack.setFiring(false, 23 + config::STARTUP_MS);
    expectState(pack, PackState::Idle);
}

void test_firing_timeout_and_held_button_interlock()
{
    PackController pack;
    pack.begin(0, true);
    pack.setFiring(true, 10);
    pack.setFiring(true, 11);
    pack.update(10 + config::MAX_FIRE_MS - 1);
    expectState(pack, PackState::Firing);
    const uint32_t coolingStarted = 10 + config::MAX_FIRE_MS;
    pack.update(coolingStarted);
    expectState(pack, PackState::Cooling);
    pack.update(coolingStarted + config::COOLDOWN_MS - 1);
    expectState(pack, PackState::Cooling);
    const uint32_t cooled = coolingStarted + config::COOLDOWN_MS;
    pack.update(cooled);
    pack.setFiring(true, cooled + 1);
    expectState(pack, PackState::Idle);
    pack.setFiring(false, cooled + 2);
    pack.setFiring(true, cooled + 3);
    expectState(pack, PackState::Firing);
}

void test_vent_is_lights_only_and_cannot_bypass_cooldown()
{
    PackController pack;
    pack.begin(0, false);
    pack.vent(1);
    expectState(pack, PackState::Off);
    pack.begin(10, true);
    pack.vent(20);
    expectState(pack, PackState::Cooling);
    pack.setFiring(true, 21);
    pack.setFiring(false, 22);
    pack.vent(23);
    expectState(pack, PackState::Cooling);
    pack.update(20 + config::COOLDOWN_MS);
    expectState(pack, PackState::Idle);
}

void test_power_off_interrupts_startup_and_cooling()
{
    PackController pack;
    pack.begin(0, false);
    pack.togglePower(10);
    pack.togglePower(20);
    expectState(pack, PackState::Stopping);
    pack.update(20 + config::SHUTDOWN_MS);
    expectState(pack, PackState::Off);

    pack.begin(2000, true);
    pack.vent(2010);
    pack.setPower(false, 2020);
    expectState(pack, PackState::Stopping);
}

void test_power_on_can_interrupt_shutdown()
{
    PackController pack;
    pack.begin(0, true);
    pack.togglePower(10);
    pack.togglePower(20);
    expectState(pack, PackState::Starting);
    pack.update(20 + config::STARTUP_MS);
    expectState(pack, PackState::Idle);
}

void test_state_timing_survives_millis_rollover()
{
    PackController pack;
    const uint32_t started = UINT32_MAX - 100;
    pack.begin(started, false);
    pack.setPower(true, started);
    pack.update(started + config::STARTUP_MS - 1);
    expectState(pack, PackState::Starting);
    pack.update(started + config::STARTUP_MS);
    expectState(pack, PackState::Idle);

    pack.begin(started, true);
    pack.setFiring(true, started);
    pack.update(started + config::MAX_FIRE_MS);
    expectState(pack, PackState::Cooling);
}

void test_cyclotron_waits_for_the_next_step()
{
    Cyclotron cyclotron;
    cyclotron.begin(100);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(0));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(1));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(config::RING_PIXELS - 1));
    cyclotron.update(194, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(0));
    cyclotron.update(195, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(1));
    TEST_ASSERT_EQUAL_UINT8(127, cyclotron.redAt(0));
}

void test_cyclotron_trail_halves_each_step()
{
    Cyclotron cyclotron;
    cyclotron.begin(0);
    cyclotron.update(8 * 95, 95);
    for (uint16_t distance = 0; distance < 8; ++distance)
        TEST_ASSERT_EQUAL_UINT8(255U >> distance, cyclotron.redAt(8 - distance));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(0));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(9));
}

void test_cyclotron_visits_all_pixels_and_wraps()
{
    Cyclotron cyclotron;
    cyclotron.begin(0);
    for (uint16_t head = 1; head < config::RING_PIXELS; ++head)
    {
        cyclotron.update(head * 95, 95);
        TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(head));
    }
    cyclotron.update(config::RING_PIXELS * 95, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(0));
    TEST_ASSERT_EQUAL_UINT8(127, cyclotron.redAt(config::RING_PIXELS - 1));
    TEST_ASSERT_EQUAL_UINT8(63, cyclotron.redAt(config::RING_PIXELS - 2));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(1));
}

void test_cyclotron_catches_up_without_timing_drift()
{
    Cyclotron cyclotron;
    cyclotron.begin(0);
    cyclotron.update(3 * 95 + 20, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(3));
    TEST_ASSERT_EQUAL_UINT8(31, cyclotron.redAt(0));
    cyclotron.update(4 * 95 - 1, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(3));
    cyclotron.update(4 * 95, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(4));
}

void test_cyclotron_timing_survives_millis_rollover()
{
    Cyclotron cyclotron;
    const uint32_t started = UINT32_MAX - 40;
    cyclotron.begin(started);
    cyclotron.update(started + 94, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(0));
    cyclotron.update(started + 95, 95);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(1));
    TEST_ASSERT_EQUAL_UINT8(127, cyclotron.redAt(0));
}

void test_cyclotron_restart_clears_the_old_trail()
{
    Cyclotron cyclotron;
    cyclotron.begin(0);
    cyclotron.update(config::RING_PIXELS * 95, 95);
    cyclotron.begin(10000);
    TEST_ASSERT_EQUAL_UINT8(255, cyclotron.redAt(0));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(config::RING_PIXELS - 1));
}

void test_afterlife_has_a_shaded_warm_red_head()
{
    AfterlifeCyclotron cyclotron;
    cyclotron.begin(0);
    TEST_ASSERT_EQUAL_UINT8(20, cyclotron.redAt(57));
    TEST_ASSERT_EQUAL_UINT8(16, cyclotron.redAt(58));
    TEST_ASSERT_EQUAL_UINT8(12, cyclotron.redAt(59));
    TEST_ASSERT_EQUAL_UINT8(8, cyclotron.redAt(0));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(1));

    cyclotron.update(3000, 20);
    TEST_ASSERT_EQUAL_UINT8(51, cyclotron.redAt(12));
    TEST_ASSERT_EQUAL_UINT8(40, cyclotron.redAt(13));
    TEST_ASSERT_EQUAL_UINT8(30, cyclotron.redAt(14));
    TEST_ASSERT_EQUAL_UINT8(20, cyclotron.redAt(15));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(16));
    TEST_ASSERT_EQUAL_UINT8(2, cyclotron.greenAt(12));
    TEST_ASSERT_EQUAL_UINT8(1, cyclotron.greenAt(13));
}

void test_afterlife_lights_only_four_window_groups()
{
    AfterlifeCyclotron cyclotron;
    cyclotron.begin(0);
    bool visited[config::RING_PIXELS] = {};
    const uint16_t visiblePixels[] = {
        57, 58, 59, 0, 1, 2,
        12, 13, 14, 15, 16, 17,
        27, 28, 29, 30, 31, 32,
        42, 43, 44, 45, 46, 47};

    for (uint32_t now = 0; now <= 6000; now += 10)
    {
        cyclotron.update(now, 20);
        for (uint16_t pixel = 0; pixel < config::RING_PIXELS; ++pixel)
            visited[pixel] = visited[pixel] || cyclotron.redAt(pixel) > 0;
    }

    for (uint16_t pixel = 0; pixel < config::RING_PIXELS; ++pixel)
    {
        bool expected = false;
        for (uint16_t visible : visiblePixels)
            expected = expected || visible == pixel;
        TEST_ASSERT_EQUAL(expected, visited[pixel]);
    }
}

void test_afterlife_ramps_speed_and_accepts_new_targets()
{
    AfterlifeCyclotron cyclotron;
    cyclotron.begin(0);
    TEST_ASSERT_EQUAL_UINT32(60, cyclotron.stepIntervalMs());
    cyclotron.update(1500, 20);
    TEST_ASSERT_EQUAL_UINT32(40, cyclotron.stepIntervalMs());
    cyclotron.update(3000, 20);
    TEST_ASSERT_EQUAL_UINT32(20, cyclotron.stepIntervalMs());
    cyclotron.update(4000, 15);
    TEST_ASSERT_EQUAL_UINT32(20, cyclotron.stepIntervalMs());
    cyclotron.update(7000, 15);
    TEST_ASSERT_EQUAL_UINT32(15, cyclotron.stepIntervalMs());
    cyclotron.update(8000, 60);
    cyclotron.update(11000, 60);
    TEST_ASSERT_EQUAL_UINT32(60, cyclotron.stepIntervalMs());
}

void test_afterlife_wraps_through_visible_pixels_without_drift()
{
    AfterlifeCyclotron cyclotron;
    cyclotron.begin(0);
    cyclotron.update(3000, 20);
    cyclotron.update(3360, 20);
    TEST_ASSERT_EQUAL_UINT8(51, cyclotron.redAt(57));
    cyclotron.update(3379, 20);
    TEST_ASSERT_EQUAL_UINT8(51, cyclotron.redAt(57));
    cyclotron.update(3380, 20);
    TEST_ASSERT_EQUAL_UINT8(51, cyclotron.redAt(58));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(57));
}

void test_afterlife_handles_millis_rollover()
{
    AfterlifeCyclotron normal;
    AfterlifeCyclotron wrapped;
    const uint32_t started = UINT32_MAX - 100;
    normal.begin(0);
    wrapped.begin(started);
    for (uint32_t elapsed = 0; elapsed <= 4000; elapsed += 10)
    {
        normal.update(elapsed, 20);
        wrapped.update(started + elapsed, 20);
        TEST_ASSERT_EQUAL_UINT32(normal.stepIntervalMs(), wrapped.stepIntervalMs());
        for (uint16_t pixel = 0; pixel < config::RING_PIXELS; ++pixel)
            TEST_ASSERT_EQUAL_UINT8(normal.redAt(pixel), wrapped.redAt(pixel));
    }
}

void test_afterlife_restart_resets_position_and_speed()
{
    AfterlifeCyclotron cyclotron;
    cyclotron.begin(0);
    cyclotron.update(3000, 20);
    cyclotron.begin(5000);
    TEST_ASSERT_EQUAL_UINT32(60, cyclotron.stepIntervalMs());
    TEST_ASSERT_EQUAL_UINT8(20, cyclotron.redAt(57));
    TEST_ASSERT_EQUAL_UINT8(0, cyclotron.redAt(12));
}

void test_cyclotron_mode_selection_preserves_pack_state()
{
    PackController pack;
    pack.begin(0, false);
    TEST_ASSERT_TRUE(pack.cyclotronMode() == config::CYCLOTRON_MODE);
    pack.setCyclotronMode(CyclotronMode::Classic1984);
    TEST_ASSERT_EQUAL_STRING("1984", pack.cyclotronModeName());
    expectState(pack, PackState::Off);

    pack.begin(100, true);
    pack.setFiring(true, 110);
    pack.setCyclotronMode(CyclotronMode::Classic1984);
    pack.setCyclotronMode(CyclotronMode::Afterlife);
    TEST_ASSERT_EQUAL_STRING("Afterlife", pack.cyclotronModeName());
    expectState(pack, PackState::Firing);
    TEST_ASSERT_EQUAL_UINT32(10, pack.stateAge(120));
    pack.update(110 + config::MAX_FIRE_MS);
    expectState(pack, PackState::Cooling);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_modes);
    RUN_TEST(test_startup_finishes_without_audio);
    RUN_TEST(test_shutdown_from_firing);
    RUN_TEST(test_fire_requires_idle_and_a_new_press);
    RUN_TEST(test_firing_timeout_and_held_button_interlock);
    RUN_TEST(test_vent_is_lights_only_and_cannot_bypass_cooldown);
    RUN_TEST(test_power_off_interrupts_startup_and_cooling);
    RUN_TEST(test_power_on_can_interrupt_shutdown);
    RUN_TEST(test_state_timing_survives_millis_rollover);
    RUN_TEST(test_cyclotron_waits_for_the_next_step);
    RUN_TEST(test_cyclotron_trail_halves_each_step);
    RUN_TEST(test_cyclotron_visits_all_pixels_and_wraps);
    RUN_TEST(test_cyclotron_catches_up_without_timing_drift);
    RUN_TEST(test_cyclotron_timing_survives_millis_rollover);
    RUN_TEST(test_cyclotron_restart_clears_the_old_trail);
    RUN_TEST(test_afterlife_has_a_shaded_warm_red_head);
    RUN_TEST(test_afterlife_lights_only_four_window_groups);
    RUN_TEST(test_afterlife_ramps_speed_and_accepts_new_targets);
    RUN_TEST(test_afterlife_wraps_through_visible_pixels_without_drift);
    RUN_TEST(test_afterlife_handles_millis_rollover);
    RUN_TEST(test_afterlife_restart_resets_position_and_speed);
    RUN_TEST(test_cyclotron_mode_selection_preserves_pack_state);
    return UNITY_END();
}
