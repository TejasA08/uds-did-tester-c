#include "uds_tester.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_hex_bytes(const char *text, uint8_t *out, size_t out_cap, size_t *out_len) {
    uint8_t tmp[UDS_MAX_PAYLOAD];
    size_t n = 0;
    char token[64];
    size_t ti = 0;

    if (out_len) *out_len = 0;
    if (!text) return 0;

    for (const char *p = text;; p++) {
        char c = *p;
        bool end = (c == '\0');
        bool sep = end || isspace((unsigned char)c) || c == ',' || c == ';';

        if (!sep) {
            if (ti + 1 >= sizeof(token)) return -1;
            token[ti++] = c;
            continue;
        }

        if (ti > 0) {
            token[ti] = '\0';
            /* strip optional 0x */
            const char *s = token;
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

            size_t len = strlen(s);
            if (len == 0) {
                ti = 0;
                if (end) break;
                continue;
            }

            if (len % 2 != 0) {
                /* single nibble token like "A" -> 0x0A */
                if (len != 1) return -1;
                int v = hex_nibble(s[0]);
                if (v < 0) return -1;
                if (n >= sizeof(tmp)) return -1;
                tmp[n++] = (uint8_t)v;
            } else {
                for (size_t i = 0; i < len; i += 2) {
                    int hi = hex_nibble(s[i]);
                    int lo = hex_nibble(s[i + 1]);
                    if (hi < 0 || lo < 0) return -1;
                    if (n >= sizeof(tmp)) return -1;
                    tmp[n++] = (uint8_t)((hi << 4) | lo);
                }
            }
            ti = 0;
        }

        if (end) break;
    }

    if (n > out_cap) return -1;
    memcpy(out, tmp, n);
    if (out_len) *out_len = n;
    return 0;
}

void format_hex(const uint8_t *data, size_t len, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!data || len == 0) return;

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        int need = (i == 0) ? 2 : 3;
        if (pos + (size_t)need + 1 > out_cap) break;
        if (i == 0)
            pos += (size_t)snprintf(out + pos, out_cap - pos, "%02X", data[i]);
        else
            pos += (size_t)snprintf(out + pos, out_cap - pos, " %02X", data[i]);
    }
}

int parse_u32_auto(const char *text, uint32_t *out) {
    if (!text || !out) return -1;
    while (*text && isspace((unsigned char)*text)) text++;
    if (*text == '\0') return -1;
    char *end = NULL;
    unsigned long v = strtoul(text, &end, 0);
    if (end == text) return -1;
    *out = (uint32_t)v;
    return 0;
}
