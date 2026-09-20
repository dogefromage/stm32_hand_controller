#include "pca9685.h"

#include <stdint.h>

#include "stm32f4xx_hal_def.h"

#define PCA9685_ADDRESS 0x80
// Datasheet link --> https://cdn-shop.adafruit.com/datasheets/PCA9685.pdf
#define PCA9685_MODE1 0x00           // as in the datasheet page no 10/52
#define PCA9685_MODE2 0x01           // as in the datasheet page no 10/52
#define PCA9685_PRE_SCALE 0xFE       // as in the datasheet page no 13/52
#define PCA9685_LED0_ON_L 0x6        // as in the datasheet page no 10/52
#define PCA9685_MODE1_SLEEP_BIT 4    // as in the datasheet page no 14/52
#define PCA9685_MODE1_AI_BIT 5       // as in the datasheet page no 14/52
#define PCA9685_MODE1_RESTART_BIT 7  // as in the datasheet page no 14/52

HAL_StatusTypeDef PCA9685_SetBit(I2C_HandleTypeDef* hi2c, uint8_t reg, uint8_t bit, uint8_t val) {
    uint8_t buffer;
    // Read all 8 bits and set only one bit to 0/1 and write all 8 bits back
    HAL_StatusTypeDef err;
    if ((err = HAL_I2C_Mem_Read(hi2c, PCA9685_ADDRESS, reg, 1, &buffer, 1, 10)) != HAL_OK) {
        return err;
    }
    if (val) {
        buffer |= (1 << bit);
    } else {
        buffer &= ~(1 << bit);
    }
    if ((err = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDRESS, reg, 1, &buffer, 1, 10)) != HAL_OK) {
        return err;
    }
    HAL_Delay(1);
    return HAL_OK;
}

HAL_StatusTypeDef PCA9685_Init(I2C_HandleTypeDef* hi2c) {
    HAL_StatusTypeDef err;

    /*
     * PCA9685 initialization
     *
     * Sequence:
     *
     *   1. Enter SLEEP
     *   2. Enable Auto-Increment
     *   3. Configure output mode
     *   4. Set PRE_SCALE while sleeping
     *   5. Wake oscillator
     *   6. Wait for oscillator startup
     *
     * RESTART is NOT used here.
     * RESTART is for restarting PWM after the device has
     * previously been put to sleep while running.
     */

    // ------------------------------------------------------------
    // 1. Put PCA9685 into SLEEP
    //
    // PRE_SCALE may only be written while SLEEP = 1.
    // ------------------------------------------------------------

    if ((err = PCA9685_SetBit(hi2c, PCA9685_MODE1, PCA9685_MODE1_SLEEP_BIT, 1)) != HAL_OK) {
        return err;
    }

    // ------------------------------------------------------------
    // 2. Enable Auto-Increment
    //
    // This allows us to write all 16 PWM channels in one
    // I2C transaction, starting at LED0_ON_L.
    // ------------------------------------------------------------

    if ((err = PCA9685_SetBit(hi2c, PCA9685_MODE1, PCA9685_MODE1_AI_BIT, 1)) != HAL_OK) {
        return err;
    }

    // ------------------------------------------------------------
    // 3. Configure MODE2
    //
    // OUTDRV = 1:
    //   Totem-pole output driver
    //
    // OCH = 0:
    //   Outputs change on I2C STOP
    //
    // These are the normal/default settings for ordinary PWM use.
    // ------------------------------------------------------------

    uint8_t mode2 = 0x04;  // OUTDRV = 1

    if ((err = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDRESS, PCA9685_MODE2, I2C_MEMADD_SIZE_8BIT, &mode2,
                                 1, 10)) != HAL_OK) {
        return err;
    }

    // ------------------------------------------------------------
    // 4. Set PWM prescaler
    //
    // For the nominal 25 MHz oscillator:
    //
    // PRE_SCALE = 121 -> approximately 50 Hz
    //
    // IMPORTANT:
    // SLEEP is still 1 here.
    // ------------------------------------------------------------

    uint8_t prescale = 121;

    if ((err = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDRESS, PCA9685_PRE_SCALE, I2C_MEMADD_SIZE_8BIT,
                                 &prescale, 1, 10)) != HAL_OK) {
        return err;
    }

    // ------------------------------------------------------------
    // 5. Wake PCA9685
    //
    // Keep AI = 1 while clearing SLEEP.
    //
    // We deliberately do NOT set RESTART here.
    // ------------------------------------------------------------

    if ((err = PCA9685_SetBit(hi2c, PCA9685_MODE1, PCA9685_MODE1_SLEEP_BIT, 0)) != HAL_OK) {
        return err;
    }

    // ------------------------------------------------------------
    // 6. Allow oscillator to stabilize
    //
    // Datasheet specifies at least 500 us.
    // ------------------------------------------------------------

    HAL_Delay(1);

    // ------------------------------------------------------------
    // Initialization complete.
    //
    // The PCA9685 is now running at approximately 50 Hz,
    // Auto-Increment is enabled, and PWM registers can be
    // updated normally.
    // ------------------------------------------------------------

    return HAL_OK;
}

// HAL_StatusTypeDef PCA9685_SetPWM(I2C_HandleTypeDef *hi2c, uint8_t Channel, uint16_t OnTime,
// uint16_t OffTime)
// {
//   HAL_StatusTypeDef err;
//   uint8_t registerAddress;
//   uint8_t pwm[4];
//   registerAddress = PCA9685_LED0_ON_L + (4 * Channel);
//   // See example 1 in the datasheet page no 18/52
//   pwm[0] = OnTime & 0xFF;
//   pwm[1] = OnTime>>8;
//   pwm[2] = OffTime & 0xFF;
//   pwm[3] = OffTime>>8;
//   if ((err = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDRESS, registerAddress, 1, pwm, 4, 10)) != HAL_OK)
//   {
//     return err;
//   }
//   return HAL_OK;
// }

HAL_StatusTypeDef PCA9685_SetAllPWM(I2C_HandleTypeDef* hi2c, int16_t pwm[16]) {
    uint8_t data[64];

    for (uint8_t ch = 0; ch < 16; ch++) {
        int16_t on = 0;
        int16_t off = pwm[ch];
        if (off < 0) {
            off = 0;
        }
        if (off >= 4096) {
            off = 4095;
        }

        data[ch * 4 + 0] = on & 0xFF;
        data[ch * 4 + 1] = on >> 8;
        data[ch * 4 + 2] = off & 0xFF;
        data[ch * 4 + 3] = off >> 8;
    }

    return HAL_I2C_Mem_Write(hi2c, PCA9685_ADDRESS,
                             PCA9685_LED0_ON_L,  // 0x06
                             I2C_MEMADD_SIZE_8BIT, data, 64, 10);
}
