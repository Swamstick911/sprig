// Screen-mirror hook. Drop-in replacement for write_pixel: draws to the ST7735
// as normal AND streams the frame to the SprigScope web app over USB. Streams
// in small chunks (no full 40 KB framebuffer) so it fits in the Sprig's RAM
// alongside the engine.

#ifndef SCREEN_MIRROR_H
#define SCREEN_MIRROR_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "HAL.h"   // Color, write_pixel

#define MIRROR_PIXELS (160 * 128)
#define MIRROR_CHUNK  256   // bytes per USB write (small footprint)

static uint8_t  mirror_chunk[MIRROR_CHUNK];
static uint32_t mirror_chunk_len = 0;
static uint32_t mirror_idx = 0;
static bool     mirror_active = false;

// Push a buffer out the CDC; give up after a short timeout so we never hang.
static bool mirror_write_all(const uint8_t *data, uint32_t len) {
    uint32_t sent = 0;
    absolute_time_t deadline = make_timeout_time_ms(40);
    while (sent < len) {
        if (!tud_cdc_connected()) return false;
        uint32_t avail = tud_cdc_write_available();
        if (avail > 0) {
            uint32_t n = len - sent;
            if (n > avail) n = avail;
            sent += tud_cdc_write(data + sent, n);
            tud_cdc_write_flush();
            deadline = make_timeout_time_ms(40);
        } else {
            if (time_reached(deadline)) return false;
            sleep_us(200);
        }
    }
    return true;
}

static void mirror_flush_chunk(void) {
    if (mirror_chunk_len && mirror_active) {
        if (!mirror_write_all(mirror_chunk, mirror_chunk_len)) mirror_active = false;
    }
    mirror_chunk_len = 0;
}

// Drop-in for write_pixel: draw, then stream this frame if a host is connected.
static void mirror_write_pixel(Color color) {
    write_pixel(color);

    if (mirror_idx == 0) {              // start of a frame
        mirror_chunk_len = 0;
        mirror_active = tud_cdc_connected();
        if (mirror_active) {
            static const uint8_t magic[4] = {0xA5, 0x5A, 0xC3, 0x3C};
            if (!mirror_write_all(magic, 4)) mirror_active = false;
        }
    }

    if (mirror_active) {
        mirror_chunk[mirror_chunk_len++] = (uint8_t)(color >> 8);   // big-endian
        mirror_chunk[mirror_chunk_len++] = (uint8_t)(color & 0xFF);
        if (mirror_chunk_len >= MIRROR_CHUNK) mirror_flush_chunk();
    }

    if (++mirror_idx >= MIRROR_PIXELS) {   // end of a frame
        mirror_flush_chunk();
        mirror_idx = 0;
    }
}

#endif // SCREEN_MIRROR_H
