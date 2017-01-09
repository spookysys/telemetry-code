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
point_precision = 2**8 # precision of input points, and output offsets
scale_precision = 2**32 # precision of output scale factors

# visualization
autorotate = True
draw_accel_mag_cloud = 0.25
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
    for item in input_json:
        accel_raw.append(np.array(item['accel']) / point_precision)
        mag_raw.append(np.array(item['mag']) / point_precision)
        gyro_axis = np.array(item['gyro']) / point_precision
        gyro_mag = item['gyro_mag'] / point_precision
        gyro_raw.append(normalize(gyro_axis) * gyro_mag)

    return accel_raw, mag_raw, gyro_raw



def ellipsoid_fit(data):
    # Regularize
    data2 = ellipsoid_fit_py.data_regularize(np.array(data), divs=8)

    # Calculate fit
    offset, radii, evecs, v = ellipsoid_fit_py.ellipsoid_fit(np.array(data2))

    # Calculate offset and scale and return
    D = np.diag(1 / radii)
    TR = evecs.dot(D).dot(evecs.T)

    # Calculate unit (size of one unit after fitting)
    unit = (radii[0]*radii[1]*radii[2]) ** (1./3.)

    # Return the fit
    return (offset.flatten(), TR.diagonal(), unit)

# return fitted value
def ellipsoid_adjust(vec, offset, scale, unit):
    return (vec - offset) * scale



def gyro_fit(accel_fitted, mag_fitted, gyro_raw):
    # Reconstruct midpoints between recorded samples
    observed = []
    for f in range(len(gyro_raw)-1):
        observed_axis = normalize(normalize(gyro_raw[f]) + normalize(gyro_raw[f+1]))
        observed_angle = (np.linalg.norm(gyro_raw[f]) + np.linalg.norm(gyro_raw[f+1])) / 2
        observed.append((observed_axis * observed_angle).tolist())

    # Calculate expected rotation from accelerometer and magnetometer data
    calculated = []
    for f in range(len(accel_fitted)-1):
        calculated_rot_mat = rotation_from_two_vectors(
            accel_fitted[f], mag_fitted[f],
            accel_fitted[f+1], mag_fitted[f+1]
        )
        (calculated_axis, calculated_angle) = axangles.mat2axangle(calculated_rot_mat)
        calculated_angle *= sample_hz / target_hz * 2**16
        calculated.append((calculated_axis * calculated_angle).tolist())

    # Cull datapoints that might have overflowed the gyro, so they don't participate in calculating the fit
    calculated_culled = []
    observed_culled = []
    for i in range(len(calculated)):
        threshold = 1000000 / point_precision
        if (abs(calculated[i][0]) < threshold and
                abs(calculated[i][1]) < threshold and
                abs(calculated[i][2]) < threshold):
            calculated_culled.append(calculated[i])
            observed_culled.append(observed[i])

    # Calculate fitting
    fit = affine_fit.Affine_Fit(observed_culled, calculated_culled)

    # Fit the data
    fitted_culled = [fit.Transform(vec) for vec in observed_culled]
    fitted = [fit.Transform(vec) for vec in observed]

    # Print a stat
    print("Gyro points culled by overflow protection: ", len(calculated)-len(calculated_culled))

    # Return
    return (fit, calculated, fitted)





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

# Load inputs
accel_raw, mag_raw, gyro_raw = load_input()

# Fit accelerometer and magnetometer to unit sphere
accel_fit = ellipsoid_fit(accel_raw)
mag_fit = ellipsoid_fit(mag_raw)

# Calculate fitted data for accelerometer and magnetometer
accel_fitted = [ellipsoid_adjust(x, *accel_fit).tolist() for x in accel_raw]
mag_fitted = [ellipsoid_adjust(x, *mag_fit).tolist() for x in mag_raw]

# Fit gyroscope data to 
(gyro_fit, gyro_calculated, gyro_fitted) = gyro_fit(accel_fitted, mag_fitted, gyro_raw)

############################################
## PRINT OUTPUT
############################################

output = {
    'accel': {
        'offset': [int(x * point_precision) for x in accel_fit[0]],
        'scale': [int(x * scale_precision) for x in accel_fit[1]],
        'unit': int(accel_fit[2] * point_precision)
    },
    'mag': {
        'offset': [int(x * point_precision) for x in mag_fit[0]],
        'scale': [int(x * scale_precision) for x in mag_fit[1]],
        'unit': int(mag_fit[2] * point_precision)
    }
}

print(output)




############################################
## EVERYTHING BELOW IS VISUALIZATION CODE
############################################


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
            glLineWidth(1)
            glColor([[1, 0, 1, alpha], [0, 1, 0, alpha]][i])
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
        for i in range(len(gyro_fitted)):
            glColor([1, 0, 1, alpha])
            glVertex(np.array(gyro_calculated[i]) * scale)
            glColor([1, 1, 1, alpha])
            glVertex(np.array(gyro_fitted[i]) * scale)
        glEnd()

        glShadeModel(GL_FLAT)
        glPointSize(3)
        glBegin(GL_POINTS)
        for i in range(len(gyro_fitted)):
            glColor([1, 0, 1, alpha])
            glVertex(np.array(gyro_calculated[i]) * scale)
            glColor([1, 1, 1, alpha])
            glVertex(np.array(gyro_fitted[i]) * scale)
        glEnd()


    if draw_anim_accel_mag:
        glLineWidth(1)
        glBegin(GL_LINES)
        glColor(1, 0, 1)
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
        glVertex(np.array(gyro_calculated[f]) * scale)
        glColor(1, .5, 1, 1)
        glVertex(0, 0, 0)
        glVertex(np.array(gyro_fitted[f]) * scale)
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
    gluPerspective(40., 1., 1., 40.)
    #glOrtho(-2, 2, -2, 2, 1, gl_eye_distance * 2)
    glMatrixMode(GL_MODELVIEW)
    glutMainLoop()
    return


if __name__ == '__main__':
    gl_main()
