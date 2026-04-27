#include "opensky_client.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"
#include "lwip/altcp.h"
#include "lwip/pbuf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define OPENSKY_HOST  "opensky-network.org"
#define OPENSKY_PORT  443
#define RESP_BUF_SIZE 16384
#define FETCH_TIMEOUT_MS 10000

// Bounding box half-width in degrees (~26 km radius)
#define BOX_LAT  0.23
#define BOX_LON  0.30

typedef enum {
    CS_DNS,
    CS_CONNECTING,
    CS_RECEIVING,
    CS_DONE,
    CS_ERROR
} ConnState;

typedef struct {
    ConnState        state;
    struct altcp_pcb *pcb;
    char             resp[RESP_BUF_SIZE];
    int              resp_len;
    char             req[256];
} OskyCtx;

static OskyCtx g_ctx;

// ── DNS ──────────────────────────────────────────────────────────────────────

static ip_addr_t g_server_addr;

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name;
    if (!ipaddr) {
        printf("OpenSky: DNS failed\n");
        g_ctx.state = CS_ERROR;
        return;
    }
    g_server_addr = *ipaddr;
    g_ctx.state = CS_CONNECTING;
}

// ── TCP/TLS callbacks ────────────────────────────────────────────────────────

static void err_cb(void *arg, err_t err) {
    (void)arg;
    printf("OpenSky: connection error %d\n", (int)err);
    g_ctx.pcb   = NULL;
    g_ctx.state = CS_ERROR;
}

static err_t recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p) {
        // Remote closed the connection — response complete
        g_ctx.state = CS_DONE;
        if (g_ctx.pcb) {
            altcp_close(pcb);
            g_ctx.pcb = NULL;
        }
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    struct pbuf *q = p;
    while (q) {
        int space = RESP_BUF_SIZE - 1 - g_ctx.resp_len;
        if (space <= 0) break;
        int copy = (q->len < (u16_t)space) ? q->len : (u16_t)space;
        memcpy(g_ctx.resp + g_ctx.resp_len, q->payload, copy);
        g_ctx.resp_len += copy;
        q = q->next;
    }
    g_ctx.resp[g_ctx.resp_len] = '\0';

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connected_cb(void *arg, struct altcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        printf("OpenSky: connect cb error %d\n", (int)err);
        g_ctx.state = CS_ERROR;
        return err;
    }

    // Send HTTP GET
    int req_len = strlen(g_ctx.req);
    err_t wr = altcp_write(pcb, g_ctx.req, (u16_t)req_len, TCP_WRITE_FLAG_COPY);
    if (wr != ERR_OK) {
        printf("OpenSky: write error %d\n", (int)wr);
        g_ctx.state = CS_ERROR;
        return wr;
    }
    altcp_output(pcb);
    g_ctx.state = CS_RECEIVING;
    return ERR_OK;
}

// ── JSON parser ───────────────────────────────────────────────────────────────

// Extract field `idx` (0-based) from a JSON array string starting at '['.
// Writes the field text (unquoted) into out[out_size]. Returns false if missing.
static bool json_arr_field(const char *arr, int idx, char *out, int out_size) {
    const char *p = arr;
    if (*p != '[') return false;
    p++;

    int field = 0;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;

        // Collect field value
        const char *start = p;
        int len = 0;
        bool quoted = (*p == '"');

        if (quoted) {
            p++; start = p;
            while (*p && *p != '"') { p++; len++; }
            if (*p == '"') p++;
        } else {
            while (*p && *p != ',' && *p != ']') { p++; len++; }
            // Trim trailing whitespace
            while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\n')) len--;
        }

        if (field == idx) {
            int copy = (len < out_size - 1) ? len : out_size - 1;
            memcpy(out, start, copy);
            out[copy] = '\0';
            return true;
        }

        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        if (*p == ',') { p++; field++; }
    }
    return false;
}

static int parse_opensky_response(const char *resp, TelemetryData *td) {
    // Skip HTTP headers
    const char *body = strstr(resp, "\r\n\r\n");
    if (!body) return 0;
    body += 4;

    // Find "states":[
    const char *states = strstr(body, "\"states\":");
    if (!states) return 0;
    states = strchr(states, '[');
    if (!states) return 0;
    states++; // skip outer '['

    int count = 0;
    const char *p = states;

    while (*p && count < MAX_TRAFFIC) {
        // Find next state entry '['
        while (*p && *p != '[' && *p != ']') p++;
        if (!*p || *p == ']') break;

        // p points to '[' of this aircraft entry
        const char *entry = p;

        // Find matching ']'
        const char *end = entry + 1;
        while (*end && *end != ']') {
            if (*end == '"') {
                end++;
                while (*end && *end != '"') end++;
            }
            if (*end) end++;
        }

        TrafficData td_tmp = {0};
        char fld[32];

        // Field 1: callsign
        if (!json_arr_field(entry, 1, fld, sizeof(fld))) { p = end + 1; continue; }
        // Trim trailing spaces
        for (int i = strlen(fld) - 1; i >= 0 && fld[i] == ' '; i--) fld[i] = '\0';
        if (strlen(fld) == 0) { p = end + 1; continue; }
        strncpy(td_tmp.id, fld, sizeof(td_tmp.id) - 1);

        // Field 5: longitude
        if (!json_arr_field(entry, 5, fld, sizeof(fld))) { p = end + 1; continue; }
        if (strcmp(fld, "null") == 0) { p = end + 1; continue; }
        td_tmp.lon = atof(fld);

        // Field 6: latitude
        if (!json_arr_field(entry, 6, fld, sizeof(fld))) { p = end + 1; continue; }
        if (strcmp(fld, "null") == 0) { p = end + 1; continue; }
        td_tmp.lat = atof(fld);

        // Field 7: baro_altitude (meters → feet)
        if (json_arr_field(entry, 7, fld, sizeof(fld)) && strcmp(fld, "null") != 0)
            td_tmp.alt = atof(fld) * 3.28084;

        // Field 9: velocity (m/s → knots)
        if (json_arr_field(entry, 9, fld, sizeof(fld)) && strcmp(fld, "null") != 0)
            td_tmp.speed = atof(fld) * 1.944;

        // Field 10: true_track (heading degrees)
        if (json_arr_field(entry, 10, fld, sizeof(fld)) && strcmp(fld, "null") != 0)
            td_tmp.heading = atof(fld);

        td->traffic[count++] = td_tmp;
        p = end + 1;
    }

    return count;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool opensky_fetch(double lat, double lon, TelemetryData *td) {
    // Build bounding-box query
    double lamin = lat - BOX_LAT, lamax = lat + BOX_LAT;
    double lomin = lon - BOX_LON, lomax = lon + BOX_LON;

    snprintf(g_ctx.req, sizeof(g_ctx.req),
        "GET /api/states/all?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f HTTP/1.1\r\n"
        "Host: " OPENSKY_HOST "\r\n"
        "Connection: close\r\n"
        "User-Agent: PicoW-PilotAssistant/1.0\r\n"
        "\r\n",
        lamin, lomin, lamax, lomax);

    // Init context
    g_ctx.state    = CS_DNS;
    g_ctx.resp_len = 0;
    g_ctx.resp[0]  = '\0';
    g_ctx.pcb      = NULL;

    // DNS lookup
    printf("OpenSky: resolving %s...\n", OPENSKY_HOST);
    ip_addr_t addr;
    cyw43_arch_lwip_begin();
    err_t dns_err = dns_gethostbyname(OPENSKY_HOST, &addr, dns_found_cb, NULL);
    cyw43_arch_lwip_end();

    if (dns_err == ERR_OK) {
        g_server_addr = addr;
        g_ctx.state = CS_CONNECTING;
    } else if (dns_err != ERR_INPROGRESS) {
        printf("OpenSky: DNS error %d\n", (int)dns_err);
        return false;
    }

    // Wait for DNS
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (g_ctx.state == CS_DNS) {
        cyw43_arch_poll();
        sleep_ms(10);
        if (to_ms_since_boot(get_absolute_time()) - t0 > FETCH_TIMEOUT_MS) {
            printf("OpenSky: DNS timeout\n");
            return false;
        }
    }
    if (g_ctx.state == CS_ERROR) return false;

    // Create TLS-wrapped connection (no cert verification)
    printf("OpenSky: connecting to %s:%d...\n", OPENSKY_HOST, OPENSKY_PORT);
    cyw43_arch_lwip_begin();
    struct altcp_tls_config *tls_cfg = altcp_tls_create_config_client(NULL, 0);
    struct altcp_pcb *pcb = altcp_tls_new(tls_cfg, IPADDR_TYPE_ANY);
    cyw43_arch_lwip_end();

    if (!pcb) {
        printf("OpenSky: altcp_tls_new failed\n");
        return false;
    }

    g_ctx.pcb = pcb;
    cyw43_arch_lwip_begin();
    altcp_arg(pcb, NULL);
    altcp_recv(pcb, recv_cb);
    altcp_err(pcb, err_cb);

    // Set SNI hostname
    mbedtls_ssl_set_hostname(altcp_tls_context(pcb), OPENSKY_HOST);

    err_t conn_err = altcp_connect(pcb, &g_server_addr, OPENSKY_PORT, connected_cb);
    cyw43_arch_lwip_end();

    if (conn_err != ERR_OK) {
        printf("OpenSky: altcp_connect error %d\n", (int)conn_err);
        return false;
    }

    // Poll until done or timeout
    t0 = to_ms_since_boot(get_absolute_time());
    while (g_ctx.state == CS_CONNECTING || g_ctx.state == CS_RECEIVING) {
        cyw43_arch_poll();
        sleep_ms(10);
        if (to_ms_since_boot(get_absolute_time()) - t0 > FETCH_TIMEOUT_MS) {
            printf("OpenSky: fetch timeout\n");
            if (g_ctx.pcb) {
                cyw43_arch_lwip_begin();
                altcp_close(g_ctx.pcb);
                cyw43_arch_lwip_end();
                g_ctx.pcb = NULL;
            }
            return false;
        }
    }

    if (g_ctx.state == CS_ERROR) {
        printf("OpenSky: fetch error\n");
        return false;
    }

    printf("OpenSky: got %d bytes, parsing...\n", g_ctx.resp_len);

    // Set own position
    td->own.lat = lat;
    td->own.lon = lon;
    td->own.alt = 0;
    td->own.pitch = td->own.roll = td->own.yaw = 0;

    // Parse traffic
    td->traffic_count = (uint8_t)parse_opensky_response(g_ctx.resp, td);
    td->valid = (td->traffic_count > 0);
    printf("OpenSky: found %d aircraft\n", td->traffic_count);

    return true;
}
