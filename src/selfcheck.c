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

static void test_xlsx_and_mock_suite(void) {
    setup_config_t setup;
    test_case_t cases[32];
    size_t count = 0;
    expect(load_config_xlsx("config/test_cases.xlsx", &setup, cases, 32, &count) == 0, "load xlsx");
    expect(count == 4, "xlsx has 4 cases");
    expect(setup.request_id == 0x7E0, "xlsx request id");
    expect(cases[1].did == 0xF190, "xlsx DID");

    setup.interface = UDS_IF_MOCK;
    uds_bus_t *bus = bus_open(&setup);
    expect(bus != NULL, "open mock bus");
    if (!bus) return;

    step_result_t results[32];
    run_suite(bus, &setup, cases, count, results);
    bus_close(bus);

    size_t passed = 0;
    for (size_t i = 0; i < count; i++)
        if (results[i].passed) passed++;
    expect(passed == count, "all mock cases pass from xlsx");

    char csv[256], xml[256];
#ifdef _WIN32
    _mkdir("build_selfcheck");
    _mkdir("build_selfcheck/reports");
#else
    mkdir("build_selfcheck", 0755);
    mkdir("build_selfcheck/reports", 0755);
#endif
    expect(write_reports("build_selfcheck/reports", &setup, "config/test_cases.xlsx", results, count,
                         csv, sizeof(csv), xml, sizeof(xml)) == 0,
           "write reports");
}

int main(void) {
    test_hex();
    test_xlsx_and_mock_suite();
    printf("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
