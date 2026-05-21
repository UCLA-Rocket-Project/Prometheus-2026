import ctypes
import sys
import ctypes
import struct
import csv
from typing import Literal

from .types import (
    ADXLLog,
    DigitalV1SecondaryLog,
    DigitalV2SecondaryLog,
    Boards,
    AnalogData,
)


adxl_1_header, adxl_1_footer = 0xDEADBEEF, 0xF00DBABE
adxl_2_header, adxl_2_footer = 0x7F800000, 0xFF800000
secondary_header_digital_v1, secondary_footer_digital_v1 = 0xB33FD3AB, 0xBABEF00B
secondary_header_digital_v2, secondary_footer_digital_v2 = 0xB33FD3AD, 0xBABEF00D


def decode_digital(file_path: str, out_path: str) -> Boards:
    CSV_FIELDS = [
        "timestamp",
        "source",
        "packet_number",
        "acc_x",
        "acc_y",
        "acc_z",
        "imu_acc_x",
        "imu_acc_y",
        "imu_acc_z",
        "gyr_x",
        "gyr_y",
        "gyr_z",
        "temp",
        "pressure",
        "lat",
        "long",
    ]

    assert file_path and len(file_path) != 0, "No file path specified"
    assert out_path and len(out_path) != 0, "No out path specified"

    rows = []  # collect all rows before sorting
    bad_count = 0

    return_type = Boards.ERROR

    with open(file_path, "rb") as file:
        chunk = file.read(4)
        while True:
            if len(chunk) == 0 or len(chunk) < 4:
                print("Seeked to end of file")
                break

            header = None

            # if this error is encountered, it means that there was an incomplete write, and
            # that we are at the end of the file
            try:
                header = struct.unpack("<I", chunk)[0]
            except struct.error as e:
                print(f"Error: {e}")
                chunk = chunk[1:] + file.read(1)
                break

            # back calculate the timestamps
            # data rate here was 400Hz
            if header == adxl_1_header:
                chunk += file.read(ctypes.sizeof(ADXLLog) - 4)
                try:
                    record = ADXLLog.from_buffer_copy(chunk)

                    interval_between_readings_us = (1 / 200) * 1e6
                    start_time = record.last_packet_timestamp - (
                        record.num_readings * interval_between_readings_us
                    )

                    for i in range(record.num_readings):
                        if record.footer != adxl_1_footer:
                            print(
                                f"Bad footer found for adxl 1 packet. Expected: {hex(adxl_1_footer).upper()} but found {hex(record.footer).upper()}"
                            )
                            bad_count += 1
                            chunk = file.read(4)
                            continue

                        rows.append(
                            {
                                "timestamp": start_time,
                                "source": "ADXL 1",
                                "packet_number": record.packet_number,
                                "acc_x": record.readings[i].x,
                                "acc_y": record.readings[i].y,
                                "acc_z": record.readings[i].z,
                                "imu_acc_x": "",
                                "imu_acc_y": "",
                                "imu_acc_z": "",
                                "gyr_x": "",
                                "gyr_y": "",
                                "gyr_z": "",
                                "temp": "",
                                "pressure": "",
                                "lat": "",
                                "long": "",
                            }
                        )
                        start_time += interval_between_readings_us

                    chunk = file.read(4)
                except Exception as e:
                    print(f"Exception while reading ADXL data: {e}")

            elif header == adxl_2_header:
                chunk += file.read(ctypes.sizeof(ADXLLog) - 4)
                try:
                    record = ADXLLog.from_buffer_copy(chunk)

                    interval_between_readings_us = (1 / 400) * 1e6
                    start_time = record.last_packet_timestamp - (
                        record.num_readings * interval_between_readings_us
                    )

                    for i in range(record.num_readings):
                        if record.footer != adxl_2_footer:
                            print(
                                f"Bad footer found for adxl 2 packet expected: {hex(adxl_2_footer).upper()} but found {hex(record.footer).upper()}"
                            )
                            bad_count += 1
                            chunk = file.read(4)
                            continue

                        rows.append(
                            {
                                "timestamp": start_time,
                                "source": "ADXL 2",
                                "packet_number": record.packet_number,
                                "acc_x": record.readings[i].x,
                                "acc_y": record.readings[i].y,
                                "acc_z": record.readings[i].z,
                                "imu_acc_x": "",
                                "imu_acc_y": "",
                                "imu_acc_z": "",
                                "gyr_x": "",
                                "gyr_y": "",
                                "gyr_z": "",
                                "temp": "",
                                "pressure": "",
                                "lat": "",
                                "long": "",
                            }
                        )
                        start_time += interval_between_readings_us

                    chunk = file.read(4)
                except Exception as e:
                    print(f"Exception while reading ADXL data: {e}")
            elif header == secondary_header_digital_v2:
                chunk += file.read(ctypes.sizeof(DigitalV2SecondaryLog) - 4)
                try:
                    record = DigitalV2SecondaryLog.from_buffer_copy(chunk)

                    if record.footer != secondary_footer_digital_v2:
                        print(
                            f"Bad footer found for digital V2 secondary packet. Expected {hex(secondary_footer_digital_v2).upper()} but got {hex(record.footer).upper()}"
                        )
                        bad_count += 1
                        chunk = file.read(4)
                        continue

                    return_type = Boards.DIGITAL_2

                    rows.append(
                        {
                            "timestamp": float(record.timestamp),
                            "source": "SECONDARY V2",
                            "packet_number": record.packet_number,
                            "acc_x": "",
                            "acc_y": "",
                            "acc_z": "",
                            "imu_acc_x": record.acc_x,
                            "imu_acc_y": record.acc_y,
                            "imu_acc_z": record.acc_z,
                            "gyr_x": record.gyr_x,
                            "gyr_y": record.gyr_y,
                            "gyr_z": record.gyr_z,
                            "temp": record.temp,
                            "pressure": record.pressure,
                            "lat": record.lat,
                            "long": record.long,
                        }
                    )
                    chunk = file.read(4)
                except Exception as e:
                    print(f"Exception while reading Secondary data: {e}")
            elif header == secondary_header_digital_v1:
                chunk += file.read(ctypes.sizeof(DigitalV1SecondaryLog) - 4)
                try:
                    record = DigitalV1SecondaryLog.from_buffer_copy(chunk)

                    if record.footer != secondary_footer_digital_v1:
                        print(
                            f"Bad footer found for digital V1 secondary packet. Expected {hex(secondary_footer_digital_v1).upper()} but got {hex(record.footer).upper()}"
                        )
                        bad_count += 1
                        chunk = file.read(4)
                        continue

                    return_type = Boards.DIGITAL_1

                    rows.append(
                        {
                            "timestamp": float(record.timestamp),
                            "source": "SECONDARY V1",
                            "packet_number": record.packet_number,
                            "acc_x": "",
                            "acc_y": "",
                            "acc_z": "",
                            "imu_acc_x": record.acc_x,
                            "imu_acc_y": record.acc_y,
                            "imu_acc_z": record.acc_z,
                            "gyr_x": record.gyr_x,
                            "gyr_y": record.gyr_y,
                            "gyr_z": record.gyr_z,
                            "temp": record.temp,
                            "pressure": record.pressure,
                            "lat": "",
                            "long": "",
                        }
                    )
                    chunk = file.read(4)
                except Exception as e:
                    print(f"Exception while reading Secondary data: {e}")

            else:
                chunk = chunk[1:] + file.read(1)

    # sort by packet_number before writing
    rows.sort(key=lambda r: r["packet_number"])

    with open(out_path, "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(rows)

    print(
        f"Wrote {len(rows)} rows to {out_path}. Eliminated {bad_count} bad rows. {bad_count / len(rows) * 100:.3f}% of rows were bad"
    )

    return return_type


analog_start = 0xFF800000


def decode_analog(
    file_path: str, out_path: str, board_type: Literal["analogv1", "analogv2"]
) -> Boards:
    CSV_FIELDS = [
        "timestamp",
        "source",
        "packet_number",
        "pt1",
        "pt2",
        "pt3",
    ]

    rows = []

    with open(file_path, "rb") as file:
        chunk = file.read(4)
        while True:
            if len(chunk) == 0 or len(chunk) < 4:
                print("Seeked to end of file")
                break

            header = None
            try:
                header = struct.unpack("<I", chunk)[0]
            except struct.error as e:
                print(f"Error while trying to get header: {e}")
                chunk = chunk[1:] + file.read(1)
                break

            if header == analog_start:
                # Fix for incomplete packets
                remaining = file.read(ctypes.sizeof(AnalogData) - 4)
                chunk += remaining
                
                if len(chunk) < ctypes.sizeof(AnalogData):
                    print(f"Incomplete analog packet at EOF: got {len(chunk)} bytes")
                    break
                
                record = AnalogData.from_buffer_copy(chunk)
                # chunk += file.read(ctypes.sizeof(AnalogData) - 4)

                # record = AnalogData.from_buffer_copy(chunk)
                rows.append(
                    {
                        "timestamp": float(record.timestamp),
                        "source": (
                            "ANALOG V1" if board_type == "analogv1" else "ANALOG V2"
                        ),
                        "packet_number": record.pkt_number,
                        "pt1": record.pt_readings[0],
                        "pt2": record.pt_readings[1],
                        "pt3": record.pt_readings[2],
                    }
                )

                chunk = file.read(4)

            else:
                chunk = chunk[1:] + file.read(1)

    # sort by packet_number before writing
    rows.sort(key=lambda r: r["packet_number"])
    with open(out_path, "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} rows to {out_path}.")
    return Boards.ANALOG_1 if board_type == "analogv1" else Boards.ANALOG_2
