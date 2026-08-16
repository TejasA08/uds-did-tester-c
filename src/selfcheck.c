/* Offline checks that do not need Peak hardware. */
#include "uds_tester.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#include <direct.h>
#else
#include <strings.h>
#include <sys/stat.h>
#endif

static int fails = 0;

static void expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        fails++;
    } else {
        printf("PASS: %s\n", msg);
    }
}

static void test_hex(void) {
    uint8_t buf[16];
    size_t n = 0;
    expect(parse_hex_bytes("62 F1 90", buf, sizeof(buf), &n) == 0 && n == 3 &&
               buf[0] == 0x62 && buf[1] == 0xF1 && buf[2] == 0x90,
           "parse spaced hex");
    expect(parse_hex_bytes("62F190", buf, sizeof(buf), &n) == 0 && n == 3, "parse compact hex");
    char s[64];
    format_hex(buf, 3, s, sizeof(s));
    expect(strcmp(s, "62 F1 90") == 0, "format hex");
}

static void test_config_and_mock_suite(void) {
#ifdef _WIN32
    _mkdir("build_selfcheck");
    _mkdir("build_selfcheck/config");
    _mkdir("build_selfcheck/reports");
#else
    mkdir("build_selfcheck", 0755);
    mkdir("build_selfcheck/config", 0755);
    mkdir("build_selfcheck/reports", 0755);
#endif
    const char *setup_path = "build_selfcheck/config/setup.csv";
    const char *cases_path = "build_selfcheck/config/test_cases.csv";
    expect(write_default_config(setup_path, cases_path) == 0, "write default config");

    setup_config_t setup;
    expect(load_setup_csv(setup_path, &setup) == 0, "load setup");
    setup.interface = UDS_IF_MOCK;

    test_case_t cases[32];
    size_t count = 0;
    expect(load_test_cases_csv(cases_path, cases, 32, &count) == 0 && count == 4, "load 4 cases");

    uds_bus_t *bus = bus_open(&setup);
    expect(bus != NULL, "open mock bus");
    if (!bus) return;

    step_result_t results[32];
    run_suite(bus, &setup, cases, count, results);
    bus_close(bus);

    size_t passed = 0;
    for (size_t i = 0; i < count; i++)
        if (results[i].passed) passed++;
    expect(passed == count, "all mock cases pass");

    char csv[256], xml[256];
    expect(write_reports("build_selfcheck/reports", &setup, cases_path, results, count,
                         csv, sizeof(csv), xml, sizeof(xml)) == 0,
           "write reports");
    FILE *f = fopen(csv, "r");
    expect(f != NULL, "csv exists");
    if (f) fclose(f);
    f = fopen(xml, "r");
    expect(f != NULL, "xml exists");
    if (f) fclose(f);

    /* NRC path */
    setup_config_t s2 = setup;
    uds_bus_t *b2 = bus_open(&s2);
    test_case_t bad;
    memset(&bad, 0, sizeof(bad));
    strncpy(bad.test_id, "NRC", sizeof(bad.test_id) - 1);
    bad.action = UDS_ACTION_READ;
    strncpy(bad.session, "Default", sizeof(bad.session) - 1);
    bad.has_did = true;
    bad.did = 0xDEAD;
    step_result_t one;
    run_suite(b2, &s2, &bad, 1, &one);
    bus_close(b2);
    expect(!one.passed && strcmp(one.nrc, "0x31") == 0, "NRC 0x31 for unknown DID");
}

int main(void) {
    test_hex();
    test_config_and_mock_suite();
    printf("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
