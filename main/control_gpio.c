/*
 * control_gpio.c
 */

#include "control_gpio.h"
#include "driver/gpio.h"

// Función auxiliar para configurar un solo pin
static void configurar_pin_salida(int pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0); // Empezar apagado
}

void control_gpio_init(void) {
    // Inicializamos los 3 pines
    configurar_pin_salida(GPIO_PIN_A);
    configurar_pin_salida(GPIO_PIN_B);
    configurar_pin_salida(GPIO_PIN_C);
}

void control_gpio_update(float valor) {
    int estado_a = 0;
    int estado_b = 0;
    int estado_c = 0;

    if (valor < 500.0f) {
        // CASO 1: Valor bajo (menor a 500)
        estado_a = 1;
        estado_b = 1;
        estado_c = 1;
    } 
    else if (valor >= 500.0f && valor < 550.0f) {
        // CASO 2: Valor medio (entre 500 y 550)
        estado_a = 1;
        estado_b = 0;
        estado_c = 0;
    } 
    else if (valor >= 550.0f && valor < 600.0f) {
        // CASO 3: Valor medio (entre 550 y 600)
        estado_a = 0;
        estado_b = 1;
        estado_c = 0;
    } else if (valor >= 600.0f && valor < 700.0f) {
        // CASO 4: Valor medio (entre 600 y 700)
        estado_a = 0;
        estado_b = 0;
        estado_c = 1;
    } else {
        // CASO 5: Valor medio (mayor a 600)
        estado_a = 0;
        estado_b = 0;
        estado_c = 0;
    }
    gpio_set_level(GPIO_PIN_A, estado_a);
    gpio_set_level(GPIO_PIN_B, estado_b);
    gpio_set_level(GPIO_PIN_C, estado_c);
}