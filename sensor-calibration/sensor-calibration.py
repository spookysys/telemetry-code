#!/usr/bin/env python3

import functools
import sys
import math
import json
from pprint import pprint
import numpy as np
import cal_lib
from transforms3d import axangles
from rigid_transform_3D import rigid_rotation_3D, rigid_transform_3D


def load_input():
    if len(sys.argv) != 2:
        raise FileNotFoundError("Please specify input file as only argument")

    with open(sys.argv[1]) as jsonfile:
        input_json = json.load(jsonfile)

    accel_raw = [item['accel'] for item in input_json]
    mag_raw = [item['mag'] for item in input_json]
    gyro_raw = [item['gyro'] for item in input_json]

    return accel_raw, mag_raw, gyro_raw



def adjust(vec, offset, scale):
    vec = np.array(vec)
    offset = np.array(offset)
    scale = np.array(scale)
    return ((vec - offset) / scale).tolist()



def analyze_mag_accel(vecs):
    vec_x = [item[0] for item in vecs]
    vec_y = [item[1] for item in vecs]
    vec_z = [item[2] for item in vecs]
    offset, scale = cal_lib.calibrate(np.array(vec_x), np.array(vec_y), np.array(vec_z))
    return {'offset': offset, 'scale': scale}




def analyze_gyro(accel_fitted, mag_fitted, gyro_raw):
    assert len(gyro_raw) == len(accel_fitted)
    assert len(accel_fitted) == len(mag_fitted, gyro_raw)

    for i in range(len(gyro_raw)-1):
        



accel_raw, mag_raw, gyro_raw = load_input()

accel_fit = analyze_mag_accel(accel_raw)
mag_fit = analyze_mag_accel(mag_raw)

accel_fitted = [adjust(x, accel_fit['offset'], accel_fit['scale']) for x in accel_raw]
mag_fitted = [adjust(x, mag_fit['offset'], mag_fit['scale']) for x in mag_raw]

gyro_fit = analyze_gyro(accel_fitted, mag_fitted, gyro_raw)

