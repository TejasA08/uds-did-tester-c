#include "uds_tester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#include <sys/stat.h>
#endif

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s [--mock] [--create-config] [--setup PATH] [--cases PATH] [--reports DIR]\n"
            "\n"
            "  --mock            Force interface=mock (no Peak hardware)\n"
            "  --create-config   Write default config CSVs and exit\n"
            "  --setup PATH      Default: config/setup.csv\n"
            "  --cases PATH      Default: config/test_cases.csv\n"
            "  --reports DIR     Default: reports\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *setup_path = "config/setup.csv";
    const char *cases_path = "config/test_cases.csv";
    const char *reports_dir = "reports";
    int force_mock = 0;
    int create_config = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mock")) force_mock = 1;
        else if (!strcmp(argv[i], "--create-config")) create_config = 1;
        else if (!strcmp(argv[i], "--setup") && i + 1 < argc) setup_path = argv[++i];
        else if (!strcmp(argv[i], "--cases") && i + 1 < argc) cases_path = argv[++i];
        else if (!strcmp(argv[i], "--reports") && i + 1 < argc) reports_dir = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (create_config) {
#ifdef _WIN32
        _mkdir("config");
#else
        mkdir("config", 0755);
#endif
        if (write_default_config(setup_path, cases_path) != 0) {
            fprintf(stderr, "Failed to write config templates\n");
            return 1;
        }
        printf("Wrote %s and %s\n", setup_path, cases_path);
        return 0;
    }

    setup_config_t setup;
    if (load_setup_csv(setup_path, &setup) != 0) {
        fprintf(stderr, "Missing setup config. Creating defaults...\n");
#ifdef _WIN32
        _mkdir("config");
#else
        mkdir("config", 0755);
#endif
        write_default_config(setup_path, cases_path);
        fprintf(stderr, "Edit %s and %s, then run again.\n", setup_path, cases_path);
        return 1;
    }
    if (force_mock) setup.interface = UDS_IF_MOCK;

    test_case_t cases[UDS_MAX_CASES];
    size_t case_count = 0;
    int lrc = load_test_cases_csv(cases_path, cases, UDS_MAX_CASES, &case_count);
    if (lrc != 0 || case_count == 0) {
        fprintf(stderr, "Failed to load test cases from %s (rc=%d)\n", cases_path, lrc);
        return 1;
    }

    printf("ECU=%s interface=%s channel=%s\n",
           setup.ecu_name,
           setup.interface == UDS_IF_PCAN ? "pcan" : "mock",
           setup.peak_channel);
    printf("Tx=0x%X Rx=0x%X cases=%zu\n", setup.request_id, setup.response_id, case_count);

    uds_bus_t *bus = bus_open(&setup);
    if (!bus) {
        fprintf(stderr, "Failed to open bus\n");
        return 2;
    }

    step_result_t *results = calloc(case_count, sizeof(step_result_t));
    if (!results) {
        bus_close(bus);
        return 2;
    }

    run_suite(bus, &setup, cases, case_count, results);
    bus_close(bus);

    char csv_path[512];
    char xml_path[512];
    if (write_reports(reports_dir, &setup, cases_path, results, case_count,
                      csv_path, sizeof(csv_path), xml_path, sizeof(xml_path)) != 0) {
        fprintf(stderr, "Failed to write reports\n");
        free(results);
        return 1;
    }

    size_t passed = 0;
    for (size_t i = 0; i < case_count; i++)
        if (results[i].passed) passed++;

    printf("Done: %zu passed, %zu failed\n", passed, case_count - passed);
    printf("CSV report: %s\n", csv_path);
    printf("Excel XML:  %s  (open in Excel)\n", xml_path);

    for (size_t i = 0; i < case_count; i++) {
        const step_result_t *r = &results[i];
        printf("  [%s] %s %s %s req=%s resp=%s%s%s\n",
               r->passed ? "PASS" : "FAIL",
               r->test_id, r->action, r->did[0] ? r->did : "",
               r->request_hex[0] ? r->request_hex : "-",
               r->response_hex[0] ? r->response_hex : "",
               r->error[0] ? " err=" : "",
               r->error);
    }

    int exit_code = (passed == case_count) ? 0 : 1;
    free(results);
    return exit_code;
}
