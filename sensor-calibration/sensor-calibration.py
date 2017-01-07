#!/usr/bin/env python3

import sys
import json
from pprint import pprint
import numpy
from cal_lib import calibrate

def load_input():
    if len(sys.argv) != 2:
        raise FileNotFoundError("Please specify input file as only argument")
    with open(sys.argv[1]) as jsonfile:
        return json.load(jsonfile)


json = load_input()
# pprint(json)


def cal_sphere(json, field):
    vec_x = []
    vec_y = []
    vec_z = []
    for item in json:
        item_vec = item[field]
        vec_x.append(item_vec[0])
        vec_y.append(item_vec[1])
        vec_z.append(item_vec[2])
    (offsets, scale) = calibrate(numpy.array(vec_x), numpy.array(vec_y), numpy.array(vec_z))
    return {'offsets': offsets, 'scale': scale}

def cal_gyro(json):
    return {}

output = {}
output['accel'] = cal_sphere(json, 'accel')
output['mag'] = cal_sphere(json, 'mag')
output['gyro'] = cal_gyro(json)

print(output)

