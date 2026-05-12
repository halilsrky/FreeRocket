#ifndef QUETERNION_H_
#define QUETERNION_H_

#include "bmi088.h"
extern volatile float beta;				// algorithm gain
extern volatile float q0, q1, q2, q3;	// quaternion of sensor frame relative to auxiliary frame

//---------------------------------------------------------------------------------------------------
// Function declarations

void MadgwickAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az, float dt);
void MahonyAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az, float dt);
void Orientation_Update(float gx, float gy, float gz, float ax, float ay, float az, float dt);
void updateQuaternion(float gx, float gy, float gz, float dt);
void UpdateMahonyGains(float ax, float ay, float az);
void quaternionToEuler(void);
void quaternionSet_zero(void);
float quaternionToThetaZ(void);
void getInitialQuaternion();
float quaternionToYaw();
float quaternionToPitch();
float quaternionToRoll();
float quaternionToYawDegree();
float quaternionToPitchDegree();
float quaternionToRollDegree();
float invSqrt(float x);

#endif
