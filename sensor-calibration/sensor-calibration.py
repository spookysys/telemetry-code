#!/usr/bin/env python3

import functools
import json
import math
import sys
import functools
from pprint import pprint

import numpy as np
from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
from transforms3d import axangles

import ellipsoid_fit as ellipsoid_fit_py


def load_input():
    if len(sys.argv) != 2:
        raise FileNotFoundError("Please specify input file as only argument")

    with open(sys.argv[1]) as jsonfile:
        input_json = json.load(jsonfile)

    accel_raw = [item['accel'] for item in input_json]
    mag_raw = [item['mag'] for item in input_json]
    gyro_raw = [item['gyro'] for item in input_json]

    return accel_raw, mag_raw, gyro_raw



def ellipsoid_fit(data):
    data2 = ellipsoid_fit_py.data_regularize(np.array(data), divs=8)

    center, radii, evecs, v = ellipsoid_fit_py.ellipsoid_fit(np.array(data2))

    dataC = data - center.T
    #dataC2 = data2 - center.T

    print(radii)
    a, b, c = radii
    r = 1#(a*b*c)**(1./3.)#preserve volume?
    D = np.array([[r/a, 0., 0.], [0., r/b, 0.], [0., 0., r/c]])
    #http://www.cs.brandeis.edu/~cs155/Lecture_07_6.pdf
    #affine transformation from ellipsoid to sphere (translation excluded)
    TR = evecs.dot(D).dot(evecs.T)

    return (center.flatten(), TR)


def ellipsoid_adjust(vec, center, TR):
    return TR.dot((vec-center).T).T



def analyze_gyro(accel_fitted, mag_fitted, gyro_raw):
    # assert len(gyro_raw) == len(accel_fitted)
    # assert len(accel_fitted) == len(mag_fitted)
    # for i in range(len(gyro_raw)-1):
    return None



accel_raw, mag_raw, gyro_raw = load_input()

accel_fit = ellipsoid_fit(accel_raw)
mag_fit = ellipsoid_fit(mag_raw)

accel_fitted = [ellipsoid_adjust(x, *accel_fit).tolist() for x in accel_raw]
mag_fitted = [ellipsoid_adjust(x, *mag_fit).tolist() for x in mag_raw]

# gyro_fit = analyze_gyro(accel_fitted, mag_fitted, gyro_raw)
gl_scatter_lists = [accel_fitted, mag_fitted]


gl_view_rotate = np.array([0, 0])
gl_eye_distance = 5
def gl_display():
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)
    glPushMatrix()

    gluLookAt(0, 0, gl_eye_distance,
              0, 0, 0,
              0, 1, 0)

    color_list = [
        [1, 0, 0],
        [0, 1, 0]
    ]

    scaler = 3
    glRotate(gl_view_rotate[1]/scaler, 1, 0, 0)
    glRotate(gl_view_rotate[0]/scaler, 0, 1, 0)

    glColor3fv([1, 1, 1])
    glutWireCube(2)

    for i in range(len(gl_scatter_lists)):
        glPointSize(3)
        glColor3fv(color_list[i])
        glBegin(GL_POINTS)
        for p in gl_scatter_lists[i]:
            glVertex(p[0], p[1], p[2])
        glEnd()

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
    glutInitWindowSize(800, 800)
    glutCreateWindow(b"Visualize")

    glClearColor(0., 0., 0., 1.)

    glShadeModel(GL_SMOOTH)
    glEnable(GL_CULL_FACE)
    glEnable(GL_DEPTH_TEST)

    glFogi(GL_FOG_MODE, GL_LINEAR)
    glFogfv(GL_FOG_COLOR, [0, 0, 0])
    glFogf(GL_FOG_DENSITY, 1.0)
    glFogf(GL_FOG_START, gl_eye_distance-1) # Fog Start Depth
    glFogf(GL_FOG_END, gl_eye_distance+1.5) # Fog End Depth
    # glEnable(GL_FOG)

    glutDisplayFunc(gl_display)
    glutMotionFunc(gl_mouse_motion)
    glutMouseFunc(gl_mouse_button)
    glutIdleFunc(gl_idle)
    glMatrixMode(GL_PROJECTION)
    #gluPerspective(40., 1., 1., 40.)
    glOrtho(-2, 2, -2, 2, 1, gl_eye_distance * 2)
    glMatrixMode(GL_MODELVIEW)
    glutMainLoop()
    return


if __name__ == '__main__':
    gl_main()
