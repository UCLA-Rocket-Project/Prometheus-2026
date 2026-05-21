import ctypes


class ADXL375_RP_Reading(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("z", ctypes.c_float),
    ]


class ADXLLog(ctypes.Structure):
    _fields_ = [
        ("header", ctypes.c_uint32),
        ("num_readings", ctypes.c_uint32),
        ("readings", ADXL375_RP_Reading * 33),
        ("packet_number", ctypes.c_uint32),
        ("last_packet_timestamp", ctypes.c_int64),
        ("footer", ctypes.c_uint32),
    ]


class DigitalV2SecondaryLog(ctypes.Structure):
    _fields_ = [
        ("header", ctypes.c_uint32),
        ("acc_x", ctypes.c_float),
        ("acc_y", ctypes.c_float),
        ("acc_z", ctypes.c_float),
        ("gyr_x", ctypes.c_float),
        ("gyr_y", ctypes.c_float),
        ("gyr_z", ctypes.c_float),
        ("lat", ctypes.c_int32),
        ("long", ctypes.c_int32),
        ("temp", ctypes.c_int32),
        ("pressure", ctypes.c_int32),
        ("packet_number", ctypes.c_uint32),
        ("timestamp", ctypes.c_int64),
        ("footer", ctypes.c_uint32),
    ]


class DigitalV1SecondaryLog(ctypes.Structure):
    _fields_ = [
        ("header", ctypes.c_uint32),
        ("acc_x", ctypes.c_float),
        ("acc_y", ctypes.c_float),
        ("acc_z", ctypes.c_float),
        ("gyr_x", ctypes.c_float),
        ("gyr_y", ctypes.c_float),
        ("gyr_z", ctypes.c_float),
        ("temp", ctypes.c_int32),
        ("pressure", ctypes.c_int32),
        ("packet_number", ctypes.c_uint32),
        ("timestamp", ctypes.c_int64),
        ("_padding", ctypes.c_int64),
        ("footer", ctypes.c_uint32),
    ]


assert ctypes.sizeof(DigitalV1SecondaryLog) == ctypes.sizeof(
    DigitalV2SecondaryLog
), "Size of Digital V1 and Digital V2 log are not the same"


from enum import Enum


class Boards(Enum):
    ANALOG_1 = "Analog V1"
    ANALOG_2 = "Analog V2"
    DIGITAL_1 = "Digital V1"
    DIGITAL_2 = "Digital V2"
    ERROR = 4


class AnalogData(ctypes.Structure):
    _fields_ = [
        ("header", ctypes.c_uint32),
        ("pt_readings", ctypes.c_float * 3),
        ("pkt_number", ctypes.c_uint32),
        ("timestamp", ctypes.c_int64),
    ]
