#include "wifi_manager.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

#define WIFI_SSID      "iPhoneSebDub"
#define WIFI_PASSWORD  "Solna123"

static bool g_started = false;
static int  g_last_status = -99;

static const char* status_str(int s) {
    switch (s) {
        case CYW43_LINK_DOWN:    return "DOWN";
        case CYW43_LINK_JOIN:    return "JOINING";
        case CYW43_LINK_NOIP:    return "CONNECTED (no IP)";
        case CYW43_LINK_UP:      return "UP";
        case CYW43_LINK_FAIL:    return "FAILED";
        case CYW43_LINK_NONET:   return "NO NETWORK";
        case CYW43_LINK_BADAUTH: return "BAD AUTH";
        default:                  return "UNKNOWN";
    }
}

bool wifi_connect(void) {
    if (cyw43_arch_init()) {
        printf("WiFi: init failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    printf("WiFi: starting async connect to \"%s\"...\n", WIFI_SSID);

    int err = cyw43_arch_wifi_connect_async(
        WIFI_SSID, WIFI_PASSWORD,
        CYW43_AUTH_WPA2_MIXED_PSK);

    if (err != 0) {
        printf("WiFi: connect_async failed err=%d\n", err);
        return false;
    }
    g_started = true;
    return true;
}

bool wifi_is_connected(void) {
    if (!g_started) return false;
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

void wifi_poll(void) {
    cyw43_arch_poll();

    if (!g_started) return;

    int s = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (s != g_last_status) {
        g_last_status = s;
        printf("WiFi: status -> %s (%d)\n", status_str(s), s);
        if (s == CYW43_LINK_UP)
            printf("WiFi: IP=%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    }

    // Auto-retry on failure or no-network (e.g. phone hotspot not yet on)
    if (s == CYW43_LINK_FAIL || s == CYW43_LINK_NONET || s == CYW43_LINK_DOWN) {
        static uint32_t last_retry_ms = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_retry_ms >= 10000) {
            last_retry_ms = now;
            printf("WiFi: retrying...\n");
            cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK);
        }
    }
}
