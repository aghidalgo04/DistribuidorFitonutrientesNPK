/*
 * ec_sensor.h
 * Cabecera para el manejo del sensor de Electroconductividad
 */

#ifndef EC_SENSOR_H
#define EC_SENSOR_H

#include <stdbool.h>
#include "esp_err.h"

// Estructura para almacenar los datos de calibración
typedef struct {
    float a;     // pendiente de la recta
    float b;     // offset
    bool  valid; // true si está calibrado
} ec_calib_t;

/**
 * @brief Inicializa el ADC y la UART necesaria para el sensor/calibración.
 */
void ec_sensor_init(void);

/**
 * @brief Carga la configuración de calibración desde NVS.
 * * @return esp_err_t ESP_OK si se cargó correctamente, o código de error.
 */
esp_err_t ec_sensor_load_calib(void);

/**
 * @brief Guarda la configuración actual en NVS.
 * * @return esp_err_t ESP_OK si se guardó correctamente.
 */
esp_err_t ec_sensor_save_calib(void);

/**
 * @brief Comprueba si el sensor tiene una calibración válida cargada.
 * * @return true si es válida.
 */
bool ec_sensor_is_calibrated(void);

/**
 * @brief Lee el voltaje actual y calcula la EC basada en la calibración actual.
 * * @param voltage_out Puntero para guardar el voltaje leído (opcional, puede ser NULL).
 * @return float Valor de Electroconductividad en mS/cm (-1.0 si no está calibrado).
 */
float ec_sensor_read(float *voltage_out);

/**
 * @brief Ejecuta el proceso interactivo de calibración por consola (Bloqueante).
 */
void ec_sensor_run_interactive_calibration(void);

#endif // EC_SENSOR_H