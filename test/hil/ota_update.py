#!/usr/bin/env python3
"""Hardware checks for authenticated OTA updates on one GlowLight lamp."""

import argparse
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path

from two_lamp_sync import Lamp, wait_until


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FIRMWARE = PROJECT_ROOT / ".pio/build/esp32c3-integration/firmware.bin"
TOKEN_PATTERN = re.compile(r'name="token" value="([0-9a-f]{32})"')
USERNAME = "glowlight"
STATUS_MARKER = "\n__GLOW_HTTP_STATUS__"


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


def assert_no_restart(lamp, boot_count, description):
    time.sleep(1)
    assert lamp.count("Starting Glow") == boot_count, description


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE)
    args = parser.parse_args()

    password = os.environ.get("GLOW_OTA_PASSWORD", "")
    if not 12 <= len(password) <= 63:
        raise SystemExit("Set GLOW_OTA_PASSWORD to the lamp's 12-63 character OTA password")
    if not args.firmware.is_file():
        raise SystemExit(f"Firmware image does not exist: {args.firmware}")

    base_url = f"http://{args.host}"
    lamp = Lamp(args.port)
    try:
        lamp.reset()
        wait_until(
            "OTA server startup",
            lambda: lamp.count_since_boot("Password-protected OTA available") >= 1,
            timeout=35,
        )
        boot_count = lamp.count("Starting Glow")

        _, status, _ = request(f"{base_url}/update")
        assert status == 401, f"unauthenticated update page returned HTTP {status}"
        _, status, _ = request(f"{base_url}/update", password + "-wrong")
        assert status == 401, f"wrong OTA password returned HTTP {status}"
        print("  unauthenticated and wrong-password requests rejected")

        _, status, body = upload(base_url, password, args.firmware)
        assert status == 400 and "INVALID_FIRMWARE" in body, (
            f"tokenless upload returned HTTP {status}: {body}"
        )
        assert_no_restart(lamp, boot_count, "tokenless upload restarted the lamp")
        print("  upload without a one-time token rejected without restart")

        with tempfile.TemporaryDirectory(prefix="glow-ota-") as directory:
            truncated = Path(directory) / "truncated.bin"
            truncated.write_bytes(args.firmware.read_bytes()[:32768])
            token = update_page(base_url, password)
            _, status, body = upload(base_url, password, truncated, token)
            assert status == 400 and "INVALID_FIRMWARE" in body, (
                f"truncated image returned HTTP {status}: {body}"
            )
        assert_no_restart(lamp, boot_count, "truncated image restarted the lamp")
        print("  truncated image rejected without restart")

        token = update_page(base_url, password)
        returncode, _, _ = upload(
            base_url,
            password,
            args.firmware,
            token,
            extra=["--limit-rate", "8k", "--max-time", "3"],
            check=False,
        )
        assert returncode != 0, "aborted upload unexpectedly completed"
        assert_no_restart(lamp, boot_count, "aborted upload restarted the lamp")
        update_page(base_url, password)
        print("  interrupted upload aborted cleanly and server remained available")

        token = update_page(base_url, password)
        _, status, body = upload(base_url, password, args.firmware, token)
        assert status == 200 and '"ok":true' in body, (
            f"valid image returned HTTP {status}: {body}"
        )
        wait_until(
            "restart into the OTA image",
            lambda: lamp.count("Starting Glow") > boot_count,
            timeout=20,
        )
        wait_until(
            "persistent configuration and OTA server after restart",
            lambda: (
                lamp.count_since_boot("Loaded persistent device configuration") >= 1
                and lamp.count_since_boot("Password-protected OTA available") >= 1
            ),
            timeout=35,
        )
        update_page(base_url, password)
        print("  valid image verified, selected, restarted, and retained configuration")
    finally:
        lamp.close()

    print("PASS: OTA authentication, recovery, and valid update hardware checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
