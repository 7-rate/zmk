/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <errno.h>

#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/split/transport/types.h>

#if !DT_HAS_CHOSEN(zmk_indicator)
#error "A zmk,indicator chosen node must be declared"
#endif

#define STRIP_CHOSEN DT_CHOSEN(zmk_indicator)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_CHOSEN, chain_length)

#define RGB_INDICATOR_BRIGHTNESS 64
#define RGB_INDICATOR_PROFILE_COUNT 3
#define RGB_INDICATOR_ANIMATION_PERIOD_MS 500
#define RGB_INDICATOR_ANIMATION_CYCLE_MS (RGB_INDICATOR_ANIMATION_PERIOD_MS * 2U)
#define RGB_INDICATOR_REFRESH_PERIOD_MS 100

LOG_MODULE_DECLARE(zmk, 4);

struct rgb_indicator_state {
    uint8_t mode;
    uint8_t active_profile_index;
    bool connected;
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    bool split_connected;
    bool has_remote_state;
#endif
};

static const struct device *led_strip;
static struct led_rgb pixels[STRIP_NUM_PIXELS];

static struct rgb_indicator_state state = {
    .mode = ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_DISCONNECTED,
    .active_profile_index = 0,
    .connected = false,
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    .split_connected = false,
    .has_remote_state = false,
#endif
};

static bool initialized;
static uint8_t animation_blue_brightness;
static bool ready_warning_logged;

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/split/central.h>

static bool refresh_state_requested;
static bool force_sync_requested;

static bool has_synced_state;
static uint8_t synced_mode;
static uint8_t synced_active_profile_index;
static bool synced_connected;

#else

#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>

#endif

static uint8_t normalize_profile_index(uint8_t profile_index) {
    return profile_index % RGB_INDICATOR_PROFILE_COUNT;
}

static struct led_rgb color_blue_level(uint8_t brightness) {
    return (struct led_rgb){.r = 0, .g = 0, .b = brightness};
}

static struct led_rgb color_blue(void) { return color_blue_level(RGB_INDICATOR_BRIGHTNESS); }

static struct led_rgb color_green(void) {
    return (struct led_rgb){.r = 0, .g = RGB_INDICATOR_BRIGHTNESS, .b = 0};
}

static uint8_t animation_sine_wave_0_to_255(uint32_t elapsed_ms) {
    static const uint8_t sine_lut[] = {
        128, 152, 176, 198, 218, 234, 245, 253, 255, 253, 245, 234, 218, 198, 176, 152,
        128, 103, 79,  57,  37,  21,  10,  2,   0,   2,   10,  21,  37,  57,  79,  103,
    };

    const uint32_t lut_size = ARRAY_SIZE(sine_lut);
    const uint32_t cycle_ms = RGB_INDICATOR_ANIMATION_CYCLE_MS;
    const uint32_t phase_ms = elapsed_ms % cycle_ms;
    const uint32_t index = (phase_ms * lut_size) / cycle_ms;

    return sine_lut[index % lut_size];
}

static void fill_pixels(struct led_rgb color) {
    for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = color;
    }
}

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void show_profile_pixel(uint8_t profile_index, struct led_rgb color) {
    fill_pixels(color_off());

    const size_t profile_led_count =
        MIN((size_t)RGB_INDICATOR_PROFILE_COUNT, (size_t)STRIP_NUM_PIXELS);
    if (profile_led_count == 0U) {
        return;
    }

    pixels[profile_index % profile_led_count] = color;
}
#endif

static uint8_t effective_mode(void) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return state.mode;
#else
    if (!state.split_connected || !state.has_remote_state || !state.connected) {
        return ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_DISCONNECTED;
    }

    return state.mode;
#endif
}

static bool render_pixels(void) {
    if (!device_is_ready(led_strip)) {
        if (!ready_warning_logged) {
            LOG_WRN("RGB indicator strip device is not ready yet; retrying");
            ready_warning_logged = true;
        }
        return false;
    }

    ready_warning_logged = false;

    const uint8_t mode = effective_mode();
    LOG_DBG("Rendering RGB indicator (mode: %d, active profile index: %d, connected: %d)", mode,
            state.active_profile_index, state.connected);

    if (mode == ZMK_SPLIT_RGB_INDICATOR_MODE_USB) {
        fill_pixels(color_green());
    } else if (mode == ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_CONNECTED) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        fill_pixels(color_blue());
#else
        show_profile_pixel(normalize_profile_index(state.active_profile_index), color_blue());
#endif
    } else {
        fill_pixels(color_blue_level(animation_blue_brightness));
    }

    int err = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
    if (err < 0) {
        LOG_ERR("Failed to update RGB indicator strip (%d)", err);
        return false;
    }

    return true;
}

static void update_animation_state(struct k_work *work);

K_WORK_DEFINE(apply_rgb_indicator_work, update_animation_state);

static void refresh_tick_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);

    k_work_submit(&apply_rgb_indicator_work);
}

K_TIMER_DEFINE(rgb_indicator_refresh_timer, refresh_tick_handler, NULL);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static void refresh_state_from_system(void) {
    const struct zmk_endpoint_instance selected_endpoint = zmk_endpoint_get_selected();
    LOG_DBG("Refreshing RGB indicator state from system");

#if IS_ENABLED(CONFIG_ZMK_BLE)
    int profile_index = zmk_ble_active_profile_index();
    if (profile_index < 0) {
        profile_index = 0;
    }
    state.active_profile_index = normalize_profile_index((uint8_t)profile_index);
#else
    state.active_profile_index = 0;
#endif

    if (selected_endpoint.transport == ZMK_TRANSPORT_USB) {
        state.mode = ZMK_SPLIT_RGB_INDICATOR_MODE_USB;
        state.connected = true;
        return;
    }

#if IS_ENABLED(CONFIG_ZMK_BLE)
    if (selected_endpoint.transport == ZMK_TRANSPORT_BLE && zmk_ble_active_profile_is_connected()) {
        state.mode = ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_CONNECTED;
        state.connected = true;
        return;
    }
#endif

    state.mode = ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_DISCONNECTED;
    state.connected = false;

    LOG_DBG("Refreshed RGB indicator state from system (mode: %d, active profile index: %d, "
            "connected: %d)",
            state.mode, state.active_profile_index, state.connected);
}

static bool should_sync_state(bool force_sync) {
    if (force_sync || !has_synced_state) {
        return true;
    }

    return synced_mode != state.mode || synced_active_profile_index != state.active_profile_index ||
           synced_connected != state.connected;
}

static void sync_to_peripherals(bool force_sync) {
    if (!should_sync_state(force_sync)) {
        return;
    }

    int err = zmk_split_central_update_rgb_indicator(state.mode, state.active_profile_index,
                                                     state.connected);
    if (err < 0) {
        if (err != -ENODEV) {
            LOG_WRN("Failed to sync RGB indicator state to peripherals (%d)", err);
        }
        return;
    }

    has_synced_state = true;
    synced_mode = state.mode;
    synced_active_profile_index = state.active_profile_index;
    synced_connected = state.connected;
}

void zmk_split_central_transport_status_changed_hook(struct zmk_split_transport_status status) {
    if (!initialized) {
        return;
    }

    if (status.connections == ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
        return;
    }

    force_sync_requested = true;
    k_work_submit(&apply_rgb_indicator_work);
}

#else

int zmk_split_peripheral_handle_rgb_indicator_sync(uint8_t mode, uint8_t active_profile_index,
                                                   bool connected) {
    if (mode > ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_DISCONNECTED) {
        return -EINVAL;
    }

    state.mode = mode;
    state.active_profile_index = normalize_profile_index(active_profile_index);
    state.connected = connected;
    state.has_remote_state = true;

    k_work_submit(&apply_rgb_indicator_work);

    return 0;
}

#endif

static void update_animation_state(struct k_work *work) {
    ARG_UNUSED(work);
    LOG_DBG("Updating RGB indicator state");

    if (!initialized) {
        return;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const bool should_refresh = refresh_state_requested;
    const bool force_sync = force_sync_requested;

    refresh_state_requested = false;
    force_sync_requested = false;

    if (should_refresh) {
        refresh_state_from_system();
    }
#endif

    const uint8_t mode = effective_mode();

    if (mode == ZMK_SPLIT_RGB_INDICATOR_MODE_BLE_DISCONNECTED) {
        const uint8_t wave = animation_sine_wave_0_to_255((uint32_t)k_uptime_get());
        animation_blue_brightness =
            (uint8_t)(((uint16_t)RGB_INDICATOR_BRIGHTNESS * (uint16_t)wave) / 255U);
    } else {
        animation_blue_brightness = 0U;
    }

    bool rendered = render_pixels();
    if (!rendered) {
        LOG_WRN("Failed to render RGB indicator state; will retry on next refresh");
        return;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    sync_to_peripherals(force_sync);
#endif
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static int rgb_indicator_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    refresh_state_requested = true;
    k_work_submit(&apply_rgb_indicator_work);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_indicator, rgb_indicator_listener);
ZMK_SUBSCRIPTION(rgb_indicator, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(rgb_indicator, zmk_ble_active_profile_changed);
#endif

#else

static int rgb_indicator_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *status_changed =
        as_zmk_split_peripheral_status_changed(eh);

    if (status_changed == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    state.split_connected = status_changed->connected;
    k_work_submit(&apply_rgb_indicator_work);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_indicator, rgb_indicator_listener);
ZMK_SUBSCRIPTION(rgb_indicator, zmk_split_peripheral_status_changed);

#endif

static int rgb_indicator_init(void) {
    led_strip = DEVICE_DT_GET(STRIP_CHOSEN);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    refresh_state_from_system();
    has_synced_state = false;
#else
    state.split_connected = zmk_split_bt_peripheral_is_connected();
    state.has_remote_state = false;
#endif

    initialized = true;
    k_timer_start(&rgb_indicator_refresh_timer, K_NO_WAIT, K_MSEC(RGB_INDICATOR_REFRESH_PERIOD_MS));
    k_work_submit(&apply_rgb_indicator_work);

    return 0;
}

SYS_INIT(rgb_indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
