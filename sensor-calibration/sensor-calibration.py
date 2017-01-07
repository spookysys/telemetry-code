#!/usr/bin/env python3

import sys
import json
from pprint import pprint
import cal_lib


print('Number of arguments:', len(sys.argv), 'arguments.')
print('Argument List:', str(sys.argv))

filename = sys.argv[1]
with open(filename) as jsonfile:
    data = json.load(jsonfile)

print(data)
