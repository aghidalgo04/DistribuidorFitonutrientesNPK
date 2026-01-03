/*
 * ec_sensor.c
 * Implementación del sensor de Electroconductividad
 */

#include "ec_sensor.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "EC_SENSOR";

// ---------- CONFIGURACIÓN INTERNA ----------

#define EC_ADC_CHANNEL   ADC1_CHANNEL_6  // GPIO34
#define EC_SAMPLES       32

// Configuración UART
#define EC_UART_PORT     UART_NUM_0
#define EC_UART_BUF_SIZE 1024

// Valores de calibración estándar (mS/cm)
#define EC_LOW_STD       1.413f 
#define EC_HIGH_STD      12.88f 

// Variable estática local para mantener el estado de la calibración
static ec_calib_t s_ec_calib = { .a = 1.0f, .b = 0.0f, .valid = false };

// ---------- FUNCIONES PRIVADAS (Auxiliares) ----------

// Lectura de byte no bloqueante
static int ec_uart_getchar_nonblock(void)
{
    uint8_t data;
    int len = uart_read_bytes(EC_UART_PORT, &data, 1, 20 / portTICK_PERIOD_MS);
    if (len > 0) {
        return (int)data;
    }
    return -1;
}

// Lectura cruda de voltaje
static float ec_read_voltage_internal(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < EC_SAMPLES; i++) {
        int raw = adc1_get_raw(EC_ADC_CHANNEL);
        sum += raw;
    }
    float raw_avg = (float)sum / (float)EC_SAMPLES;
    // ADC 12 bits -> 3.3V
    return (raw_avg / 4095.0f) * 3.3f;
}

// ---------- FUNCIONES PÚBLICAS ----------

void ec_sensor_init(void)
{
    // 1. Configurar ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(EC_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // 2. Configurar UART (para calibración interactiva)
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(EC_UART_PORT, EC_UART_BUF_SIZE, EC_UART_BUF_SIZE, 0, NULL, 0);
    uart_param_config(EC_UART_PORT, &uart_config);
    uart_set_pin(EC_UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
                 
    ESP_LOGI(TAG, "Sensor EC inicializado (ADC + UART).");
}

esp_err_t ec_sensor_save_calib(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error abriendo NVS (write): %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, "ec_calib", &s_ec_calib, sizeof(ec_calib_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t ec_sensor_load_calib(void)
{
    nvs_handle_t nvs;
    size_t size = sizeof(ec_calib_t);
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo abrir NVS para leer calibración (puede que sea la primera vez).");
        s_ec_calib.valid = false;
        return err;
    }

    err = nvs_get_blob(nvs, "ec_calib", &s_ec_calib, &size);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Calibración cargada: a=%.4f, b=%.4f, valid=%d", 
                 s_ec_calib.a, s_ec_calib.b, s_ec_calib.valid);
    } else {
        s_ec_calib.valid = false;
    }
    return err;
}

bool ec_sensor_is_calibrated(void)
{
    return s_ec_calib.valid;
}

float ec_sensor_read(float *voltage_out)
{
    float v = ec_read_voltage_internal();
    
    if (voltage_out != NULL) {
        *voltage_out = v;
    }

    if (!s_ec_calib.valid) {
        return -1.0f; 
    }
    
    // y = ax + b
    return (s_ec_calib.a * v) + s_ec_calib.b;
}

void ec_sensor_run_interactive_calibration(void)
{
    printf("\r\n=== MODO CALIBRACION (2 puntos) ===\r\n");
    printf("1. Pon la sonda en 1.413 mS/cm y pulsa '1' + Enter\r\n");

    int ch;
    float V1 = 0.0f;
    float V2 = 0.0f;

    // Esperar punto 1
    while (1) {
        ch = ec_uart_getchar_nonblock();
        if (ch == '1') {
            V1 = ec_read_voltage_internal();
            printf(" -> Leido V1 = %.3f V\r\n", V1);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("\r\n2. Pon la sonda en 12.88 mS/cm y pulsa '2' + Enter\r\n");

    // Esperar punto 2
    while (1) {
        ch = ec_uart_getchar_nonblock();
        if (ch == '2') {
            V2 = ec_read_voltage_internal();
            printf(" -> Leido V2 = %.3f V\r\n", V2);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Calcular recta
    if (V2 - V1 == 0) {
        printf("Error: Voltajes iguales, no se puede calibrar.\r\n");
        return;
    }

    float a = (EC_HIGH_STD - EC_LOW_STD) / (V2 - V1);
    float b = EC_LOW_STD - (a * V1);

    s_ec_calib.a = a;
    s_ec_calib.b = b;
    s_ec_calib.valid = true;

    printf("\r\nCalibrado: a=%.4f, b=%.4f. Guardando...\r\n", a, b);
    
    if (ec_sensor_save_calib() == ESP_OK) {
        printf("Guardado exitoso en NVS.\r\n\r\n");
    } else {
        printf("Error al guardar en NVS.\r\n\r\n");
    }
}