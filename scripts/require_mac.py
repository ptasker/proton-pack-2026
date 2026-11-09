# PlatformIO post-script: refuse to upload unless the connected ESP32's MAC matches
# custom_required_mac, so a build for one pack can never overwrite the other.
import subprocess

Import("env")

required = env.GetProjectOption("custom_required_mac", "").strip().lower()


def check_mac(source, target, env):
    port = env.subst("$UPLOAD_PORT")
    if not required:
        return
    if not port:
        print("require_mac: no upload port detected; refusing to upload")
        env.Exit(1)
    esptool = env.PioPlatform().get_package_dir("tool-esptoolpy") + "/esptool.py"
    result = subprocess.run([env.subst("$PYTHONEXE"), esptool, "--port", port, "read_mac"],
                            capture_output=True, text=True)
    macs = [line.split("MAC:")[1].strip().lower() for line in result.stdout.splitlines() if "MAC:" in line]
    if not macs or macs[0] != required:
        print("require_mac: board on %s has MAC %s, expected %s; refusing to upload"
              % (port, macs[0] if macs else "unknown", required))
        env.Exit(1)
    print("require_mac: %s matches; uploading" % required)


env.AddPreAction("upload", check_mac)
