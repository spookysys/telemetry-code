#!/usr/bin/env python3

# Credit: http://nghiaho.com/?page_id=671
# Could also try: https://github.com/charnley/rmsd/blob/master/calculate_rmsd


from numpy import *
from math import sqrt

# Input: expects Nx3 matrix of points
# Returns R,t
# R = 3x3 rotation matrix
# t = 3x1 column vector


def rigid_transform_3D(A, B):
    assert len(A) == len(B)

    N = A.shape[0]  # total points

    centroid_A = mean(A, axis=0)
    centroid_B = mean(B, axis=0)

    # centre the points
    AA = A - tile(centroid_A, (N, 1))
    BB = B - tile(centroid_B, (N, 1))

    # dot is matrix multiplication for array
    H = transpose(AA) * BB
    U, S, Vt = linalg.svd(H)
    R = Vt.T * U.T

    # special reflection case
    if linalg.det(R) < 0:
        Vt[2, :] *= -1
        R = Vt.T * U.T

    t = -R * centroid_A.T + centroid_B.T

    return R, t


# my attempt at removing translation logic, as my values are centered
def rigid_rotation_3D(AA, BB):
    assert len(AA) == len(BB)

    N = AA.shape[0]  # total points

    # dot is matrix multiplication for array
    H = transpose(AA) * BB

    U, S, Vt = linalg.svd(H)

    R = Vt.T * U.T

    # special reflection case
    if linalg.det(R) < 0:
        # print("Reflection detected")
        Vt[2, :] *= -1
        R = Vt.T * U.T

    return R



# Test with random data
if __name__ == "__main__":
    # Random rotation and translation
    R = mat(random.rand(3, 3))
    # t = mat(random.rand(3, 1))
    t = tile(0, [3, 1])

    # make R a proper rotation matrix, force orthonormal
    U, S, Vt = linalg.svd(R)
    R = U * Vt

    # remove reflection
    if linalg.det(R) < 0:
        Vt[2, :] *= -1
        R = U * Vt

    # number of points
    n = 10

    A = mat(random.rand(n, 3))
    B = R * A.T + tile(t, (1, n))
    B = B.T

    # recover the transformation
    # ret_R, ret_t = rigid_transform_3D(A, B)
    ret_R = rigid_rotation_3D(A, B)
    ret_t = tile(0, [3, 1])

    A2 = (ret_R * A.T) + tile(ret_t, (1, n))
    A2 = A2.T

    # Find the error
    err = A2 - B

    err = multiply(err, err)
    err = sum(err)
    rmse = sqrt(err / n)

    print("Points A")
    print(A)
    print("")

    print("Points B")
    print(B)
    print("")

    print("Rotation")
    print(R)
    print("")

    print("Translation")
    print(t)
    print("")

    print("RMSE:", rmse)
    print("If RMSE is near zero, the function is correct!")
