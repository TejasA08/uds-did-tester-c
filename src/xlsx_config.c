#include "uds_tester.h"

#include "miniz.h"
#include "miniz_zip.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#define XLSX_MAX_ROWS 600
#define XLSX_MAX_COLS 16
#define XLSX_CELL_CAP 512
#define XLSX_SS_MAX  4096

typedef struct {
    char cells[XLSX_MAX_ROWS][XLSX_MAX_COLS][XLSX_CELL_CAP];
    int nrows;
    int ncols;
} sheet_grid_t;

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *zip_extract_to_heap(mz_zip_archive *zip, const char *name, size_t *out_size) {
    size_t sz = 0;
    void *p = mz_zip_reader_extract_file_to_heap(zip, name, &sz, 0);
    if (!p) return NULL;
    if (out_size) *out_size = sz;
    return (char *)p;
}

static void xml_unescape_inplace(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (r[0] == '&') {
            if (!strncmp(r, "&amp;", 5)) { *w++ = '&'; r += 5; }
            else if (!strncmp(r, "&lt;", 4)) { *w++ = '<'; r += 4; }
            else if (!strncmp(r, "&gt;", 4)) { *w++ = '>'; r += 4; }
            else if (!strncmp(r, "&quot;", 6)) { *w++ = '"'; r += 6; }
            else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r += 6; }
            else { *w++ = *r++; }
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

static int col_letters_to_index(const char *ref) {
    int col = 0;
    while (*ref && isalpha((unsigned char)*ref)) {
        col = col * 26 + (toupper((unsigned char)*ref) - 'A' + 1);
        ref++;
    }
    return col - 1;
}

static int parse_shared_strings(const char *xml, char ***out_ss, size_t *out_count) {
    /* Count <si> roughly */
    size_t cap = 64;
    size_t n = 0;
    char **ss = calloc(cap, sizeof(char *));
    if (!ss) return -1;

    const char *p = xml;
    while ((p = strstr(p, "<si")) != NULL) {
        const char *si_end = strstr(p, "</si>");
        if (!si_end) break;
        /* collect all <t ...>text</t> inside si (handles rich text simply) */
        char buf[XLSX_CELL_CAP];
        buf[0] = '\0';
        const char *q = p;
        while (q < si_end) {
            const char *t = strstr(q, "<t");
            if (!t || t >= si_end) break;
            const char *gt = strchr(t, '>');
            if (!gt || gt >= si_end) break;
            gt++;
            const char *tend = strstr(gt, "</t>");
            if (!tend || tend > si_end) break;
            size_t len = (size_t)(tend - gt);
            if (len >= sizeof(buf)) len = sizeof(buf) - 1;
            /* append */
            size_t cur = strlen(buf);
            if (cur + len >= sizeof(buf)) len = sizeof(buf) - 1 - cur;
            memcpy(buf + cur, gt, len);
            buf[cur + len] = '\0';
            q = tend + 4;
        }
        xml_unescape_inplace(buf);
        if (n >= cap) {
            cap *= 2;
            char **nss = realloc(ss, cap * sizeof(char *));
            if (!nss) {
                for (size_t i = 0; i < n; i++) free(ss[i]);
                free(ss);
                return -1;
            }
            ss = nss;
        }
        ss[n] = xstrdup(buf);
        if (!ss[n]) {
            for (size_t i = 0; i < n; i++) free(ss[i]);
            free(ss);
            return -1;
        }
        n++;
        p = si_end + 5;
    }
    *out_ss = ss;
    *out_count = n;
    return 0;
}

static void free_shared_strings(char **ss, size_t n) {
    if (!ss) return;
    for (size_t i = 0; i < n; i++) free(ss[i]);
    free(ss);
}

static void set_cell(sheet_grid_t *g, int row, int col, const char *val) {
    if (row < 0 || col < 0 || row >= XLSX_MAX_ROWS || col >= XLSX_MAX_COLS) return;
    strncpy(g->cells[row][col], val ? val : "", XLSX_CELL_CAP - 1);
    if (row + 1 > g->nrows) g->nrows = row + 1;
    if (col + 1 > g->ncols) g->ncols = col + 1;
}

static int parse_sheet_xml(const char *xml, char **ss, size_t sscount, sheet_grid_t *g) {
    memset(g, 0, sizeof(*g));
    const char *p = xml;
    while ((p = strstr(p, "<c")) != NULL) {
        if (p[2] != ' ' && p[2] != '>' && p[2] != '/') {
            p += 2;
            continue;
        }
        const char *tag_end = strchr(p, '>');
        if (!tag_end) break;
        int self_close = (tag_end > p && tag_end[-1] == '/');
        char attrs[256];
        size_t alen = (size_t)(tag_end - p);
        if (alen >= sizeof(attrs)) alen = sizeof(attrs) - 1;
        memcpy(attrs, p, alen);
        attrs[alen] = '\0';

        const char *rpos = strstr(attrs, "r=\"");
        if (!rpos) {
            p = tag_end + 1;
            continue;
        }
        rpos += 3;
        char ref[16];
        size_t ri = 0;
        while (*rpos && *rpos != '"' && ri + 1 < sizeof(ref)) ref[ri++] = *rpos++;
        ref[ri] = '\0';
        int col = col_letters_to_index(ref);
        int row = 0;
        const char *rp = ref;
        while (*rp && isalpha((unsigned char)*rp)) rp++;
        row = atoi(rp) - 1;

        int is_shared = strstr(attrs, "t=\"s\"") != NULL;
        int is_inline = strstr(attrs, "t=\"inlineStr\"") != NULL;

        char value[XLSX_CELL_CAP];
        value[0] = '\0';

        if (self_close) {
            set_cell(g, row, col, "");
            p = tag_end + 1;
            continue;
        }

        const char *cell_end = strstr(tag_end, "</c>");
        if (!cell_end) break;

        if (is_inline) {
            const char *t = strstr(tag_end, "<t");
            if (t && t < cell_end) {
                const char *gt = strchr(t, '>');
                const char *tend = gt ? strstr(gt, "</t>") : NULL;
                if (gt && tend && tend < cell_end) {
                    size_t len = (size_t)(tend - (gt + 1));
                    if (len >= sizeof(value)) len = sizeof(value) - 1;
                    memcpy(value, gt + 1, len);
                    value[len] = '\0';
                }
            }
        } else {
            const char *v = strstr(tag_end, "<v>");
            if (v && v < cell_end) {
                v += 3;
                const char *vend = strstr(v, "</v>");
                if (vend && vend < cell_end) {
                    size_t len = (size_t)(vend - v);
                    if (len >= sizeof(value)) len = sizeof(value) - 1;
                    memcpy(value, v, len);
                    value[len] = '\0';
                }
            }
            if (is_shared && value[0]) {
                size_t idx = (size_t)strtoul(value, NULL, 10);
                if (idx < sscount && ss[idx]) {
                    strncpy(value, ss[idx], sizeof(value) - 1);
                    value[sizeof(value) - 1] = '\0';
                }
            }
        }
        xml_unescape_inplace(value);
        set_cell(g, row, col, value);
        p = cell_end + 4;
    }
    return 0;
}

static int find_sheet_path(mz_zip_archive *zip, const char *sheet_name, char *path_out, size_t path_cap) {
    size_t wb_sz = 0, rel_sz = 0;
    char *wb = zip_extract_to_heap(zip, "xl/workbook.xml", &wb_sz);
    char *rel = zip_extract_to_heap(zip, "xl/_rels/workbook.xml.rels", &rel_sz);
    if (!wb || !rel) {
        free(wb);
        free(rel);
        return -1;
    }

    /* Find <sheet name="Setup" r:id="rId1" .../> */
    char rid[64] = {0};
    const char *p = wb;
    int found = 0;
    while ((p = strstr(p, "<sheet ")) != NULL) {
        const char *end = strchr(p, '>');
        if (!end) break;
        char tag[512];
        size_t n = (size_t)(end - p);
        if (n >= sizeof(tag)) n = sizeof(tag) - 1;
        memcpy(tag, p, n);
        tag[n] = '\0';
        char name_attr[128];
        const char *np = strstr(tag, "name=\"");
        if (!np) {
            p = end + 1;
            continue;
        }
        np += 6;
        size_t ni = 0;
        while (*np && *np != '"' && ni + 1 < sizeof(name_attr)) name_attr[ni++] = *np++;
        name_attr[ni] = '\0';
        if (strcasecmp(name_attr, sheet_name) == 0) {
            const char *idp = strstr(tag, "r:id=\"");
            if (!idp) idp = strstr(tag, "id=\"");
            if (idp) {
                idp = strchr(idp, '"') + 1;
                size_t ii = 0;
                while (*idp && *idp != '"' && ii + 1 < sizeof(rid)) rid[ii++] = *idp++;
                rid[ii] = '\0';
                found = 1;
                break;
            }
        }
        p = end + 1;
    }
    free(wb);
    if (!found) {
        free(rel);
        return -2;
    }

    /* Map rId -> Target by scanning each Relationship element */
    const char *rp = rel;
    int mapped = 0;
    while ((rp = strstr(rp, "<Relationship")) != NULL) {
        const char *end = strchr(rp, '>');
        if (!end) break;
        char tag[512];
        size_t n = (size_t)(end - rp);
        if (n >= sizeof(tag)) n = sizeof(tag) - 1;
        memcpy(tag, rp, n);
        tag[n] = '\0';

        const char *idp = strstr(tag, "Id=\"");
        const char *tp = strstr(tag, "Target=\"");
        if (idp && tp) {
            idp += 4;
            char idbuf[64];
            size_t ii = 0;
            while (*idp && *idp != '"' && ii + 1 < sizeof(idbuf)) idbuf[ii++] = *idp++;
            idbuf[ii] = '\0';
            if (strcmp(idbuf, rid) == 0) {
                tp += 8;
                char target[128];
                size_t ti = 0;
                while (*tp && *tp != '"' && ti + 1 < sizeof(target)) target[ti++] = *tp++;
                target[ti] = '\0';
                free(rel);
                if (!strncmp(target, "/xl/", 4))
                    snprintf(path_out, path_cap, "xl/%s", target + 4);
                else if (!strncmp(target, "xl/", 3))
                    snprintf(path_out, path_cap, "%s", target);
                else
                    snprintf(path_out, path_cap, "xl/%s", target);
                mapped = 1;
                break;
            }
        }
        rp = end + 1;
    }
    if (!mapped) {
        free(rel);
        return -3;
    }
    return 0;
}

static int load_named_sheet(mz_zip_archive *zip, const char *sheet_name, sheet_grid_t *g) {
    char path[256];
    if (find_sheet_path(zip, sheet_name, path, sizeof(path)) != 0) return -1;

    size_t ss_sz = 0;
    char *ss_xml = zip_extract_to_heap(zip, "xl/sharedStrings.xml", &ss_sz);
    char **ss = NULL;
    size_t sscount = 0;
    if (ss_xml) {
        if (parse_shared_strings(ss_xml, &ss, &sscount) != 0) {
            free(ss_xml);
            return -2;
        }
        free(ss_xml);
    }

    size_t sh_sz = 0;
    char *sheet_xml = zip_extract_to_heap(zip, path, &sh_sz);
    if (!sheet_xml) {
        free_shared_strings(ss, sscount);
        return -3;
    }
    int rc = parse_sheet_xml(sheet_xml, ss, sscount, g);
    free(sheet_xml);
    free_shared_strings(ss, sscount);
    return rc;
}

static int parse_bool(const char *s) {
    if (!s) return 0;
    return !strcasecmp(s, "1") || !strcasecmp(s, "y") || !strcasecmp(s, "yes") ||
           !strcasecmp(s, "true") || !strcasecmp(s, "on");
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

static void apply_setup_kv(setup_config_t *setup, const char *key, const char *val) {
    if (!key || !val) return;
    if (!strcasecmp(key, "peak_channel")) {
        strncpy(setup->peak_channel, val, sizeof(setup->peak_channel) - 1);
    } else if (!strcasecmp(key, "interface")) {
        setup->interface = !strcasecmp(val, "pcan") ? UDS_IF_PCAN : UDS_IF_MOCK;
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

static uds_action_t parse_action(const char *s) {
    if (!strcasecmp(s, "session")) return UDS_ACTION_SESSION;
    if (!strcasecmp(s, "read")) return UDS_ACTION_READ;
    if (!strcasecmp(s, "write")) return UDS_ACTION_WRITE;
    return (uds_action_t)-1;
}

static uds_compare_t parse_compare(const char *s) {
    if (!s || !s[0] || !strcasecmp(s, "exact")) return UDS_COMPARE_EXACT;
    if (!strcasecmp(s, "startswith")) return UDS_COMPARE_STARTSWITH;
    if (!strcasecmp(s, "ignore_data")) return UDS_COMPARE_IGNORE_DATA;
    return UDS_COMPARE_EXACT;
}

static int header_index(sheet_grid_t *g, const char *name) {
    for (int c = 0; c < g->ncols && c < XLSX_MAX_COLS; c++) {
        if (!strcasecmp(g->cells[0][c], name)) return c;
    }
    return -1;
}

static const char *cell(sheet_grid_t *g, int row, int col) {
    if (col < 0 || row < 0) return "";
    return g->cells[row][col];
}

int load_config_xlsx(const char *path, setup_config_t *setup,
                     test_case_t *cases, size_t cap, size_t *count) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, path, 0)) {
        fprintf(stderr, "Failed to open xlsx: %s\n", path);
        return -1;
    }

    sheet_grid_t *setup_sheet = calloc(1, sizeof(*setup_sheet));
    sheet_grid_t *cases_sheet = calloc(1, sizeof(*cases_sheet));
    if (!setup_sheet || !cases_sheet) {
        free(setup_sheet);
        free(cases_sheet);
        mz_zip_reader_end(&zip);
        return -1;
    }

    if (load_named_sheet(&zip, "Setup", setup_sheet) != 0) {
        fprintf(stderr, "xlsx missing 'Setup' sheet\n");
        free(setup_sheet);
        free(cases_sheet);
        mz_zip_reader_end(&zip);
        return -2;
    }
    if (load_named_sheet(&zip, "TestCases", cases_sheet) != 0) {
        fprintf(stderr, "xlsx missing 'TestCases' sheet\n");
        free(setup_sheet);
        free(cases_sheet);
        mz_zip_reader_end(&zip);
        return -3;
    }
    mz_zip_reader_end(&zip);

    setup_defaults(setup);
    for (int r = 1; r < setup_sheet->nrows; r++) {
        const char *key = cell(setup_sheet, r, 0);
        const char *val = cell(setup_sheet, r, 1);
        if (!key[0]) continue;
        if (!strcasecmp(key, "Field") || !strcasecmp(key, "key")) continue;
        apply_setup_kv(setup, key, val);
    }

    int c_id = header_index(cases_sheet, "TestID");
    if (c_id < 0) c_id = header_index(cases_sheet, "testid");
    int c_act = header_index(cases_sheet, "Action");
    int c_sess = header_index(cases_sheet, "Session");
    int c_did = header_index(cases_sheet, "DID");
    int c_name = header_index(cases_sheet, "DID_Name");
    int c_write = header_index(cases_sheet, "WriteData");
    int c_exp = header_index(cases_sheet, "Expected");
    int c_cmp = header_index(cases_sheet, "Compare");
    int c_to = header_index(cases_sheet, "TimeoutMs");
    int c_notes = header_index(cases_sheet, "Notes");
    if (c_id < 0 || c_act < 0) {
        fprintf(stderr, "TestCases sheet missing TestID/Action headers\n");
        free(setup_sheet);
        free(cases_sheet);
        return -4;
    }

    *count = 0;
    for (int r = 1; r < cases_sheet->nrows; r++) {
        const char *tid = cell(cases_sheet, r, c_id);
        if (!tid[0]) continue;
        if (*count >= cap) {
            free(setup_sheet);
            free(cases_sheet);
            return -5;
        }
        test_case_t *tc = &cases[*count];
        memset(tc, 0, sizeof(*tc));
        strncpy(tc->test_id, tid, sizeof(tc->test_id) - 1);
        tc->action = parse_action(cell(cases_sheet, r, c_act));
        if ((int)tc->action < 0) {
            free(setup_sheet);
            free(cases_sheet);
            return -6;
        }
        strncpy(tc->session, cell(cases_sheet, r, c_sess), sizeof(tc->session) - 1);
        const char *did = cell(cases_sheet, r, c_did);
        if (did[0]) {
            uint32_t d = 0;
            if (parse_u32_auto(did, &d) == 0) {
                tc->has_did = true;
                tc->did = (uint16_t)d;
            }
        }
        strncpy(tc->did_name, cell(cases_sheet, r, c_name), sizeof(tc->did_name) - 1);
        const char *wd = cell(cases_sheet, r, c_write);
        if (wd[0]) {
            if (parse_hex_bytes(wd, tc->write_data, sizeof(tc->write_data), &tc->write_len) != 0) {
                free(setup_sheet);
                free(cases_sheet);
                return -7;
            }
        }
        const char *ex = cell(cases_sheet, r, c_exp);
        if (ex[0]) {
            if (parse_hex_bytes(ex, tc->expected, sizeof(tc->expected), &tc->expected_len) != 0) {
                free(setup_sheet);
                free(cases_sheet);
                return -8;
            }
            tc->has_expected = true;
        }
        tc->compare = parse_compare(cell(cases_sheet, r, c_cmp));
        const char *to = cell(cases_sheet, r, c_to);
        if (to[0]) parse_u32_auto(to, &tc->timeout_ms);
        strncpy(tc->notes, cell(cases_sheet, r, c_notes), sizeof(tc->notes) - 1);
        (*count)++;
    }
    free(setup_sheet);
    free(cases_sheet);
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

int write_default_xlsx(const char *path) {
    /* Prefer shipping templates/test_cases.xlsx */
    if (copy_file("templates/test_cases.xlsx", path) == 0) return 0;
    if (copy_file("config/test_cases.xlsx", path) == 0 && strcmp(path, "config/test_cases.xlsx") != 0)
        return 0;
    fprintf(stderr, "Missing templates/test_cases.xlsx — cannot create Excel config.\n");
    return -1;
}
