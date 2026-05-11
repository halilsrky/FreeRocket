/*
 * quaternion.h
 *
 *  Created on: May 9, 2025
 *      Author: Halil
 */

#ifndef INC_QUATERNION_H_
#define INC_QUATERNION_H_


void ekf_init(void);
void ekf_predict(float gx, float gy, float gz, float dt);
void ekf_update(float ax, float ay, float az);
float* ekf_getQuaternion(void);
float ekf_getTheta(void);
float quaternionToYaw1();
float quaternionToPitch1();
float quaternionToRoll1();
float quaternionToYawDegree1();

#endif /* INC_QUATERNION_H_ */
