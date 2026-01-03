#include "as7265x.h"
#include "esp_log.h"

#define TAG "AS7265x_DRIVER"

/********* Funciones de Bajo Nivel *********/

void as72xx_write(uint8_t reg, uint8_t value)
{
    while (i2cm_read(AS72XX_STATUS_REG) & AS72XX_TX_VALID) {
        // Espera activa (puedes añadir vTaskDelay(1) si lo prefieres)
    }
    i2cm_write(AS72XX_WRITE_REG, reg | 0x80);

    while (i2cm_read(AS72XX_STATUS_REG) & AS72XX_TX_VALID);
    i2cm_write(AS72XX_WRITE_REG, value);
}

uint8_t as72xx_read(uint8_t reg)
{
    while (i2cm_read(AS72XX_STATUS_REG) & AS72XX_TX_VALID);
    i2cm_write(AS72XX_WRITE_REG, reg);

    while (!(i2cm_read(AS72XX_STATUS_REG) & AS72XX_RX_VALID));
    return i2cm_read(AS72XX_READ_REG);
}

uint16_t read_channel(uint8_t baseReg)
{
    uint8_t high = as72xx_read(baseReg);
    uint8_t low  = as72xx_read(baseReg + 1);
    return ((uint16_t)high << 8) | low;
}

/********* Función Principal Modificada *********/

// Ahora recibe un puntero donde guardar los datos
void read_all_18_channels_with_leds(uint16_t *output_buffer)
{
    static const uint8_t banks[3] = {0x00, 0x01, 0x02};
    int idx = 0;

    for (int b = 0; b < 3; b++)
    {
        // 1. Seleccionar Sensor (Banco)
        as72xx_write(AS7265X_DEV_SELECT_REG, banks[b]);

        // 2. Configurar Integración
        as72xx_write(AS72XX_INT_T_REG, 50);

        // 3. Encender LED
        as72xx_write(AS72XX_LED_CONFIG_REG, LED_DRIVE_ON);

        // 4. Iniciar Medición (Mode 0: One-Shot) y Ganancia
        as72xx_write(AS72XX_CONFIG_REG, 0b00100000); 

        // Esperar DATA_RDY (Bit 1 del registro 4)
        int timeout = 0;
        while (!(as72xx_read(AS72XX_CONFIG_REG) & 0x02)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout++;
            if(timeout > 50) break; // Timeout ~500ms
        }

        // 5. Apagar LED
        as72xx_write(AS72XX_LED_CONFIG_REG, LED_DRIVE_OFF);

        // 6. Leer y GUARDAR en el buffer del usuario
        for (int ch = 0; ch < 6; ch++)
        {
            // Escribimos directamente en la memoria del array del main
            if (output_buffer != NULL) {
                output_buffer[idx] = read_channel(0x08 + (ch * 2));
            }
            idx++;
        }
    }
}