#include "uds_tester.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
static double now_sec(void) {
    static LARGE_INTEGER freq;
    static int init = 0;
    LARGE_INTEGER c;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}
#else
#include <strings.h>
#include <sys/time.h>
static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}
#endif

static int session_sub(const char *label, uint8_t *out) {
    if (!label || !label[0] || !strcasecmp(label, "default")) {
        *out = 0x01;
        return 0;
    }
    if (!strcasecmp(label, "programming")) {
        *out = 0x02;
        return 0;
    }
    if (!strcasecmp(label, "extended") || !strcasecmp(label, "extendeddiagnostic") ||
        !strcasecmp(label, "extended diagnostic")) {
        *out = 0x03;
        return 0;
    }
    uint32_t v = 0;
    if (parse_u32_auto(label, &v) == 0) {
        *out = (uint8_t)v;
        return 0;
    }
    return -1;
}

static bool compare_payload(const uint8_t *actual, size_t alen,
                            const uint8_t *expected, size_t elen,
                            bool has_expected, uds_compare_t mode) {
    if (!has_expected) {
        return alen > 0 && actual[0] != 0x7F;
    }
    if (mode == UDS_COMPARE_EXACT) {
        return alen == elen && memcmp(actual, expected, alen) == 0;
    }
    if (mode == UDS_COMPARE_STARTSWITH) {
        return alen >= elen && memcmp(actual, expected, elen) == 0;
    }
    if (mode == UDS_COMPARE_IGNORE_DATA) {
        if (elen >= 1) {
            if (alen < 1 || actual[0] != expected[0]) return false;
        }
        if (elen >= 3 && alen >= 3) {
            return actual[1] == expected[1] && actual[2] == expected[2];
        }
        return true;
    }
    return false;
}

static void fill_nrc(const uint8_t *resp, size_t len, char *out, size_t cap) {
    out[0] = '\0';
    if (len >= 3 && resp[0] == 0x7F)
        snprintf(out, cap, "0x%02X", resp[2]);
}

static int send_session(uds_bus_t *bus, const char *session, uint32_t timeout_ms,
                        uint8_t *req, size_t *req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    uint8_t sub = 0;
    if (session_sub(session, &sub) != 0) return -1;
    req[0] = 0x10;
    req[1] = sub;
    *req_len = 2;
    return bus_send_uds(bus, req, *req_len, resp, resp_cap, resp_len, timeout_ms);
}

int run_suite(uds_bus_t *bus, const setup_config_t *setup,
              const test_case_t *cases, size_t count,
              step_result_t *results) {
    char current_session[32] = "";
    uint64_t last_tp_ms = 0;

    for (size_t i = 0; i < count; i++) {
        const test_case_t *tc = &cases[i];
        step_result_t *r = &results[i];
        memset(r, 0, sizeof(*r));
        strncpy(r->test_id, tc->test_id, sizeof(r->test_id) - 1);
        strncpy(r->session, tc->session, sizeof(r->session) - 1);
        strncpy(r->did_name, tc->did_name, sizeof(r->did_name) - 1);
        strncpy(r->notes, tc->notes, sizeof(r->notes) - 1);
        strncpy(r->status, "OK", sizeof(r->status) - 1);

        if (tc->action == UDS_ACTION_SESSION) strncpy(r->action, "Session", sizeof(r->action) - 1);
        else if (tc->action == UDS_ACTION_READ) strncpy(r->action, "Read", sizeof(r->action) - 1);
        else strncpy(r->action, "Write", sizeof(r->action) - 1);

        if (tc->has_did) snprintf(r->did, sizeof(r->did), "0x%04X", tc->did);

        uint32_t timeout = tc->timeout_ms ? tc->timeout_ms : setup->timeout_ms;
        uint8_t req[UDS_MAX_PAYLOAD];
        uint8_t resp[UDS_MAX_PAYLOAD];
        size_t req_len = 0;
        size_t resp_len = 0;

        double t0 = now_sec();
        int rc = 0;

        if (setup->tester_present) {
            /* lightweight: send 3E 80 occasionally between cases */
            (void)last_tp_ms;
            uint8_t tp[2] = {0x3E, 0x80};
            uint8_t ignore[8];
            size_t ignore_len = 0;
            bus_send_uds(bus, tp, 2, ignore, sizeof(ignore), &ignore_len, 300);
        }

        /* auto session before read/write */
        if ((tc->action == UDS_ACTION_READ || tc->action == UDS_ACTION_WRITE) &&
            tc->session[0] && strcasecmp(current_session, tc->session) != 0) {
            rc = send_session(bus, tc->session, timeout, req, &req_len, resp, sizeof(resp), &resp_len);
            if (rc != 0) {
                strncpy(r->status, "ERROR", sizeof(r->status) - 1);
                snprintf(r->error, sizeof(r->error), "session switch failed (%d)", rc);
                format_hex(req, req_len, r->request_hex, sizeof(r->request_hex));
                r->passed = false;
                r->duration_ms = (now_sec() - t0) * 1000.0;
                continue;
            }
            strncpy(current_session, tc->session, sizeof(current_session) - 1);
        }

        if (tc->action == UDS_ACTION_SESSION) {
            rc = send_session(bus, tc->session[0] ? tc->session : "Default",
                              timeout, req, &req_len, resp, sizeof(resp), &resp_len);
            if (rc == 0) strncpy(current_session, tc->session, sizeof(current_session) - 1);
        } else if (tc->action == UDS_ACTION_READ) {
            if (!tc->has_did) {
                rc = -100;
                snprintf(r->error, sizeof(r->error), "Read requires DID");
            } else {
                req[0] = 0x22;
                req[1] = (uint8_t)((tc->did >> 8) & 0xFF);
                req[2] = (uint8_t)(tc->did & 0xFF);
                req_len = 3;
                rc = bus_send_uds(bus, req, req_len, resp, sizeof(resp), &resp_len, timeout);
            }
        } else if (tc->action == UDS_ACTION_WRITE) {
            if (!tc->has_did || tc->write_len == 0) {
                rc = -101;
                snprintf(r->error, sizeof(r->error), "Write requires DID and WriteData");
            } else {
                req[0] = 0x2E;
                req[1] = (uint8_t)((tc->did >> 8) & 0xFF);
                req[2] = (uint8_t)(tc->did & 0xFF);
                memcpy(req + 3, tc->write_data, tc->write_len);
                req_len = 3 + tc->write_len;
                rc = bus_send_uds(bus, req, req_len, resp, sizeof(resp), &resp_len, timeout);
            }
        }

        format_hex(req, req_len, r->request_hex, sizeof(r->request_hex));
        format_hex(resp, resp_len, r->response_hex, sizeof(r->response_hex));
        format_hex(resp, resp_len, r->actual_hex, sizeof(r->actual_hex));
        if (tc->has_expected)
            format_hex(tc->expected, tc->expected_len, r->expected_hex, sizeof(r->expected_hex));
        fill_nrc(resp, resp_len, r->nrc, sizeof(r->nrc));

        if (rc != 0) {
            if (!r->error[0]) snprintf(r->error, sizeof(r->error), "transport error %d", rc);
            strncpy(r->status, "ERROR", sizeof(r->status) - 1);
            r->passed = false;
        } else {
            if (resp_len > 0 && resp[0] == 0x7F) strncpy(r->status, "NRC", sizeof(r->status) - 1);
            r->passed = compare_payload(resp, resp_len, tc->expected, tc->expected_len,
                                        tc->has_expected, tc->compare);
            if (!r->passed && strcmp(r->status, "NRC") != 0)
                strncpy(r->status, "MISMATCH", sizeof(r->status) - 1);
        }

        r->duration_ms = (now_sec() - t0) * 1000.0;
    }
    return 0;
}
