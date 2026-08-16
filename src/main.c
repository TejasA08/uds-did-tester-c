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

static int ends_with_ci(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    if (n < m) return 0;
    return strcasecmp(s + n - m, suf) == 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s [--mock] [--create-config] [--config PATH] [--reports DIR]\n"
            "\n"
            "  --mock              Force interface=mock (no Peak hardware)\n"
            "  --create-config     Write default Excel config and exit\n"
            "  --config PATH       Default: config/test_cases.xlsx\n"
            "                      (Setup + TestCases sheets). CSV still supported.\n"
            "  --setup PATH        Legacy CSV setup path (with --cases)\n"
            "  --cases PATH        Legacy CSV cases path (with --setup)\n"
            "  --reports DIR       Default: reports\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *config_xlsx = "config/test_cases.xlsx";
    const char *setup_csv = NULL;
    const char *cases_csv = NULL;
    const char *reports_dir = "reports";
    int force_mock = 0;
    int create_config = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mock")) force_mock = 1;
        else if (!strcmp(argv[i], "--create-config")) create_config = 1;
        else if (!strcmp(argv[i], "--config") && i + 1 < argc) config_xlsx = argv[++i];
        else if (!strcmp(argv[i], "--setup") && i + 1 < argc) setup_csv = argv[++i];
        else if (!strcmp(argv[i], "--cases") && i + 1 < argc) cases_csv = argv[++i];
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

#ifdef _WIN32
    _mkdir("config");
    _mkdir("reports");
#else
    mkdir("config", 0755);
    mkdir("reports", 0755);
#endif

    if (create_config) {
        if (write_default_xlsx(config_xlsx) != 0) {
            /* fallback CSV pair */
            if (write_default_config("config/setup.csv", "config/test_cases.csv") != 0) {
                fprintf(stderr, "Failed to write config templates\n");
                return 1;
            }
            printf("Wrote config/setup.csv and config/test_cases.csv\n");
            return 0;
        }
        printf("Wrote %s (sheets: Setup, TestCases)\n", config_xlsx);
        return 0;
    }

    setup_config_t setup;
    test_case_t cases[UDS_MAX_CASES];
    size_t case_count = 0;
    const char *config_note = config_xlsx;
    int loaded = -1;

    if (setup_csv && cases_csv) {
        loaded = load_setup_csv(setup_csv, &setup);
        if (loaded == 0)
            loaded = load_test_cases_csv(cases_csv, cases, UDS_MAX_CASES, &case_count);
        config_note = cases_csv;
    } else if (ends_with_ci(config_xlsx, ".xlsx")) {
        FILE *probe = fopen(config_xlsx, "rb");
        if (!probe) {
            fprintf(stderr, "Missing %s — creating template...\n", config_xlsx);
            if (write_default_xlsx(config_xlsx) != 0) {
                write_default_config("config/setup.csv", "config/test_cases.csv");
                fprintf(stderr, "Edit the config, then run again.\n");
                return 1;
            }
            fprintf(stderr, "Edit %s (Setup + TestCases), then run again.\n", config_xlsx);
            return 1;
        }
        fclose(probe);
        loaded = load_config_xlsx(config_xlsx, &setup, cases, UDS_MAX_CASES, &case_count);
    } else {
        fprintf(stderr, "Unsupported config type (use .xlsx or --setup/--cases CSV)\n");
        return 1;
    }

    if (loaded != 0 || case_count == 0) {
        fprintf(stderr, "Failed to load config (rc=%d, cases=%zu)\n", loaded, case_count);
        return 1;
    }
    if (force_mock) setup.interface = UDS_IF_MOCK;

    printf("ECU=%s interface=%s channel=%s\n",
           setup.ecu_name,
           setup.interface == UDS_IF_PCAN ? "pcan" : "mock",
           setup.peak_channel);
    printf("Tx=0x%X Rx=0x%X cases=%zu config=%s\n",
           setup.request_id, setup.response_id, case_count, config_note);

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
    if (write_reports(reports_dir, &setup, config_note, results, case_count,
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
