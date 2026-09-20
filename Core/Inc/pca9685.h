#pragma once

#include "main.h"

HAL_StatusTypeDef PCA9685_Init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef PCA9685_SetAllPWM(I2C_HandleTypeDef *hi2c, int16_t pwm[16]);
