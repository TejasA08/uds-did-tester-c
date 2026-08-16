#include "bus_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(unsigned ms) { Sleep(ms); }
static uint64_t now_ms(void) { return GetTickCount64(); }
#else
#include <sys/time.h>
#include <unistd.h>
static void sleep_ms(unsigned ms) { usleep(ms * 1000); }
static uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ull + (uint64_t)tv.tv_usec / 1000ull;
}
#endif

/* Classic CAN ISO-TP (ISO 15765-2), normal 11-bit addressing. */

static int wait_frame(uds_bus_t *bus, uint32_t expect_id, uint8_t *data, uint8_t *len,
                      uint32_t timeout_ms) {
    uint64_t deadline = now_ms() + timeout_ms;
    while (now_ms() <= deadline) {
        uint32_t id = 0;
        uint8_t n = 0;
        uint8_t buf[64];
        int rc = pcan_read_frame(bus, &id, buf, &n, 10);
        if (rc == 0 && id == expect_id && n > 0) {
            if (n > 8) n = 8;
            memcpy(data, buf, n);
            *len = n;
            return 0;
        }
        sleep_ms(1);
    }
    return -1;
}

int isotp_transceive(uds_bus_t *bus, const uint8_t *req, size_t req_len,
                     uint8_t *resp, size_t resp_cap, size_t *resp_len,
                     uint32_t timeout_ms) {
    if (!req_len || req_len > UDS_MAX_PAYLOAD) return -1;
    uint8_t frame[8];
    memset(frame, 0x00, sizeof(frame));

    /* ---- TX ---- */
    if (req_len <= 7) {
        frame[0] = (uint8_t)(0x00 | (req_len & 0x0F));
        memcpy(&frame[1], req, req_len);
        if (pcan_write_frame(bus, bus->setup.request_id, frame, 8) != 0) return -1;
    } else {
        frame[0] = (uint8_t)(0x10 | ((req_len >> 8) & 0x0F));
        frame[1] = (uint8_t)(req_len & 0xFF);
        memcpy(&frame[2], req, 6);
        if (pcan_write_frame(bus, bus->setup.request_id, frame, 8) != 0) return -1;

        uint8_t fc[8];
        uint8_t fclen = 0;
        if (wait_frame(bus, bus->setup.response_id, fc, &fclen, timeout_ms) != 0) return -2;
        if ((fc[0] & 0xF0) != 0x30) return -3;

        size_t offset = 6;
        uint8_t sn = 1;
        while (offset < req_len) {
            memset(frame, 0x00, sizeof(frame));
            frame[0] = (uint8_t)(0x20 | (sn & 0x0F));
            size_t chunk = req_len - offset;
            if (chunk > 7) chunk = 7;
            memcpy(&frame[1], req + offset, chunk);
            if (pcan_write_frame(bus, bus->setup.request_id, frame, 8) != 0) return -1;
            offset += chunk;
            sn = (uint8_t)((sn + 1) & 0x0F);
            sleep_ms(1);
        }
    }

    /* ---- RX ---- */
    uint8_t first[8];
    uint8_t first_len = 0;
    if (wait_frame(bus, bus->setup.response_id, first, &first_len, timeout_ms) != 0) return -4;

    uint8_t pci = first[0] & 0xF0;
    if (pci == 0x00) {
        size_t n = first[0] & 0x0F;
        if (n > 7 || n + 1 > first_len) return -5;
        if (n > resp_cap) return -6;
        memcpy(resp, &first[1], n);
        *resp_len = n;
        return 0;
    }

    if (pci == 0x10) {
        size_t total = ((size_t)(first[0] & 0x0F) << 8) | first[1];
        if (total > resp_cap || total < 1) return -6;
        size_t got = 0;
        size_t copy = total < 6 ? total : 6;
        memcpy(resp, &first[2], copy);
        got = copy;

        /* send FC CTS */
        uint8_t fc[8] = {0x30, 0x00, 0x00, 0, 0, 0, 0, 0};
        if (pcan_write_frame(bus, bus->setup.request_id, fc, 8) != 0) return -1;

        uint8_t expect_sn = 1;
        while (got < total) {
            uint8_t cf[8];
            uint8_t cflen = 0;
            if (wait_frame(bus, bus->setup.response_id, cf, &cflen, timeout_ms) != 0) return -7;
            if ((cf[0] & 0xF0) != 0x20) return -8;
            if ((cf[0] & 0x0F) != expect_sn) return -9;
            size_t chunk = total - got;
            if (chunk > 7) chunk = 7;
            memcpy(resp + got, &cf[1], chunk);
            got += chunk;
            expect_sn = (uint8_t)((expect_sn + 1) & 0x0F);
        }
        *resp_len = total;
        return 0;
    }

    return -10;
}
