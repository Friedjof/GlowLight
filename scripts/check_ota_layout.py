#!/usr/bin/env python3
"""Verify that the firmware profiles fit the repository's two OTA slots."""

import argparse
import csv
from pathlib import Path


def parse_size(value):
    return int(value.strip(), 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--partitions', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, action='append', required=True)
    args = parser.parse_args()

    rows = {}
    with args.partitions.open(newline='', encoding='ascii') as partition_file:
        for row in csv.reader(
                line for line in partition_file if not line.lstrip().startswith('#')):
            if not row or not row[0].strip():
                continue
            rows[row[0].strip()] = [field.strip() for field in row]

    for name, subtype in (('ota_0', 'ota_0'), ('ota_1', 'ota_1')):
        if name not in rows or len(rows[name]) < 5:
            raise SystemExit(f'missing OTA app partition: {name}')
        if rows[name][1:3] != ['app', subtype]:
            raise SystemExit(f'{name} is not an {subtype} app partition')
    if 'otadata' not in rows or rows['otadata'][1:3] != ['data', 'ota']:
        raise SystemExit('missing OTA data partition')

    slot_size = min(parse_size(rows['ota_0'][4]), parse_size(rows['ota_1'][4]))
    for firmware in args.firmware:
        size = firmware.stat().st_size
        if size > slot_size:
            raise SystemExit(
                f'{firmware} is {size} bytes; OTA slot holds {slot_size} bytes')
        print(f'{firmware}: {size}/{slot_size} bytes')


if __name__ == '__main__':
    main()
