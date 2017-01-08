#!/usr/bin/env python3

import functools
import sys
import math
import json
from pprint import pprint
import numpy as np
import cal_lib
from transforms3d import axangles
from OpenGL.GLUT import *
from OpenGL.GLU import *
from OpenGL.GL import *

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
    assert len(accel_fitted) == len(mag_fitted)
    # for i in range(len(gyro_raw)-1):
    return None

accel_raw, mag_raw, gyro_raw = load_input()

accel_fit = analyze_mag_accel(accel_raw)
mag_fit = analyze_mag_accel(mag_raw)

accel_fitted = [adjust(x, accel_fit['offset'], accel_fit['scale']) for x in accel_raw]
mag_fitted = [adjust(x, mag_fit['offset'], mag_fit['scale']) for x in mag_raw]

gyro_fit = analyze_gyro(accel_fitted, mag_fitted, gyro_raw)





gl_view_rotate = np.array([0, 0])
def gl_display():
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)
    glPushMatrix()

    gluLookAt(0, 0, 5,
              0, 0, 0,
              0, 1, 0)

    mag = np.linalg.norm(gl_view_rotate)
    if mag > 1:
        scaler = 3
        axe = gl_view_rotate / mag
        glRotate(mag/scaler, axe[1], axe[0], 0)

    color = [1, 1, 1]
    glMaterialfv(GL_FRONT, GL_DIFFUSE, color)
    glutWireCube(2)
    
    glPopMatrix()
    glutSwapBuffers()
    return

gl_mouse_last_coord = [0, 0]
def gl_mouse_button(button, state, x, y):
    global gl_mouse_last_coord
    if button == 2:
        sys.exit(0)
    gl_mouse_last_coord = [x, y]

def gl_mouse_motion(x, y):
    global gl_view_rotate, gl_mouse_last_coord
    gl_view_rotate += np.array([x, y]) - gl_mouse_last_coord
    gl_mouse_last_coord = [x, y]

def gl_idle():
    glutPostRedisplay()

def gl_main():
    glutInit(sys.argv)
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH)
    glutInitWindowSize(400, 400)
    glutCreateWindow(b"Visualize")

    glClearColor(0., 0., 0., 1.)

    glShadeModel(GL_SMOOTH)
    glEnable(GL_CULL_FACE)
    glEnable(GL_DEPTH_TEST)
    glEnable(GL_LIGHTING)
    lightZeroPosition = [10., 4., 10., 1.]
    lightZeroColor = [0.8, 1.0, 0.8, 1.0] #green tinged
    glLightfv(GL_LIGHT0, GL_POSITION, lightZeroPosition)
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor)
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.1)
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05)
    glEnable(GL_LIGHT0)
    glutDisplayFunc(gl_display)
    glutMotionFunc(gl_mouse_motion)
    glutMouseFunc(gl_mouse_button)
    glutIdleFunc(gl_idle)
    glMatrixMode(GL_PROJECTION)
    gluPerspective(40., 1., 1., 40.)
    glMatrixMode(GL_MODELVIEW)
    glutMainLoop()
    return


if __name__ == '__main__':
    gl_main()
