#!/usr/bin/env python3
"""Install a firmware image on GlowLight lamps over the network.

This is the plain rollout counterpart to test/hil/ota_update.py: that script
proves the OTA endpoint is hard to abuse and needs the lamp's serial output to
do so, which rules it out for lamps that are only reachable over WiFi. The HTTP
conversation itself lives here and is shared by both.

    GLOW_OTA_PASSWORD=... python3 scripts/ota_push.py \\
        --host glowlight-52877c.local --host glowlight-52c82c.local

Lamps are updated one after another and each one has to answer again before the
next is touched, so a failure cannot take out the whole group at once.
"""

import argparse
import os
import re
import subprocess
import time
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
TOKEN_PATTERN = re.compile(r'name="token" value="([0-9a-f]{32})"')
USERNAME = "glowlight"
STATUS_MARKER = "\n__GLOW_HTTP_STATUS__"


def default_firmware(board="esp32c3"):
    return PROJECT_ROOT / ".pio/build" / board / "firmware.bin"


def request(url, password=None, arguments=None, check=True):
    command = ["curl", "--silent", "--show-error"]
    if password is not None:
        command += ["--digest", "--user", f"{USERNAME}:{password}"]
    if arguments:
        command += arguments
    command += ["--write-out", STATUS_MARKER + "%{http_code}", url]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if check and result.returncode != 0:
        raise AssertionError(f"curl failed with exit code {result.returncode}")
    body, marker, status = result.stdout.rpartition(STATUS_MARKER)
    if not marker:
        raise AssertionError("HTTP response did not contain a status marker")
    return result.returncode, int(status), body


def update_page(base_url, password):
    _, status, page = request(f"{base_url}/update", password)
    assert status == 200, f"authenticated update page returned HTTP {status}"
    token = TOKEN_PATTERN.search(page)
    assert token is not None, "authenticated update page had no one-time token"
    return token.group(1)


def upload(base_url, password, firmware, token=None, extra=None, check=True):
    arguments = [] if extra is None else list(extra)
    if token is not None:
        arguments += ["--form", f"token={token}"]
    arguments += [
        "--form",
        f"firmware=@{firmware};type=application/octet-stream",
    ]
    return request(f"{base_url}/api/ota", password, arguments, check)


def read_password():
    password = os.environ.get("GLOW_OTA_PASSWORD", "")
    if not 12 <= len(password) <= 63:
        raise SystemExit(
            "Set GLOW_OTA_PASSWORD to the lamp's 12-63 character OTA password"
        )
    return password


def wait_until_available(base_url, timeout):
    """The lamp is back when its OTA endpoint demands credentials again.

    Without a serial line this is the only honest completion signal: it means
    the lamp restarted, joined WiFi and started serving again.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        returncode, status, _ = request(f"{base_url}/update", check=False)
        if returncode == 0 and status == 401:
            return True
        time.sleep(2)
    return False


def push(host, password, firmware, timeout):
    base_url = f"http://{host}"
    print(f"==> {host}")

    returncode, status, _ = request(f"{base_url}/update", check=False)
    if returncode != 0 or status != 401:
        print(f"    unreachable (curl {returncode}, HTTP {status})")
        return False

    token = update_page(base_url, password)
    _, status, body = upload(base_url, password, firmware, token)
    if status != 200 or '"ok":true' not in body:
        print(f"    rejected the image: HTTP {status} {body.strip()}")
        return False
    print("    image accepted, waiting for the restart")

    if not wait_until_available(base_url, timeout):
        print(f"    did not answer again within {timeout} s")
        return False
    print("    back online")
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", action="append", required=True,
                        help="lamp hostname or address, repeatable")
    parser.add_argument("--firmware", type=Path, default=default_firmware())
    parser.add_argument("--timeout", type=int, default=90,
                        help="seconds to wait for a lamp to come back")
    args = parser.parse_args()

    password = read_password()
    if not args.firmware.is_file():
        raise SystemExit(f"Firmware image does not exist: {args.firmware}")

    print(f"Image: {args.firmware} ({args.firmware.stat().st_size} bytes)")
    failed = [host for host in args.host
              if not push(host, password, args.firmware, args.timeout)]

    if failed:
        print(f"FAIL: {len(failed)} of {len(args.host)} lamp(s) not updated: "
              + ", ".join(failed))
        return 1

    print(f"PASS: updated {len(args.host)} lamp(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
