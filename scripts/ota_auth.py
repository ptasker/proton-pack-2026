# PlatformIO pre-script for pack_ota: pass the pack Wi-Fi password from the
# git-ignored include/WifiSecrets.h to espota, so it lives in one place.
import os
import re

Import("env")

path = os.path.join(env.subst("$PROJECT_DIR"), "include", "WifiSecrets.h")
password = None
if os.path.isfile(path):
    with open(path, encoding="utf-8") as handle:
        match = re.search(r'AP_PASSWORD\[\]\s*=\s*"([^"\\]*)"', handle.read())
    if match:
        password = match.group(1)

if password:
    env.Append(UPLOAD_FLAGS=["--auth=" + password])
else:
    print("pack_ota: no simple AP_PASSWORD found in include/WifiSecrets.h; "
          "OTA upload will be rejected")
