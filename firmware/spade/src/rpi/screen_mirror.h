// Screen-mirror hook. A drop-in replacement for write_pixel that draws to the
// ST7735 as normal AND streams the frame to the SprigScope web app over USB.
// Every screen update goes through fill_start/render/fill_end, so swapping
// write_pixel -> mirror_write_pixel mirrors *everything* the Sprig shows.

#ifndef SCREEN_MIRROR_H
#define SCREEN_MIRROR_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "HAL.h"   // Color, write_pixel

#define MIRROR_PIXELS (160 * 128)

static uint8_t  mirror_tx[4 + MIRROR_PIXELS * 2];
static uint32_t mirror_idx = 0;

static void mirror_send_frame(void) {
    if (!tud_cdc_connected()) return;
    mirror_tx[0] = 0xA5; mirror_tx[1] = 0x5A; mirror_tx[2] = 0xC3; mirror_tx[3] = 0x3C;
    const uint32_t len = 4 + MIRROR_PIXELS * 2;
    uint32_t sent = 0;
    absolute_time_t deadline = make_timeout_time_ms(120);
    while (sent < len) {
        if (!tud_cdc_connected()) return;
        uint32_t avail = tud_cdc_write_available();
        if (avail > 0) {
            uint32_t n = len - sent;
            if (n > avail) n = avail;
            sent += tud_cdc_write(mirror_tx + sent, n);
            tud_cdc_write_flush();
            deadline = make_timeout_time_ms(120);
        } else {
            if (time_reached(deadline)) return;
            sleep_us(200);
        }
    }
    tud_cdc_write_flush();
}

static void mirror_write_pixel(Color color) {
    write_pixel(color);
    mirror_tx[4 + mirror_idx * 2]     = (uint8_t)(color >> 8);
    mirror_tx[4 + mirror_idx * 2 + 1] = (uint8_t)(color & 0xFF);
    if (++mirror_idx >= MIRROR_PIXELS) {
        mirror_send_frame();
        mirror_idx = 0;
    }
}

#endif // SCREEN_MIRROR_H
