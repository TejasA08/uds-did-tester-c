#ifndef UDS_TESTER_H
#define UDS_TESTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define UDS_MAX_PAYLOAD      4095
#define UDS_MAX_HEX_CHARS    (UDS_MAX_PAYLOAD * 3 + 1)
#define UDS_MAX_CASES        512
#define UDS_MAX_STR          128
#define UDS_MAX_NOTES        256

typedef enum {
    UDS_IF_MOCK = 0,
    UDS_IF_PCAN = 1
} uds_interface_t;

typedef struct {
    char peak_channel[64];
    uds_interface_t interface;
    char can_type[16];          /* classic | fd */
    uint32_t bitrate;
    uint32_t fd_data_bitrate;
    uint32_t request_id;
    uint32_t response_id;
    uint32_t timeout_ms;
    bool tester_present;
    uint32_t tester_present_interval_ms;
    char ecu_name[64];
} setup_config_t;

typedef enum {
    UDS_ACTION_SESSION = 0,
    UDS_ACTION_READ,
    UDS_ACTION_WRITE
} uds_action_t;

typedef enum {
    UDS_COMPARE_EXACT = 0,
    UDS_COMPARE_STARTSWITH,
    UDS_COMPARE_IGNORE_DATA
} uds_compare_t;

typedef struct {
    char test_id[32];
    uds_action_t action;
    char session[32];
    bool has_did;
    uint16_t did;
    char did_name[64];
    uint8_t write_data[UDS_MAX_PAYLOAD];
    size_t write_len;
    uint8_t expected[UDS_MAX_PAYLOAD];
    size_t expected_len;
    bool has_expected;
    uds_compare_t compare;
    uint32_t timeout_ms; /* 0 = use setup default */
    char notes[UDS_MAX_NOTES];
} test_case_t;

typedef struct {
    char test_id[32];
    char action[16];
    char session[32];
    char did[16];
    char did_name[64];
    char request_hex[UDS_MAX_HEX_CHARS];
    char response_hex[UDS_MAX_HEX_CHARS];
    char expected_hex[UDS_MAX_HEX_CHARS];
    char actual_hex[UDS_MAX_HEX_CHARS];
    bool passed;
    char status[16];
    char nrc[8];
    double duration_ms;
    char notes[UDS_MAX_NOTES];
    char error[256];
} step_result_t;

/* hexutil.c */
int parse_hex_bytes(const char *text, uint8_t *out, size_t out_cap, size_t *out_len);
void format_hex(const uint8_t *data, size_t len, char *out, size_t out_cap);
int parse_u32_auto(const char *text, uint32_t *out);

/* config_csv.c */
int load_setup_csv(const char *path, setup_config_t *setup);
int load_test_cases_csv(const char *path, test_case_t *cases, size_t cap, size_t *count);
int write_default_config(const char *setup_path, const char *cases_path);

/* bus */
typedef struct uds_bus uds_bus_t;
uds_bus_t *bus_open(const setup_config_t *setup);
void bus_close(uds_bus_t *bus);
int bus_send_uds(uds_bus_t *bus, const uint8_t *req, size_t req_len,
                 uint8_t *resp, size_t resp_cap, size_t *resp_len, uint32_t timeout_ms);

/* runner.c */
int run_suite(uds_bus_t *bus, const setup_config_t *setup,
              const test_case_t *cases, size_t count,
              step_result_t *results);

/* report.c */
int write_reports(const char *reports_dir, const setup_config_t *setup,
                  const char *config_note,
                  const step_result_t *results, size_t count,
                  char *out_csv_path, size_t out_csv_cap,
                  char *out_xml_path, size_t out_xml_cap);

#endif
