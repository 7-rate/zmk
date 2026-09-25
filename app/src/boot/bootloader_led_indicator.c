/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include <zmk/bootloader_led_indicator.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_CHOSEN(zmk_indicator)

#define BOOTLOADER_LED_STRIP_NODE DT_CHOSEN(zmk_indicator)
#define BOOTLOADER_LED_STRIP_LEN DT_PROP(BOOTLOADER_LED_STRIP_NODE, chain_length)
#define BOOTLOADER_LED_WHITE_LEVEL 64

#if defined(CONFIG_SOC_NRF52840)
#define NRF52840_SPIM3_ERRATA_195_REG ((volatile uint32_t *)0x4002F004)
#endif

int zmk_bootloader_led_set_white(void) {
    static const struct device *const led_strip = DEVICE_DT_GET(BOOTLOADER_LED_STRIP_NODE);
    static struct led_rgb pixels[BOOTLOADER_LED_STRIP_LEN];
    const struct led_rgb white = {
        .r = BOOTLOADER_LED_WHITE_LEVEL,
        .g = BOOTLOADER_LED_WHITE_LEVEL,
        .b = BOOTLOADER_LED_WHITE_LEVEL,
    };

    if (!device_is_ready(led_strip)) {
        LOG_WRN("Bootloader LED strip device is not ready");
        return -ENODEV;
    }

    for (size_t i = 0; i < BOOTLOADER_LED_STRIP_LEN; i++) {
        pixels[i] = white;
    }

    int err = led_strip_update_rgb(led_strip, pixels, BOOTLOADER_LED_STRIP_LEN);
    if (err < 0) {
        LOG_ERR("Failed to update bootloader LED strip (%d)", err);
        return err;
    }

#if defined(CONFIG_SOC_NRF52840)
    /* nRF52840 anomaly 195 workaround for SPIM3 after disable. */
    *NRF52840_SPIM3_ERRATA_195_REG = 1U;
#endif

    return 0;
}

#else

int zmk_bootloader_led_set_white(void) { return -ENODEV; }

#endif