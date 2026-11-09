#pragma once

// Copy this file to include/WifiSecrets.h (git-ignored) and set your own
// password. Without WifiSecrets.h the pack builds with Wi-Fi turned off.
namespace wifi_secrets
{
    // WPA2 password for the pack's own Wi-Fi network: 8-63 characters.
    constexpr char AP_PASSWORD[] = "change-me-please";

    // The network name is the pack name, set on the web page (Pack name).
}
