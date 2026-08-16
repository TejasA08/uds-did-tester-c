#include "bus_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * PCAN-Basic backend (Windows).
 * Build with -DUDS_HAS_PCAN and link PCANBasic.lib / PCANBasic.dll from PEAK.
 * Header expected as PCANBasic.h on the include path.
 */

#ifndef UDS_HAS_PCAN

int pcan_open(uds_bus_t *bus) {
    (void)bus;
    return -1;
}
void pcan_close(uds_bus_t *bus) { (void)bus; }
int pcan_write_frame(uds_bus_t *bus, uint32_t can_id, const uint8_t *data, uint8_t len) {
    (void)bus; (void)can_id; (void)data; (void)len;
    return -1;
}
int pcan_read_frame(uds_bus_t *bus, uint32_t *can_id, uint8_t *data, uint8_t *len,
                    uint32_t timeout_ms) {
    (void)bus; (void)can_id; (void)data; (void)len; (void)timeout_ms;
    return -1;
}

#else

#include "PCANBasic.h"

struct pcan_state {
    TPCANHandle channel;
};

static TPCANHandle parse_channel(const char *name) {
    if (!name) return PCAN_USBBUS1;
    if (!strcmp(name, "PCAN_USBBUS1") || !strcmp(name, "1")) return PCAN_USBBUS1;
    if (!strcmp(name, "PCAN_USBBUS2") || !strcmp(name, "2")) return PCAN_USBBUS2;
    if (!strcmp(name, "PCAN_USBBUS3") || !strcmp(name, "3")) return PCAN_USBBUS3;
    if (!strcmp(name, "PCAN_USBBUS4") || !strcmp(name, "4")) return PCAN_USBBUS4;
    if (!strcmp(name, "PCAN_USBBUS5") || !strcmp(name, "5")) return PCAN_USBBUS5;
    if (!strcmp(name, "PCAN_USBBUS6") || !strcmp(name, "6")) return PCAN_USBBUS6;
    if (!strcmp(name, "PCAN_USBBUS7") || !strcmp(name, "7")) return PCAN_USBBUS7;
    if (!strcmp(name, "PCAN_USBBUS8") || !strcmp(name, "8")) return PCAN_USBBUS8;
    return PCAN_USBBUS1;
}

static TPCANBaudrate map_bitrate(uint32_t bitrate) {
    switch (bitrate) {
        case 1000000: return PCAN_BAUD_1M;
        case 800000: return PCAN_BAUD_800K;
        case 500000: return PCAN_BAUD_500K;
        case 250000: return PCAN_BAUD_250K;
        case 125000: return PCAN_BAUD_125K;
        case 100000: return PCAN_BAUD_100K;
        case 95000: return PCAN_BAUD_95K;
        case 83000: return PCAN_BAUD_83K;
        case 50000: return PCAN_BAUD_50K;
        case 47000: return PCAN_BAUD_47K;
        case 33000: return PCAN_BAUD_33K;
        case 20000: return PCAN_BAUD_20K;
        case 10000: return PCAN_BAUD_10K;
        case 5000: return PCAN_BAUD_5K;
        default: return PCAN_BAUD_500K;
    }
}

int pcan_open(uds_bus_t *bus) {
    struct pcan_state *st = calloc(1, sizeof(*st));
    if (!st) return -1;
    st->channel = parse_channel(bus->setup.peak_channel);
    TPCANStatus status = CAN_Initialize(st->channel, map_bitrate(bus->setup.bitrate), 0, 0, 0);
    if (status != PCAN_ERROR_OK) {
        fprintf(stderr, "CAN_Initialize failed: 0x%X\n", (unsigned)status);
        free(st);
        return -1;
    }
    bus->pcan_handle = st;
    return 0;
}

void pcan_close(uds_bus_t *bus) {
    if (!bus || !bus->pcan_handle) return;
    struct pcan_state *st = bus->pcan_handle;
    CAN_Uninitialize(st->channel);
    free(st);
    bus->pcan_handle = NULL;
}

int pcan_write_frame(uds_bus_t *bus, uint32_t can_id, const uint8_t *data, uint8_t len) {
    struct pcan_state *st = bus->pcan_handle;
    if (!st) return -1;
    TPCANMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.ID = can_id;
    msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
    msg.LEN = len > 8 ? 8 : len;
    memcpy(msg.DATA, data, msg.LEN);
    TPCANStatus status = CAN_Write(st->channel, &msg);
    return status == PCAN_ERROR_OK ? 0 : -1;
}

int pcan_read_frame(uds_bus_t *bus, uint32_t *can_id, uint8_t *data, uint8_t *len,
                    uint32_t timeout_ms) {
    struct pcan_state *st = bus->pcan_handle;
    if (!st) return -1;
    (void)timeout_ms;
    TPCANMsg msg;
    TPCANTimestamp ts;
    TPCANStatus status = CAN_Read(st->channel, &msg, &ts);
    if (status == PCAN_ERROR_QRCVEMPTY) return 1;
    if (status != PCAN_ERROR_OK) return -1;
    *can_id = msg.ID;
    *len = msg.LEN;
    memcpy(data, msg.DATA, msg.LEN);
    return 0;
}

#endif /* UDS_HAS_PCAN */
