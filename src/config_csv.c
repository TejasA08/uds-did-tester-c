#include "uds_tester.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

static void trim(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[--n] = '\0';
    }
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int split_csv_line(char *line, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    while (count < max_fields) {
        fields[count++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    for (int i = 0; i < count; i++) trim(fields[i]);
    return count;
}

static int parse_bool(const char *s) {
    if (!s) return 0;
    if (!strcasecmp(s, "1") || !strcasecmp(s, "y") || !strcasecmp(s, "yes") ||
        !strcasecmp(s, "true") || !strcasecmp(s, "on"))
        return 1;
    return 0;
}

static void setup_defaults(setup_config_t *setup) {
    memset(setup, 0, sizeof(*setup));
    strncpy(setup->peak_channel, "PCAN_USBBUS1", sizeof(setup->peak_channel) - 1);
    setup->interface = UDS_IF_MOCK;
    strncpy(setup->can_type, "classic", sizeof(setup->can_type) - 1);
    setup->bitrate = 500000;
    setup->fd_data_bitrate = 2000000;
    setup->request_id = 0x7E0;
    setup->response_id = 0x7E8;
    setup->timeout_ms = 1000;
    setup->tester_present = false;
    setup->tester_present_interval_ms = 2000;
    strncpy(setup->ecu_name, "MyECU", sizeof(setup->ecu_name) - 1);
}

int load_setup_csv(const char *path, setup_config_t *setup) {
    setup_defaults(setup);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[512];
    bool header_skipped = false;
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char *fields[4] = {0};
        int n = split_csv_line(line, fields, 4);
        if (n < 2) continue;
        if (!header_skipped) {
            header_skipped = true;
            if (!strcasecmp(fields[0], "field") || !strcasecmp(fields[0], "key"))
                continue;
        }
        const char *key = fields[0];
        const char *val = fields[1];
        if (!strcasecmp(key, "peak_channel")) {
            strncpy(setup->peak_channel, val, sizeof(setup->peak_channel) - 1);
        } else if (!strcasecmp(key, "interface")) {
            if (!strcasecmp(val, "pcan")) setup->interface = UDS_IF_PCAN;
            else setup->interface = UDS_IF_MOCK;
        } else if (!strcasecmp(key, "can_type")) {
            strncpy(setup->can_type, val, sizeof(setup->can_type) - 1);
        } else if (!strcasecmp(key, "bitrate")) {
            parse_u32_auto(val, &setup->bitrate);
        } else if (!strcasecmp(key, "fd_data_bitrate")) {
            parse_u32_auto(val, &setup->fd_data_bitrate);
        } else if (!strcasecmp(key, "request_id")) {
            parse_u32_auto(val, &setup->request_id);
        } else if (!strcasecmp(key, "response_id")) {
            parse_u32_auto(val, &setup->response_id);
        } else if (!strcasecmp(key, "timeout_ms")) {
            parse_u32_auto(val, &setup->timeout_ms);
        } else if (!strcasecmp(key, "tester_present")) {
            setup->tester_present = parse_bool(val) ? true : false;
        } else if (!strcasecmp(key, "tester_present_interval_ms")) {
            parse_u32_auto(val, &setup->tester_present_interval_ms);
        } else if (!strcasecmp(key, "ecu_name")) {
            strncpy(setup->ecu_name, val, sizeof(setup->ecu_name) - 1);
        }
    }
    fclose(fp);
    return 0;
}

static uds_action_t parse_action(const char *s) {
    if (!strcasecmp(s, "session")) return UDS_ACTION_SESSION;
    if (!strcasecmp(s, "read")) return UDS_ACTION_READ;
    if (!strcasecmp(s, "write")) return UDS_ACTION_WRITE;
    return (uds_action_t)-1;
}

static uds_compare_t parse_compare(const char *s) {
    if (!s || s[0] == '\0' || !strcasecmp(s, "exact")) return UDS_COMPARE_EXACT;
    if (!strcasecmp(s, "startswith")) return UDS_COMPARE_STARTSWITH;
    if (!strcasecmp(s, "ignore_data")) return UDS_COMPARE_IGNORE_DATA;
    return UDS_COMPARE_EXACT;
}

int load_test_cases_csv(const char *path, test_case_t *cases, size_t cap, size_t *count) {
    *count = 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[1024];
    bool header = true;
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char *f[12] = {0};
        int n = split_csv_line(line, f, 12);
        if (header) {
            header = false;
            if (!strcasecmp(f[0], "testid") || !strcasecmp(f[0], "test_id"))
                continue;
        }
        if (n < 2 || !f[0] || f[0][0] == '\0') continue;
        if (*count >= cap) {
            fclose(fp);
            return -2;
        }

        test_case_t *tc = &cases[*count];
        memset(tc, 0, sizeof(*tc));
        strncpy(tc->test_id, f[0], sizeof(tc->test_id) - 1);
        tc->action = parse_action(f[1] ? f[1] : "");
        if ((int)tc->action < 0) {
            fclose(fp);
            return -3;
        }
        if (n > 2 && f[2]) strncpy(tc->session, f[2], sizeof(tc->session) - 1);
        if (n > 3 && f[3] && f[3][0]) {
            uint32_t did = 0;
            if (parse_u32_auto(f[3], &did) == 0) {
                tc->has_did = true;
                tc->did = (uint16_t)did;
            }
        }
        if (n > 4 && f[4]) strncpy(tc->did_name, f[4], sizeof(tc->did_name) - 1);
        if (n > 5 && f[5] && f[5][0]) {
            if (parse_hex_bytes(f[5], tc->write_data, sizeof(tc->write_data), &tc->write_len) != 0) {
                fclose(fp);
                return -4;
            }
        }
        if (n > 6 && f[6] && f[6][0]) {
            if (parse_hex_bytes(f[6], tc->expected, sizeof(tc->expected), &tc->expected_len) != 0) {
                fclose(fp);
                return -5;
            }
            tc->has_expected = true;
        }
        if (n > 7) tc->compare = parse_compare(f[7]);
        if (n > 8 && f[8] && f[8][0]) parse_u32_auto(f[8], &tc->timeout_ms);
        if (n > 9 && f[9]) strncpy(tc->notes, f[9], sizeof(tc->notes) - 1);
        (*count)++;
    }
    fclose(fp);
    return 0;
}

int write_default_config(const char *setup_path, const char *cases_path) {
    FILE *fs = fopen(setup_path, "w");
    if (!fs) return -1;
    fputs(
        "field,value,notes\n"
        "peak_channel,PCAN_USBBUS1,PCAN_USBBUS1 or PCAN_USBBUS2\n"
        "interface,mock,mock | pcan\n"
        "can_type,classic,classic | fd\n"
        "bitrate,500000,\n"
        "fd_data_bitrate,2000000,\n"
        "request_id,0x7E0,Tester to ECU\n"
        "response_id,0x7E8,ECU to Tester\n"
        "timeout_ms,1000,\n"
        "tester_present,no,yes/no\n"
        "tester_present_interval_ms,2000,\n"
        "ecu_name,MyECU,\n",
        fs);
    fclose(fs);

    FILE *fc = fopen(cases_path, "w");
    if (!fc) return -1;
    fputs(
        "testid,action,session,did,did_name,writedata,expected,compare,timeout_ms,notes\n"
        "TC001,Session,Extended,,,,50 03,startswith,,Enter extended\n"
        "TC002,Read,Extended,0xF190,VIN,,62 F1 90,startswith,1000,Sample read\n"
        "TC003,Write,Extended,0xF1A0,Coding,01 02 03,6E F1 A0,exact,1000,Sample write\n"
        "TC004,Read,Extended,0xF1A0,Verify coding,,62 F1 A0 01 02 03,exact,1000,Verify after write\n",
        fc);
    fclose(fc);
    return 0;
}
