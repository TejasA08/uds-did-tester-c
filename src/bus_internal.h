#ifndef BUS_INTERNAL_H
#define BUS_INTERNAL_H

#include "uds_tester.h"

struct uds_bus {
    setup_config_t setup;
    int is_mock;
    /* mock ECU state */
    uint8_t session;
    uint8_t mem_f190[17];
    size_t mem_f190_len;
    uint8_t mem_f1a0[64];
    size_t mem_f1a0_len;
#ifdef UDS_HAS_PCAN
    void *pcan_handle; /* opaque */
#endif
};

int isotp_transceive(uds_bus_t *bus, const uint8_t *req, size_t req_len,
                     uint8_t *resp, size_t resp_cap, size_t *resp_len,
                     uint32_t timeout_ms);

int pcan_open(uds_bus_t *bus);
void pcan_close(uds_bus_t *bus);
int pcan_write_frame(uds_bus_t *bus, uint32_t can_id, const uint8_t *data, uint8_t len);
int pcan_read_frame(uds_bus_t *bus, uint32_t *can_id, uint8_t *data, uint8_t *len,
                    uint32_t timeout_ms);

#endif
