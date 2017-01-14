#!/usr/bin/env python3

import json
import math
import sys
import time
from pprint import pprint
import msgpack
import numpy as np
import scipy.signal
from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
import transforms3d as t3d
from transforms3d.utils import normalized_vector

import ellipsoid_fit as ellipsoid_fit_py

# settings
input_hz = 50 # rate of samples in json file (not sensor sample rate)
output_filename = 'calibration_data.msg'
gyro_cutoff = 2**15 * 0.95 # protect against overflow
gyro_scale = 2 * 250. / 2**15 / 360. # -> rotations per second (why 2x ?)
estimate_gyro_offset = True # if false, gyro-offset is read from gyro_offset.json
rota_smoothing = 49

# visualization
draw_autorotate = True
draw_ortho = True
draw_axes = True
draw_accel_mag = True
draw_accel_mag_cloud = 0.5
draw_gyro_fit = False
draw_gyro_strips = False
draw_gyro_sticks = False
draw_gyro_scale = 1. / 2**15



def read_input():
	if len(sys.argv) != 2:
		raise FileNotFoundError("Please specify rotation data input file as only argument.")

	gyro_offset = None
	if not estimate_gyro_offset:
		with open("gyro_offset.json") as jsonfile:
			gyro_offset = json.load(jsonfile)['gyro_offset']

	with open(sys.argv[1]) as jsonfile:
		input_json = json.load(jsonfile)

	accel = [normalized_vector(item['accel']) * item['accel_mag'] for item in input_json]
	mag = [normalized_vector(item['mag']) * item['mag_mag'] for item in input_json]
	gyro = [normalized_vector(item['gyro']) * item['gyro_mag'] for item in input_json]

	return (accel, mag, gyro, gyro_offset)


def pointcloud_offset_scale_fit(from_pts, to_pts):
	scale = []
	offset = []
	for axis in range(len(from_pts[0])):
		x = [x[axis] for x in from_pts]
		y = [y[axis] for y in to_pts]
		fit = np.polyfit(x, y, 1)
		scale.append(fit[0])
		offset.append(fit[1])

	normalize = (scale[0] * scale[1] * scale[2]) ** (1/3)
	return {
		'center': offset,
		'rescale': (scale / normalize).tolist(),
		'normalize': normalize
	}


def pointcloud_scale_fit(from_pts, to_pts, offset):
	scale = []
	for axis in range(len(from_pts[0])):
		x = [x[axis] for x in from_pts]
		y = [y[axis] for y in to_pts]
		slope = np.array(x).dot(y) / np.array(x).dot(x)
		scale.append(slope)

	normalize = (scale[0] * scale[1] * scale[2]) ** (1/3)
	return {
		'center': offset,
		'rescale': (scale / normalize).tolist(),
		'normalize': normalize
	}




# return fitted value
def adjust(vec, fit):
	center = fit['center']
	rescale = fit['rescale']
	normalize = fit['normalize']
	return ((np.array(vec) - center) * rescale) * normalize



def accel_mag_fit(data):
	# Regularize
	data_regular = ellipsoid_fit_py.data_regularize(np.array(data), divs=8)

	# Calculate fit
	center, radii, evecs, v = ellipsoid_fit_py.ellipsoid_fit(np.array(data_regular))

	# Calculate center and scale and return
	D = np.diag(1 / radii)
	TR = evecs.dot(D).dot(evecs.T)

	# Create the fit
	scale = TR.diagonal()
	normalize = (scale[0] * scale[1] * scale[2]) ** (1/3)
	fit = {
		'center': center.flatten().tolist(),
		'rescale': (scale / normalize).tolist(),
		'normalize': normalize
	}

	# Process the data
	fitted = [adjust(x, fit).tolist() for x in data]

	# Return the fit and the fitted data
	return (fit, fitted)



def calculate_expected_gyro(accel, mag):
	length = len(accel)

	# Create rotation matrices from accel/mag lists
	matrices = [
		rotation_from_two_vectors(
			accel[(i - 1) % length], mag[(i - 1) % length],
			accel[(i + 1) % length], mag[(i + 1) % length]
		)
		for i in range(1, length-1)
	]
	matrices.insert(0, matrices[0])
	matrices.append(matrices[-1])

	# Convert matrices to gyroscope-compatible scaled axis
	def cvt(x):
		(d, m) = t3d.axangles.mat2axangle(x)
		m *= input_hz  # per second
		m /= 2*math.pi # radians to rotations per second
		m /= gyro_scale # whatever unit the gyro uses
		return d * m

	return [cvt(x) for x in matrices]


def smooth_vector_list(data, window_length, poly_order=2):
	data = np.array(data).transpose()
	return np.array([
		scipy.signal.savgol_filter(
			x,
			window_length,
			poly_order
		)
		for x in data
	]).transpose()



def smooth_circular_path(data, window_length, poly_order=2):
	magnitudes = [np.linalg.norm(x) for x in data]
	directions = [x/y for x, y in zip(data, magnitudes)]
	magnitudes = scipy.signal.savgol_filter(magnitudes, window_length, poly_order)
	directions = smooth_vector_list(directions, window_length, poly_order)
	return [x*y for x, y in zip(directions, magnitudes)]


def gyro_safe(observed, expected):
	observed_safe = [
		x[0] < gyro_cutoff and x[1] < gyro_cutoff and x[2] < gyro_cutoff
		for x in np.abs(np.array(observed))
	]
	expected_safe = [
		x[0] < gyro_cutoff and x[1] < gyro_cutoff and x[2] < gyro_cutoff
		for x in np.abs(np.array(expected))
	]
	safe = [
		x and y
		for x, y in zip(observed_safe, expected_safe)
	]

	# Print a stat about the culling
	print("Observed points passing overflow protection: ", sum(observed_safe), " of ", len(observed))
	print("Expected points passing overflow protection: ", sum(expected_safe), " of ", len(observed))
	print("Gyro points passing overflow protection: ", sum(safe), " of ", len(observed))

	return safe


def gyro_fit(observed, expected, gyro_offset, safe):
	# regularize
	expected_regular, observed_regular = ellipsoid_fit_py.data_regularize(expected, type="cubic", divs=9, extra=observed)

	# Calculate fit
	observed_safe = [x for i, x in enumerate(observed) if safe[i]]
	expected_safe = [x for i, x in enumerate(expected) if safe[i]]
	if estimate_gyro_offset:
		fit = pointcloud_offset_scale_fit(observed_safe, expected_safe)
	else:
		fit = pointcloud_scale_fit(observed_safe, expected_safe, gyro_offset)

	# Fit gyro data
	fitted = [adjust(x, fit) for x in gyro]

	# Return
	return (fit, fitted, observed_regular, expected_regular)




# Calculate current rotation
def rotation_from_two_vectors(t0v0, t0v1, t1v0, t1v1):
	t0v0 = normalized_vector(t0v0)
	t0v1 = normalized_vector(t0v1)
	t0a0 = normalized_vector(t0v0+t0v1)
	t0a1 = normalized_vector(np.cross(t0v0, t0v1))
	t0a2 = normalized_vector(np.cross(t0a0, t0a1))
	t0mat = np.mat([t0a0.tolist(), t0a1.tolist(), t0a2.tolist()])

	t1v0 = normalized_vector(t1v0)
	t1v1 = normalized_vector(t1v1)
	t1a0 = normalized_vector(t1v0+t1v1)
	t1a1 = normalized_vector(np.cross(t1v0, t1v1))
	t1a2 = normalized_vector(np.cross(t1a0, t1a1))
	t1mat = np.mat([t1a0.tolist(), t1a1.tolist(), t1a2.tolist()])

	return t0mat.T * t1mat



# Load inputs
(accel, mag, gyro, gyro_offset) = read_input()

# Smooth gyro
gyro_orig = gyro
#gyro = smooth_circular_path(gyro, rota_smoothing)

# Smooth accelerometer and magnetometer paths
#accel = smooth_circular_path(accel, rota_smoothing)
#mag = smooth_circular_path(mag, rota_smoothing)

# Fit accelerometer and magnetometer
(accel_fit, accel_fitted) = accel_mag_fit(accel)
(mag_fit, mag_fitted) = accel_mag_fit(mag)

# Calculate expected gyro
expected_gyro = calculate_expected_gyro(accel_fitted, mag_fitted)

# Determine which gyro points are safe from Overflow
gyro_safe = gyro_safe(gyro_orig, expected_gyro)

# Fit gyro
(gyro_fit, gyro_fitted, gyro_observed_regular, gyro_expected_regular) = gyro_fit(gyro, expected_gyro, gyro_offset, gyro_safe)



############################################
## SERIALIZE AND PRINT OUTPUT
############################################

output = {
	'accel': accel_fit,
	'mag': mag_fit
#    'gyro': gyro_fit
}
print()
pprint(output)

print()
print("Running visualization")

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
	f = int(t) % (len(accel)-1)
	accel_vec = (1-d)*np.array(accel_fitted[f]) + d*np.array(accel_fitted[f+1])
	mag_vec = (1-d)*np.array(mag_fitted[f]) + d*np.array(mag_fitted[f+1])
	gyro_vec = (1-d)*np.array(gyro[f]) + d*np.array(gyro[f+1])

	# Calculate axes
	axes = [
		normalized_vector(normalized_vector(accel_vec) + normalized_vector(mag_vec)),
		normalized_vector(np.cross(normalized_vector(accel_vec), normalized_vector(mag_vec))),
		None
	]
	axes[2] = normalized_vector(np.cross(axes[0], axes[1]))


	# Prepare for drawing
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)
	glEnable(GL_BLEND)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE)
	glPushMatrix()

	gluLookAt(0, 0, gl_eye_distance,
			  0, 0, 0,
			  0, 1, 0)


	scaler = 2
	glRotate(gl_view_rotate[1]/scaler, 1, 0, 0)
	glRotate(gl_view_rotate[0]/scaler, 0, 1, 0)

	if draw_autorotate:
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
			glColor([[.5, 0, 1, alpha], [0, 1, 0, alpha]][i])
			glBegin(GL_LINE_STRIP)
			#glBegin(GL_POINTS)
			for p in [accel_fitted, mag_fitted][i]:
				glVertex(p)
			glEnd()


	# Draw gyro strips
	if draw_gyro_strips:
		for i in (0, 1, 2):
			alpha = 0.8
			glPointSize(3)
			glLineWidth(1)
			glColor([[0, 0, 1, alpha], [0, .5, 0, alpha], [.75, 0, 0, alpha]][i])
			l = [gyro, expected_gyro, gyro_fitted][i]
			glBegin(GL_LINES)
			for j in range(len(l)-1):
				if gyro_safe[j] and gyro_safe[j+1]:
					glVertex(l[j] * draw_gyro_scale)
					glVertex(l[j+1] * draw_gyro_scale)
			glEnd()



	# Draw gyro fit cloud
	if draw_gyro_fit:
		alpha = 0.5

		c = [[1, 0, 1, alpha], [0, .5, 0, alpha]]
		v0 = np.array(gyro_observed_regular) * draw_gyro_scale
		v1 = np.array(gyro_expected_regular) * draw_gyro_scale

		glShadeModel(GL_SMOOTH)
		glLineWidth(1)
		glBegin(GL_LINES)
		for i in range(len(gyro_observed_regular)):
			if gyro_safe[i]:
				glColor(c[0])
				glVertex(v0[i])
				glColor(c[1])
				glVertex(v1[i])
		glEnd()

		glShadeModel(GL_FLAT)
		glPointSize(3)
		glBegin(GL_POINTS)
		for i in range(len(gyro_observed_regular)):
			if gyro_safe[i]:
				glColor(c[0])
				glVertex(v0[i])
				glColor(c[1])
				glVertex(v1[i])
		glEnd()


	if draw_accel_mag:
		glLineWidth(1)
		glBegin(GL_LINES)
		glColor(.5, 0, 1)
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
		scale = draw_gyro_scale
		glLineWidth(4)
		glBegin(GL_LINES)
		glColor(1, 1, .5, 1)
		glVertex(0, 0, 0)
		glVertex(np.array(expected_gyro[f]) * scale)
		glColor(1, .5, 1, 1)
		glVertex(0, 0, 0)
		glVertex(np.array(gyro[f]) * scale)
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
	if draw_ortho:
		glOrtho(-2, 2, -2, 2, 1, gl_eye_distance * 2)
	else:
		gluPerspective(40., 1., 1., 40.)
	glMatrixMode(GL_MODELVIEW)
	glutMainLoop()
	return


if __name__ == '__main__':
	gl_main()
