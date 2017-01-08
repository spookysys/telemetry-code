#!/usr/bin/env python3

import functools
import time
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

import affine_fit
import ellipsoid_fit as ellipsoid_fit_py


# settings
sample_hz = 10
target_hz = 200
precision_scale = float(1<<8)

# visualization
autorotate = True
draw_accel_mag_cloud = 0.5
draw_gyro_cloud = False
draw_axes = True
draw_anim_accel_mag = True
draw_gyro_sticks = True # Broken, because of overflow culling



def load_input():
    if len(sys.argv) != 2:
        raise FileNotFoundError("Please specify input file as only argument")

    with open(sys.argv[1]) as jsonfile:
        input_json = json.load(jsonfile)

    accel_raw = []
    mag_raw = []
    gyro_raw = []
    sum_scale_error = 0
    for item in input_json:
        accel_raw.append(np.array(item['accel']) / precision_scale)
        mag_raw.append(np.array(item['mag']) / precision_scale)
        gyro_axis = np.array(item['gyro']) / precision_scale
        gyro_mag = item['gyro_mag'] / precision_scale
        sum_scale_error += float(gyro_mag) / np.linalg.norm(gyro_axis)
        gyro_raw.append(normalize(gyro_axis) * gyro_mag)
    print("scale_error (fixed): ", sum_scale_error / len(input_json))

    return accel_raw, mag_raw, gyro_raw



def ellipsoid_fit(data):
    data2 = ellipsoid_fit_py.data_regularize(np.array(data), divs=8)

    center, radii, evecs, v = ellipsoid_fit_py.ellipsoid_fit(np.array(data2))

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
    expected = []
    observed = []
    for f in range(len(accel_fitted)-1):
        expected_rot_mat = rotation_from_two_vectors(
            accel_fitted[f], mag_fitted[f],
            accel_fitted[f+1], mag_fitted[f+1]
        )
        (expected_axis, expected_angle) = axangles.mat2axangle(expected_rot_mat)
        expected_angle *= sample_hz / target_hz * 2**16
        expected.append((expected_axis * expected_angle).tolist())

        observed_axis = normalize(normalize(gyro_raw[f]) + normalize(gyro_raw[f+1]))
        observed_angle = (np.linalg.norm(gyro_raw[f]) + np.linalg.norm(gyro_raw[f+1])) / 2
        observed.append((observed_axis * observed_angle).tolist())
    return (expected, observed)


def normalize(v):
    norm = np.linalg.norm(v)
    if norm == 0:
        return v
    return v / norm


# Calculate current rotation
def rotation_from_two_vectors(t0v0, t0v1, t1v0, t1v1):
    t0v0 = normalize(t0v0)
    t0v1 = normalize(t0v1)
    t0a0 = normalize(t0v0+t0v1)
    t0a1 = normalize(np.cross(t0v0, t0v1))
    t0a2 = normalize(np.cross(t0a0, t0a1))
    t0mat = np.mat([t0a0.tolist(), t0a1.tolist(), t0a2.tolist()])

    t1v0 = normalize(t1v0)
    t1v1 = normalize(t1v1)
    t1a0 = normalize(t1v0+t1v1)
    t1a1 = normalize(np.cross(t1v0, t1v1))
    t1a2 = normalize(np.cross(t1a0, t1a1))
    t1mat = np.mat([t1a0.tolist(), t1a1.tolist(), t1a2.tolist()])

    return t0mat.T * t1mat


accel_raw, mag_raw, gyro_raw = load_input()

accel_fit = ellipsoid_fit(accel_raw)
mag_fit = ellipsoid_fit(mag_raw)

accel_fitted = [ellipsoid_adjust(x, *accel_fit).tolist() for x in accel_raw]
mag_fitted = [ellipsoid_adjust(x, *mag_fit).tolist() for x in mag_raw]

(gyro_expected_full, gyro_observed_full) = analyze_gyro(accel_fitted, mag_fitted, gyro_raw)

# overflow culling
gyro_expected_culled = []
gyro_observed_culled = []
for i in range(len(gyro_expected_full)):
    observed_limit = 25000000 / precision_scale
    expected_limit = observed_limit / 25
    # print(np.array(gyro_observed[i]) / gyro_expected[i])
    if (abs(gyro_observed_full[i][0]) < observed_limit
            and abs(gyro_observed_full[i][1]) < observed_limit
            and abs(gyro_observed_full[i][2]) < observed_limit
            and abs(gyro_expected_full[i][0]) < expected_limit
            and abs(gyro_expected_full[i][1]) < expected_limit
            and abs(gyro_expected_full[i][2]) < expected_limit):
        gyro_expected_culled.append(gyro_expected_full[i])
        gyro_observed_culled.append(gyro_observed_full[i])

print("Gyro Overflow Filter before: ", len(gyro_expected_full), " after: ", len(gyro_expected_culled))


gyro_fit = affine_fit.Affine_Fit(gyro_observed_culled, gyro_expected_culled)
print(gyro_fit.To_Str())
gyro_fitted_culled = [gyro_fit.Transform(vec) for vec in gyro_observed_culled]
gyro_fitted_full = [gyro_fit.Transform(vec) for vec in gyro_observed_full]

total_error = 0
for i in range(len(gyro_fitted_culled)):
    total_error += np.linalg.norm(np.array(gyro_fitted_culled[i]) - gyro_expected_culled[i])
print("gyro avg error: ", total_error / len(gyro_fitted_culled))

gl_anim = 0
gl_view_rotate = np.array([0, 0])
gl_eye_distance = 5
gl_start_time = time.time()
gl_paused = False
def gl_display():
    # Get frame and delta for animation
    if gl_paused:
        t = gl_start_time
    else:
        t = time.time() - gl_start_time
    t *= 4
    d = t % 1
    f = int(t) % (len(accel_raw)-1)
    accel_vec = (1-d)*np.array(accel_fitted[f]) + d*np.array(accel_fitted[f+1])
    mag_vec = (1-d)*np.array(mag_fitted[f]) + d*np.array(mag_fitted[f+1])
    gyro_vec = (1-d)*np.array(gyro_raw[f]) + d*np.array(gyro_raw[f+1])

    # Calculate axes
    axes = [
        normalize(normalize(accel_vec) + normalize(mag_vec)),
        normalize(np.cross(normalize(accel_vec), normalize(mag_vec))),
        None
    ]
    axes[2] = normalize(np.cross(axes[0], axes[1]))


    # Prepare for drawing
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE)
    glPushMatrix()

    gluLookAt(0, 0, gl_eye_distance,
              0, 0, 0,
              0, 1, 0)


    scaler = 3
    glRotate(gl_view_rotate[1]/scaler, 1, 0, 0)
    glRotate(gl_view_rotate[0]/scaler, 0, 1, 0)

    if autorotate:
        rotMat = [axes[0][0], axes[1][0], axes[2][0], 0., axes[0][1], axes[1][1], axes[2][1], 0., axes[0][2], axes[1][2], axes[2][2], 0., 0., 0., 0., 1.]
        glMultMatrixd(rotMat)

    # Draw unit cube
    glLineWidth(1)
    glColor3fv([1, 1, 1])
    glutWireCube(2)

    # Draw mag and accel strips
    if draw_accel_mag_cloud > 0:
        for i in range(2):
            alpha = draw_accel_mag_cloud
            glPointSize(3)
            glColor([[1, 0, 0, alpha], [0, 1, 0, alpha]][i])
            glBegin(GL_POINTS)
            for p in [accel_fitted, mag_fitted][i]:
                glVertex(p)
            glEnd()

    # Draw gyro clouds
    if draw_gyro_cloud:
        scale = target_hz / 10 / 2**16
        alpha = 0.5

        glShadeModel(GL_SMOOTH)
        glLineWidth(1)
        glBegin(GL_LINES)
        for i in range(len(gyro_fitted_culled)):
            glColor([1, 0, 1, alpha])
            glVertex(np.array(gyro_expected_culled[i]) * scale)
            glColor([1, 1, 1, alpha])
            glVertex(np.array(gyro_fitted_culled[i]) * scale)
        glEnd()

        glShadeModel(GL_FLAT)
        glPointSize(3)
        glBegin(GL_POINTS)
        for i in range(len(gyro_fitted_culled)):
            glColor([1, 0, 1, alpha])
            glVertex(np.array(gyro_expected_culled[i]) * scale)
            glColor([1, 1, 1, alpha])
            glVertex(np.array(gyro_fitted_culled[i]) * scale)
        glEnd()


    if draw_anim_accel_mag:
        glLineWidth(1)
        glBegin(GL_LINES)
        glColor(1, 0, 0)
        glVertex(0, 0, 0)
        glVertex(accel_vec)
        glColor(0, 1, 0)
        glVertex(0, 0, 0)
        glVertex(mag_vec)
        glEnd()

    if draw_axes:
        glLineWidth(4)
        glBegin(GL_LINES)
        glColor(1, 1, 0)
        glVertex(0, 0, 0)
        glVertex(axes[0])
        glColor(1, 0, 0)
        glVertex(0, 0, 0)
        glVertex(axes[1])
        glColor(0, 0, 1)
        glVertex(0, 0, 0)
        glVertex(axes[2])
        glEnd()

    if draw_gyro_sticks:
        scale = target_hz / 10 / 2**16
        glLineWidth(4)
        glBegin(GL_LINES)
        glColor(1, 1, .5, 1)
        glVertex(0, 0, 0)
        glVertex(np.array(gyro_expected_full[f]) * scale)
        glColor(1, .5, 1, 1)
        glVertex(0, 0, 0)
        glVertex(np.array(gyro_fitted_full[f]) * scale)        
        glEnd()


    glPopMatrix()
    glutSwapBuffers()
    return

gl_mouse_last_coord = [0, 0]
def gl_mouse_button(button, state, x, y):
    global gl_paused, gl_start_time
    global gl_mouse_last_coord
    if state == 0 and button == 2:
        if not gl_paused:
            gl_paused = True
            gl_start_time = time.time() - gl_start_time
        else:
            gl_paused = False
            gl_start_time = time.time() - gl_start_time

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

    glShadeModel(GL_FLAT)
    glEnable(GL_CULL_FACE)
    glDisable(GL_DEPTH_TEST)

    glFogi(GL_FOG_MODE, GL_LINEAR)
    glFogfv(GL_FOG_COLOR, [0, 0, 0])
    glFogf(GL_FOG_DENSITY, 1.0)
    glFogf(GL_FOG_START, gl_eye_distance-1) # Fog Start Depth
    glFogf(GL_FOG_END, gl_eye_distance+1) # Fog End Depth
    glEnable(GL_FOG)

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
