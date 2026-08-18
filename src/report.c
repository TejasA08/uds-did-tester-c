#include "uds_tester.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define makdir(path) _mkdir(path)
#else
#include <sys/types.h>
#define makdir(path) mkdir(path, 0755)
#endif

static void xml_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    if (!in) {
        out[0] = '\0';
        return;
    }
    for (; *in && o + 1 < cap; in++) {
        if (*in == '&') {
            if (o + 5 >= cap) break;
            memcpy(out + o, "&amp;", 5);
            o += 5;
        } else if (*in == '<') {
            if (o + 4 >= cap) break;
            memcpy(out + o, "&lt;", 4);
            o += 4;
        } else if (*in == '>') {
            if (o + 4 >= cap) break;
            memcpy(out + o, "&gt;", 4);
            o += 4;
        } else {
            out[o++] = *in;
        }
    }
    out[o] = '\0';
}

static void stamp(char *buf, size_t cap) {
    time_t t = time(NULL);
    struct tm *tm_ptr = localtime(&t);
    if (!tm_ptr) {
        snprintf(buf, cap, "unknown");
        return;
    }
    strftime(buf, cap, "%Y%m%d_%H%M%S", tm_ptr);
}

int write_reports(const char *reports_dir, const setup_config_t *setup,
                  const char *config_note,
                  const step_result_t *results, size_t count,
                  char *out_csv_path, size_t out_csv_cap,
                  char *out_xml_path, size_t out_xml_cap) {
    makdir(reports_dir);

    char ts[32];
    stamp(ts, sizeof(ts));

    snprintf(out_csv_path, out_csv_cap, "%s/UDS_Report_%s_%s.csv", reports_dir, setup->ecu_name, ts);
    snprintf(out_xml_path, out_xml_cap, "%s/UDS_Report_%s_%s.xml", reports_dir, setup->ecu_name, ts);

    size_t passed = 0;
    for (size_t i = 0; i < count; i++)
        if (results[i].passed) passed++;

    FILE *csv = fopen(out_csv_path, "w");
    if (!csv) return -1;
    fprintf(csv, "TestID,Action,Session,DID,DID_Name,Request,Response,Expected,Actual,PassFail,Status,NRC,DurationMs,Notes,Error\n");
    for (size_t i = 0; i < count; i++) {
        const step_result_t *r = &results[i];
        fprintf(csv,
                "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%.2f,%s,%s\n",
                r->test_id, r->action, r->session, r->did, r->did_name,
                r->request_hex, r->response_hex, r->expected_hex, r->actual_hex,
                r->passed ? "PASS" : "FAIL", r->status, r->nrc, r->duration_ms,
                r->notes, r->error);
    }
    fclose(csv);

    FILE *xml = fopen(out_xml_path, "w");
    if (!xml) return -1;
    fputs("<?xml version=\"1.0\"?>\n", xml);
    fputs("<?mso-application progid=\"Excel.Sheet\"?>\n", xml);
    fputs("<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n", xml);
    fputs(" xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n", xml);

    fputs("<Worksheet ss:Name=\"Summary\"><Table>\n", xml);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">ECU</Data></Cell><Cell><Data ss:Type=\"String\">%s</Data></Cell></Row>\n", setup->ecu_name);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Config</Data></Cell><Cell><Data ss:Type=\"String\">%s</Data></Cell></Row>\n", config_note ? config_note : "");
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Interface</Data></Cell><Cell><Data ss:Type=\"String\">%s</Data></Cell></Row>\n",
            setup->interface == UDS_IF_PCAN ? "pcan" : "mock");
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Request ID</Data></Cell><Cell><Data ss:Type=\"String\">0x%X</Data></Cell></Row>\n", setup->request_id);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Response ID</Data></Cell><Cell><Data ss:Type=\"String\">0x%X</Data></Cell></Row>\n", setup->response_id);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Total</Data></Cell><Cell><Data ss:Type=\"Number\">%zu</Data></Cell></Row>\n", count);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Passed</Data></Cell><Cell><Data ss:Type=\"Number\">%zu</Data></Cell></Row>\n", passed);
    fprintf(xml, "<Row><Cell><Data ss:Type=\"String\">Failed</Data></Cell><Cell><Data ss:Type=\"Number\">%zu</Data></Cell></Row>\n", count - passed);
    fputs("</Table></Worksheet>\n", xml);

    fputs("<Worksheet ss:Name=\"Results\"><Table>\n", xml);
    const char *headers[] = {
        "TestID", "Action", "Session", "DID", "DID_Name", "Request", "Response",
        "Expected", "Actual", "PassFail", "Status", "NRC", "DurationMs", "Notes", "Error"
    };
    fputs("<Row>", xml);
    for (size_t h = 0; h < sizeof(headers) / sizeof(headers[0]); h++)
        fprintf(xml, "<Cell><Data ss:Type=\"String\">%s</Data></Cell>", headers[h]);
    fputs("</Row>\n", xml);

    for (size_t i = 0; i < count; i++) {
        const step_result_t *r = &results[i];
        char esc[16][512];
        const char *vals[] = {
            r->test_id, r->action, r->session, r->did, r->did_name, r->request_hex,
            r->response_hex, r->expected_hex, r->actual_hex,
            r->passed ? "PASS" : "FAIL", r->status, r->nrc, NULL, r->notes, r->error
        };
        fputs("<Row>", xml);
        for (size_t c = 0; c < 15; c++) {
            if (c == 12) {
                fprintf(xml, "<Cell><Data ss:Type=\"Number\">%.2f</Data></Cell>", r->duration_ms);
            } else {
                xml_escape(vals[c] ? vals[c] : "", esc[c], sizeof(esc[c]));
                fprintf(xml, "<Cell><Data ss:Type=\"String\">%s</Data></Cell>", esc[c]);
            }
        }
        fputs("</Row>\n", xml);
    }
    fputs("</Table></Worksheet>\n</Workbook>\n", xml);
    fclose(xml);
    return 0;
}
