/**
 * Madgwick AHRS Filter Implementation
 */

#include "madgwick_filter.h"
#include <math.h>

// Fast inverse square root (optional optimization)
static float inv_sqrt(float x) {
    if (x <= 1e-20f || !isfinite(x)) {
        return 0.0f;
    }
    return 1.0f / sqrtf(x);
}

static void reset_quaternion(MadgwickFilter* filter) {
    filter->q.q0 = 1.0f;
    filter->q.q1 = 0.0f;
    filter->q.q2 = 0.0f;
    filter->q.q3 = 0.0f;
}

/**
 * Initialize Madgwick filter
 */
void madgwick_init(MadgwickFilter* filter, float sample_freq, float beta) {
    filter->q.q0 = 1.0f;
    filter->q.q1 = 0.0f;
    filter->q.q2 = 0.0f;
    filter->q.q3 = 0.0f;
    filter->beta = beta;
    filter->sample_freq = sample_freq;
    filter->inv_sample_freq = 1.0f / sample_freq;
}

/**
 * Update filter with 9-DOF sensor data (accel + gyro + mag)
 */
void madgwick_update(MadgwickFilter* filter,
                    float gx, float gy, float gz,
                    float ax, float ay, float az,
                    float mx, float my, float mz) {
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float hx, hy;
    float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz;
    float _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    // Use IMU algorithm if magnetometer measurement invalid
    if ((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
        madgwick_update_imu(filter, gx, gy, gz, ax, ay, az);
        return;
    }

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-filter->q.q1 * gx - filter->q.q2 * gy - filter->q.q3 * gz);
    qDot2 = 0.5f * (filter->q.q0 * gx + filter->q.q2 * gz - filter->q.q3 * gy);
    qDot3 = 0.5f * (filter->q.q0 * gy - filter->q.q1 * gz + filter->q.q3 * gx);
    qDot4 = 0.5f * (filter->q.q0 * gz + filter->q.q1 * gy - filter->q.q2 * gx);

    // Compute feedback only if accelerometer measurement valid
    float acc_sq = ax * ax + ay * ay + az * az;
    if (acc_sq > 1e-8f && isfinite(acc_sq)) {
        // Normalize accelerometer measurement
        recipNorm = inv_sqrt(acc_sq);
        if (recipNorm == 0.0f) {
            goto integrate_9dof;
        }
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Normalize magnetometer measurement
        float mag_sq = mx * mx + my * my + mz * mz;
        recipNorm = inv_sqrt(mag_sq);
        if (recipNorm == 0.0f) {
            goto integrate_9dof;
        }
        mx *= recipNorm;
        my *= recipNorm;
        mz *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        _2q0mx = 2.0f * filter->q.q0 * mx;
        _2q0my = 2.0f * filter->q.q0 * my;
        _2q0mz = 2.0f * filter->q.q0 * mz;
        _2q1mx = 2.0f * filter->q.q1 * mx;
        _2q0 = 2.0f * filter->q.q0;
        _2q1 = 2.0f * filter->q.q1;
        _2q2 = 2.0f * filter->q.q2;
        _2q3 = 2.0f * filter->q.q3;
        _2q0q2 = 2.0f * filter->q.q0 * filter->q.q2;
        _2q2q3 = 2.0f * filter->q.q2 * filter->q.q3;
        q0q0 = filter->q.q0 * filter->q.q0;
        q0q1 = filter->q.q0 * filter->q.q1;
        q0q2 = filter->q.q0 * filter->q.q2;
        q0q3 = filter->q.q0 * filter->q.q3;
        q1q1 = filter->q.q1 * filter->q.q1;
        q1q2 = filter->q.q1 * filter->q.q2;
        q1q3 = filter->q.q1 * filter->q.q3;
        q2q2 = filter->q.q2 * filter->q.q2;
        q2q3 = filter->q.q2 * filter->q.q3;
        q3q3 = filter->q.q3 * filter->q.q3;

        // Reference direction of Earth's magnetic field
        hx = mx * q0q0 - _2q0my * filter->q.q3 + _2q0mz * filter->q.q2 +
             mx * q1q1 + _2q1 * my * filter->q.q2 + _2q1 * mz * filter->q.q3 -
             mx * q2q2 - mx * q3q3;
        hy = _2q0mx * filter->q.q3 + my * q0q0 - _2q0mz * filter->q.q1 +
             _2q1mx * filter->q.q2 - my * q1q1 + my * q2q2 +
             _2q2 * mz * filter->q.q3 - my * q3q3;
        _2bx = sqrtf(hx * hx + hy * hy);
        _2bz = -_2q0mx * filter->q.q2 + _2q0my * filter->q.q1 + mz * q0q0 +
               _2q1mx * filter->q.q3 - mz * q1q1 + _2q2 * my * filter->q.q3 -
               mz * q2q2 + mz * q3q3;
        _4bx = 2.0f * _2bx;
        _4bz = 2.0f * _2bz;

        // Gradient descent algorithm corrective step
        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) +
             _2q1 * (2.0f * q0q1 + _2q2q3 - ay) -
             _2bz * filter->q.q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
             (-_2bx * filter->q.q3 + _2bz * filter->q.q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
             _2bx * filter->q.q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) +
             _2q0 * (2.0f * q0q1 + _2q2q3 - ay) -
             4.0f * filter->q.q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) +
             _2bz * filter->q.q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
             (_2bx * filter->q.q2 + _2bz * filter->q.q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
             (_2bx * filter->q.q3 - _4bz * filter->q.q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) +
             _2q3 * (2.0f * q0q1 + _2q2q3 - ay) -
             4.0f * filter->q.q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) +
             (-_4bx * filter->q.q2 - _2bz * filter->q.q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
             (_2bx * filter->q.q1 + _2bz * filter->q.q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
             (_2bx * filter->q.q0 - _4bz * filter->q.q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) +
             _2q2 * (2.0f * q0q1 + _2q2q3 - ay) +
             (-_4bx * filter->q.q3 + _2bz * filter->q.q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
             (-_2bx * filter->q.q0 + _2bz * filter->q.q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
             _2bx * filter->q.q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

        // Normalize step magnitude (guard against zero to avoid NaN lockups)
        float step_sq = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
        if (step_sq > 1e-12f) {
            recipNorm = inv_sqrt(step_sq);
            s0 *= recipNorm;
            s1 *= recipNorm;
            s2 *= recipNorm;
            s3 *= recipNorm;

            // Apply feedback step
            qDot1 -= filter->beta * s0;
            qDot2 -= filter->beta * s1;
            qDot3 -= filter->beta * s2;
            qDot4 -= filter->beta * s3;
        }
    }

integrate_9dof:
    // Integrate rate of change of quaternion to yield quaternion
    filter->q.q0 += qDot1 * filter->inv_sample_freq;
    filter->q.q1 += qDot2 * filter->inv_sample_freq;
    filter->q.q2 += qDot3 * filter->inv_sample_freq;
    filter->q.q3 += qDot4 * filter->inv_sample_freq;

    // Normalize quaternion
    float q_sq = filter->q.q0 * filter->q.q0 + filter->q.q1 * filter->q.q1 +
                 filter->q.q2 * filter->q.q2 + filter->q.q3 * filter->q.q3;
    recipNorm = inv_sqrt(q_sq);
    if (recipNorm == 0.0f) {
        reset_quaternion(filter);
        return;
    }
    filter->q.q0 *= recipNorm;
    filter->q.q1 *= recipNorm;
    filter->q.q2 *= recipNorm;
    filter->q.q3 *= recipNorm;
}

/**
 * Update filter with 6-DOF sensor data (accel + gyro, no mag)
 */
void madgwick_update_imu(MadgwickFilter* filter,
                        float gx, float gy, float gz,
                        float ax, float ay, float az) {
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2;
    float q0q0, q1q1, q2q2, q3q3;

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-filter->q.q1 * gx - filter->q.q2 * gy - filter->q.q3 * gz);
    qDot2 = 0.5f * (filter->q.q0 * gx + filter->q.q2 * gz - filter->q.q3 * gy);
    qDot3 = 0.5f * (filter->q.q0 * gy - filter->q.q1 * gz + filter->q.q3 * gx);
    qDot4 = 0.5f * (filter->q.q0 * gz + filter->q.q1 * gy - filter->q.q2 * gx);

    // Compute feedback only if accelerometer measurement valid
    float acc_sq = ax * ax + ay * ay + az * az;
    if (acc_sq > 1e-8f && isfinite(acc_sq)) {
        // Normalize accelerometer measurement
        recipNorm = inv_sqrt(acc_sq);
        if (recipNorm == 0.0f) {
            goto integrate_6dof;
        }
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        _2q0 = 2.0f * filter->q.q0;
        _2q1 = 2.0f * filter->q.q1;
        _2q2 = 2.0f * filter->q.q2;
        _2q3 = 2.0f * filter->q.q3;
        _4q0 = 4.0f * filter->q.q0;
        _4q1 = 4.0f * filter->q.q1;
        _4q2 = 4.0f * filter->q.q2;
        _8q1 = 8.0f * filter->q.q1;
        _8q2 = 8.0f * filter->q.q2;
        q0q0 = filter->q.q0 * filter->q.q0;
        q1q1 = filter->q.q1 * filter->q.q1;
        q2q2 = filter->q.q2 * filter->q.q2;
        q3q3 = filter->q.q3 * filter->q.q3;

        // Gradient descent algorithm corrective step
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * filter->q.q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * filter->q.q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * filter->q.q3 - _2q1 * ax + 4.0f * q2q2 * filter->q.q3 - _2q2 * ay;

        // Normalize step magnitude (guard against zero to avoid NaN lockups)
        float step_sq = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
        if (step_sq > 1e-12f) {
            recipNorm = inv_sqrt(step_sq);
            s0 *= recipNorm;
            s1 *= recipNorm;
            s2 *= recipNorm;
            s3 *= recipNorm;

            // Apply feedback step
            qDot1 -= filter->beta * s0;
            qDot2 -= filter->beta * s1;
            qDot3 -= filter->beta * s2;
            qDot4 -= filter->beta * s3;
        }
    }

integrate_6dof:
    // Integrate rate of change of quaternion to yield quaternion
    filter->q.q0 += qDot1 * filter->inv_sample_freq;
    filter->q.q1 += qDot2 * filter->inv_sample_freq;
    filter->q.q2 += qDot3 * filter->inv_sample_freq;
    filter->q.q3 += qDot4 * filter->inv_sample_freq;

    // Normalize quaternion
    float q_sq = filter->q.q0 * filter->q.q0 + filter->q.q1 * filter->q.q1 +
                 filter->q.q2 * filter->q.q2 + filter->q.q3 * filter->q.q3;
    recipNorm = inv_sqrt(q_sq);
    if (recipNorm == 0.0f) {
        reset_quaternion(filter);
        return;
    }
    filter->q.q0 *= recipNorm;
    filter->q.q1 *= recipNorm;
    filter->q.q2 *= recipNorm;
    filter->q.q3 *= recipNorm;
}

/**
 * Convert quaternion to Euler angles
 */
void quaternion_to_euler(const Quaternion* q, float* roll, float* pitch, float* yaw) {
    // Roll (X-axis rotation)
    float sinr_cosp = 2.0f * (q->q0 * q->q1 + q->q2 * q->q3);
    float cosr_cosp = 1.0f - 2.0f * (q->q1 * q->q1 + q->q2 * q->q2);
    *roll = atan2f(sinr_cosp, cosr_cosp);

    // Pitch (Y-axis rotation)
    float sinp = 2.0f * (q->q0 * q->q2 - q->q3 * q->q1);
    if (fabsf(sinp) >= 1.0f) {
        *pitch = copysignf(M_PI / 2.0f, sinp);  // Use 90 degrees if out of range
    } else {
        *pitch = asinf(sinp);
    }

    // Yaw (Z-axis rotation)
    float siny_cosp = 2.0f * (q->q0 * q->q3 + q->q1 * q->q2);
    float cosy_cosp = 1.0f - 2.0f * (q->q2 * q->q2 + q->q3 * q->q3);
    *yaw = atan2f(siny_cosp, cosy_cosp);
}

/**
 * Helper functions to get angles in degrees
 */
float madgwick_get_roll_deg(const MadgwickFilter* filter) {
    float roll, pitch, yaw;
    quaternion_to_euler(&filter->q, &roll, &pitch, &yaw);
    return roll * 180.0f / M_PI;
}

float madgwick_get_pitch_deg(const MadgwickFilter* filter) {
    float roll, pitch, yaw;
    quaternion_to_euler(&filter->q, &roll, &pitch, &yaw);
    return pitch * 180.0f / M_PI;
}

float madgwick_get_yaw_deg(const MadgwickFilter* filter) {
    float roll, pitch, yaw;
    quaternion_to_euler(&filter->q, &roll, &pitch, &yaw);
    return yaw * 180.0f / M_PI;
}
