#include "i2c.h"
#include "esp_log.h"

static const char *TAG = "i2cm";

/* Inicializa el bus I2C como master */
void i2cm_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2CM_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2CM_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2CM_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2CM_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2CM_PORT, conf.mode, 0, 0, 0));

    ESP_LOGI(TAG, "I2C master inicializado (SDA=%d, SCL=%d)", I2CM_SDA_PIN, I2CM_SCL_PIN);
}

/*  Escribe en un registro físico del AS7265x */
void i2cm_write(uint8_t reg, uint8_t data)
{
    uint8_t buffer[2] = { reg, data };

    esp_err_t ret = i2c_master_write_to_device(
        I2CM_PORT,
        AS7265X_I2C_ADDR,
        buffer,
        sizeof(buffer),
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error en escritura I2C: reg=0x%02X", reg);
    }
}

/* Lee un registro físico del AS7265x */
uint8_t i2cm_read(uint8_t reg)
{
    uint8_t data = 0;

    esp_err_t ret = i2c_master_write_read_device(
        I2CM_PORT,
        AS7265X_I2C_ADDR,
        &reg,
        1,
        &data,
        1,
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error en lectura I2C: reg=0x%02X", reg);
    }

    return data;
}