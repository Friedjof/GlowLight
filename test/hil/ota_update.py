#!/usr/bin/env python3
"""Hardware checks for authenticated OTA updates on one GlowLight lamp."""

import argparse
import sys
import tempfile
import time
from pathlib import Path

from two_lamp_sync import Lamp, wait_until

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

# The HTTP conversation is shared with the rollout tool, so a change to the OTA
# endpoint cannot leave one of the two behind.
from ota_push import read_password, request, update_page, upload  # noqa: E402


DEFAULT_FIRMWARE = PROJECT_ROOT / ".pio/build/esp32c3-integration/firmware.bin"


def assert_no_restart(lamp, boot_count, description):
    time.sleep(1)
    assert lamp.count("Starting Glow") == boot_count, description


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE)
    args = parser.parse_args()

    password = read_password()
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
