#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include <stdint.h>
#include "driver/i2c.h"

// ===== CONFIGURACIÓN DEL BUS I2C =====

#define I2CM_SDA_PIN           21
#define I2CM_SCL_PIN           22
#define I2CM_PORT              I2C_NUM_0
#define I2CM_FREQ_HZ           100000      // 100 kHz

// Dirección del AS7265x (modo I2C virtual register)
#define AS7265X_I2C_ADDR       0x49

// ===== PROTOTIPOS =====

/**
 * @brief Inicializa el bus I2C como master
 */
void i2cm_init(void);

/**
 * @brief Escribe un byte en un registro físico del AS7265x
 *
 * @param reg Dirección de registro
 * @param data Dato a escribir
 */
void i2cm_write(uint8_t reg, uint8_t data);

/**
 * @brief Lee un byte de un registro físico del AS7265x
 *
 * @param reg Dirección del registro
 * @return uint8_t Byte leído
 */
uint8_t i2cm_read(uint8_t reg);

#endif