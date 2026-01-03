/*
 * control_gpio.h
 * Librería para controlar 3 salidas (A, B, C) según rangos de valores.
 */

#ifndef CONTROL_GPIO_H
#define CONTROL_GPIO_H

// --- CONFIGURACIÓN DE PINES (Cámbialos por los que uses) ---
#define GPIO_PIN_A  17
#define GPIO_PIN_B  18
#define GPIO_PIN_C  19

/**
 * @brief Configura los pines A, B y C como salidas digitales.
 */
void control_gpio_init(void);

/**
 * @brief Enciende o apaga A, B y C dependiendo del valor recibido.
 * * @param valor El número (float) que usaremos para decidir qué encender.
 */
void control_gpio_update(float valor);

#endif // CONTROL_GPIO_H