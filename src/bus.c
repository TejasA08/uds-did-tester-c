#include "bus_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int mock_request(uds_bus_t *bus, const uint8_t *req, size_t req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    if (req_len == 0) return -1;
    uint8_t sid = req[0];

    if (sid == 0x10 && req_len >= 2) {
        bus->session = req[1];
        if (resp_cap < 6) return -1;
        resp[0] = 0x50;
        resp[1] = req[1];
        resp[2] = 0x00;
        resp[3] = 0x32;
        resp[4] = 0x01;
        resp[5] = 0xF4;
        *resp_len = 6;
        return 0;
    }

    if (sid == 0x3E) {
        *resp_len = 0; /* suppress positive */
        return 0;
    }

    if (sid == 0x22 && req_len >= 3) {
        uint16_t did = (uint16_t)((req[1] << 8) | req[2]);
        const uint8_t *data = NULL;
        size_t dlen = 0;
        if (did == 0xF190) {
            data = bus->mem_f190;
            dlen = bus->mem_f190_len;
        } else if (did == 0xF1A0) {
            data = bus->mem_f1a0;
            dlen = bus->mem_f1a0_len;
        } else {
            if (resp_cap < 3) return -1;
            resp[0] = 0x7F;
            resp[1] = 0x22;
            resp[2] = 0x31;
            *resp_len = 3;
            return 0;
        }
        if (resp_cap < 3 + dlen) return -1;
        resp[0] = 0x62;
        resp[1] = req[1];
        resp[2] = req[2];
        memcpy(resp + 3, data, dlen);
        *resp_len = 3 + dlen;
        return 0;
    }

    if (sid == 0x2E && req_len >= 3) {
        uint16_t did = (uint16_t)((req[1] << 8) | req[2]);
        const uint8_t *payload = req + 3;
        size_t plen = req_len - 3;
        if (did == 0xF1A0) {
            if (plen > sizeof(bus->mem_f1a0)) plen = sizeof(bus->mem_f1a0);
            memcpy(bus->mem_f1a0, payload, plen);
            bus->mem_f1a0_len = plen;
        } else if (did == 0xF190) {
            if (plen > sizeof(bus->mem_f190)) plen = sizeof(bus->mem_f190);
            memcpy(bus->mem_f190, payload, plen);
            bus->mem_f190_len = plen;
        }
        if (resp_cap < 3) return -1;
        resp[0] = 0x6E;
        resp[1] = req[1];
        resp[2] = req[2];
        *resp_len = 3;
        return 0;
    }

    if (resp_cap < 3) return -1;
    resp[0] = 0x7F;
    resp[1] = sid;
    resp[2] = 0x11;
    *resp_len = 3;
    return 0;
}

uds_bus_t *bus_open(const setup_config_t *setup) {
    uds_bus_t *bus = calloc(1, sizeof(*bus));
    if (!bus) return NULL;
    bus->setup = *setup;
    bus->session = 0x01;
    memcpy(bus->mem_f190, "VIN123456789ABCD", 16);
    bus->mem_f190_len = 16;
    bus->mem_f1a0[0] = 0x00;
    bus->mem_f1a0[1] = 0x00;
    bus->mem_f1a0[2] = 0x00;
    bus->mem_f1a0_len = 3;

    if (setup->interface == UDS_IF_MOCK) {
        bus->is_mock = 1;
        return bus;
    }

#ifdef UDS_HAS_PCAN
    bus->is_mock = 0;
    if (pcan_open(bus) != 0) {
        free(bus);
        return NULL;
    }
    return bus;
#else
    fprintf(stderr,
            "PCAN support was not compiled in. Rebuild with UDS_HAS_PCAN=1 "
            "on Windows with PCAN-Basic, or set interface=mock.\n");
    free(bus);
    return NULL;
#endif
}

void bus_close(uds_bus_t *bus) {
    if (!bus) return;
#ifdef UDS_HAS_PCAN
    if (!bus->is_mock) pcan_close(bus);
#endif
    free(bus);
}

int bus_send_uds(uds_bus_t *bus, const uint8_t *req, size_t req_len,
                 uint8_t *resp, size_t resp_cap, size_t *resp_len, uint32_t timeout_ms) {
    if (!bus || !req || !resp || !resp_len) return -1;
    *resp_len = 0;
    if (bus->is_mock)
        return mock_request(bus, req, req_len, resp, resp_cap, resp_len);
    return isotp_transceive(bus, req, req_len, resp, resp_cap, resp_len, timeout_ms);
}
