#pragma once

// Copy this file to include/creds.h and fill in your details.
// creds.h is git-ignored.

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// --- Optional -------------------------------------------------------------

// Discovery registration. Once Wi-Fi is up (DHCP by default) the device
// POSTs {name, ip, port, mac, protocol, firmware, uptime_s} as JSON to this
// URL, again on every reconnect, and every 5 minutes as a refresh. Senders
// query the service to find the device. See tools/discovery_server.py for
// a reference registry. Leave DISCOVERY_URL undefined to disable.
// #define DISCOVERY_URL         "http://192.168.1.10:8787/register"
// #define DISCOVERY_TOKEN       "optional-bearer-token"
// #define DISCOVERY_DEVICE_NAME "living-room"   // default: neoncore-<mac tail>
// #define DISCOVERY_HTTPS       1               // only for https:// URLs (+~125 KB flash)

// Static IP instead of DHCP. All three required; DNS defaults to the gateway.
// #define STATIC_IP      "192.168.1.42"
// #define STATIC_GATEWAY "192.168.1.1"
// #define STATIC_SUBNET  "255.255.255.0"
// #define STATIC_DNS     "192.168.1.1"

// Replace the factory MAC with a locally-administered address.
// #define WIFI_MAC_OVERRIDE {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}

// Password for the fallback access point (used when WIFI_SSID is empty).
// Must be 8+ characters; the AP is open if unset.
// #define WIFI_AP_PASSWORD "changeme123"

// List nearby networks on the serial console at boot.
// #define WIFI_SCAN_ON_BOOT 1
