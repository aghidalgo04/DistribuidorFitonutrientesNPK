#ifndef AS7265X_H
#define AS7265X_H

#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c.h"

// Definimos la cantidad total de canales para usar en el main
#define AS7265X_TOTAL_CHANNELS    18

/********* Registros Virtuales *********/
#define AS72XX_STATUS_REG         0x00
#define AS72XX_WRITE_REG          0x01
#define AS72XX_READ_REG           0x02

#define AS72XX_TX_VALID           0x02
#define AS72XX_RX_VALID           0x01

/********* Registros de Control *********/
#define AS72XX_CONFIG_REG         0x04
#define AS72XX_INT_T_REG          0x05
#define AS72XX_LED_CONFIG_REG     0x07
#define AS7265X_DEV_SELECT_REG    0x4F

/********* Configuración de LEDs *********/
#define LED_DRIVE_ON              0x08 
#define LED_DRIVE_OFF             0x00

/**
 * Funciones de bajo nivel
 */
void as72xx_write(uint8_t reg, uint8_t value);
uint8_t as72xx_read(uint8_t reg);
uint16_t read_channel(uint8_t baseReg);

/**
 * @brief Ejecuta la secuencia de medición y llena el buffer proporcionado.
 * * @param output_buffer Puntero a un array de uint16_t de tamaño 18.
 * Aquí se guardarán los resultados.
 */
void read_all_18_channels_with_leds(uint16_t *output_buffer);

#endif // AS7265X_H