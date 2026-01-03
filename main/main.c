#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "mqtt_client.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "i2c.h"
#include "as7265x.h"
#include "ec_sensor.h"
#include "control_gpio.h"


#define DEFAULT_STA_SSID            "Galaxy S22 97D1"
#define DEFAULT_STA_PASS            "pueb8119"
#define EXAMPLE_ESP_MAXIMUM_RETRY   5

#ifndef CONFIG_ESP_WIFI_CHANNEL
  #define CONFIG_ESP_WIFI_CHANNEL 1
#endif
#ifndef CONFIG_ESP_MAX_STA_CONN
  #define CONFIG_ESP_MAX_STA_CONN 4
#endif

static const char *TAG = "AS7265x_TB";

// Reemplazá con tu servidor ThingsBoard
#define MQTT_URI "mqtt://demo.thingsboard.io"
// Token del dispositivo en ThingsBoard
#define ACCESS_TOKEN "9q6yC6XQRkTgxoUiCWR0"

#define THINGSBOARD_HOST "http://demo.thingsboard.io"
#define TB_TELEMETRY_PATH "/api/v1/" ACCESS_TOKEN "/telemetry"  // POST JSON aquí

// ---------------- CONFIGURACIÓN OTA ----------------
#define OTA_URL "https://raw.githubusercontent.com/KinzCheetah24/ota-firmware/main/SBC25T04.bin"
#define OTA_HOUR   20 // Se le tiene que poner una hora menos
#define OTA_MINUTE 03


static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static int s_retry_num = 0;
static TaskHandle_t s_msg_task = NULL;

static esp_mqtt_client_handle_t client = NULL;

uint16_t sensor_values[AS7265X_TOTAL_CHANNELS];

int sensor_interval_ms = 5000;

int read_interval = 15;
static bool s_should_reconnect = true;

static void go_to_sleep_and_schedule(void) {
    uint64_t sleep_time_us = (uint64_t)read_interval * 1000ULL;

    esp_sleep_enable_timer_wakeup(sleep_time_us);

    ESP_LOGI(TAG, "Deep-sleep por %i s. Política de ahorro: "
                  "Wi-Fi sólo activo al transferir, luego hibernación.",
                  read_interval / 1000);

    esp_deep_sleep_start();
}

// Realiza la actualización de firmware
static void perform_ota_update(void) {
    s_should_reconnect = false; 
    
    ESP_LOGI(TAG, "Iniciando actualización OTA desde: %s", OTA_URL);

    esp_http_client_config_t http_config = {
        .url = OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Actualización OTA completada correctamente. Reiniciando...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Error en OTA: %s", esp_err_to_name(ret));
        // Si falla, permitimos reconectar de nuevo si fuera necesario
        s_should_reconnect = true; 
    }
}

// Inicializa y espera sincronización de hora
static void init_sntp(void) {
    ESP_LOGI(TAG, "Inicializando SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Esperando sincronización NTP (%d/%d)...", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (timeinfo.tm_year >= (2016 - 1900)) {
        ESP_LOGI(TAG, "Hora sincronizada correctamente");
    } else {
        ESP_LOGW(TAG, "No se pudo sincronizar hora por NTP (OTA podría fallar por certs)");
    }
}

void send_mqtt_data(const char *payload) {
    if (client) {
        int msg_id = esp_mqtt_client_publish(client,
                                                "v1/devices/me/telemetry",
                                                payload,
                                                0,    // longitud = calculada automáticamente
                                                1,    // QoS = 1
                                                0);   // retain = 0
        ESP_LOGI(TAG, "Publicado en MQTT msg_id=%d", msg_id);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al broker ThingsBoard");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.username = ACCESS_TOKEN,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, client);
    esp_mqtt_client_start(client);
}

/* ---------- Tarea de sensores ---------- */
static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Iniciando tarea de sensores");
    char payload[600]; 
    ESP_LOGI(TAG, "Leyendo sensores...");
    
    // 1. Leer espectrometría
    read_all_18_channels_with_leds(sensor_values);

    // 2. Leer EC y voltaje
    float voltage;
    float ec_value = ec_sensor_read(&voltage);

    if (ec_value < 0) {
        ESP_LOGW(TAG, "Sensor EC no calibrado o error (V=%.3f)", voltage);
    } else {
        printf("Voltaje: %.3f V | EC: %.3f mS/cm\r\n", voltage, ec_value);
    }

    // 3. Imprimir debug de canales (opcional, consume tiempo)
    // Imprimir NIR (Canales 0-5)
    ESP_LOGI(TAG, "NIR: %u %u %u %u %u %u", 
        sensor_values[0], sensor_values[1], sensor_values[2], 
        sensor_values[3], sensor_values[4], sensor_values[5]);

    // Imprimir VIS (Canales 6-11)
    ESP_LOGI(TAG, "VIS: %u %u %u %u %u %u", 
        sensor_values[6], sensor_values[7], sensor_values[8], 
        sensor_values[9], sensor_values[10], sensor_values[11]);

    // Imprimir UV (Canales 12-17)
    ESP_LOGI(TAG, "UV : %u %u %u %u %u %u", 
        sensor_values[12], sensor_values[13], sensor_values[14], 
        sensor_values[15], sensor_values[16], sensor_values[17]);

    control_gpio_update(sensor_values[12]);

    // 4. Construir JSON
    
    int len = snprintf(payload, sizeof(payload),
        "{"
        "\"A\":%.2f,\"B\":%.2f,\"C\":%.2f,\"D\":%.2f,\"E\":%.2f,\"F\":%.2f,"
        "\"G\":%.2f,\"H\":%.2f,\"I\":%.2f,\"J\":%.2f,\"K\":%.2f,\"L\":%.2f,"
        "\"R\":%.2f,\"S\":%.2f,\"T\":%.2f,\"U\":%.2f,\"V\":%.2f,\"W\":%.2f,"
        "\"Voltage\":%.2f,\"EC_Value\":%.2f"
        "}",
        (float)sensor_values[12], (float)sensor_values[13], (float)sensor_values[14], 
        (float)sensor_values[15], (float)sensor_values[16], (float)sensor_values[17],
        (float)sensor_values[6], (float)sensor_values[7], (float)sensor_values[8], 
        (float)sensor_values[9], (float)sensor_values[10], (float)sensor_values[11],
        (float)sensor_values[0], (float)sensor_values[1], (float)sensor_values[2],
        (float)sensor_values[3], (float)sensor_values[4], (float)sensor_values[5],
        voltage, ec_value
    );

    if (len > 0 && len < sizeof(payload)) {
        send_mqtt_data(payload);
        ESP_LOGI(TAG, "Esperando envío de datos...");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    } else {
        ESP_LOGE(TAG, "Error: Buffer JSON demasiado pequeño");
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Debug de hora
    // ESP_LOGI(TAG, "Hora actual: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (timeinfo.tm_hour == OTA_HOUR && timeinfo.tm_min == OTA_MINUTE) {
        ESP_LOGI(TAG, "Hora programada alcanzada (%02d:%02d). Iniciando OTA...", OTA_HOUR, OTA_MINUTE);
        
        perform_ota_update();
    }

    go_to_sleep_and_schedule();

    vTaskDelay(pdMS_TO_TICKS(sensor_interval_ms));

}

static void stop_sensor_task_if_running(void) {
    if (s_msg_task) {
        ESP_LOGI(TAG, "Parando tarea sensores");
        vTaskDelete(s_msg_task);
        s_msg_task = NULL;
    }
}

/* ---------- Handler de eventos Wi-Fi/IP ---------- */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START -> connect");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_should_reconnect) {
                    ESP_LOGW(TAG, "WIFI desconectado accidentalmente, reintentando...");
                    stop_sensor_task_if_running();
                    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                        esp_wifi_connect();
                        s_retry_num++;
                    } else {
                        ESP_LOGE(TAG, "Fallo al conectar WiFi. A dormir.");
                        go_to_sleep_and_schedule();
                    }
                } else {
                    ESP_LOGI(TAG, "WiFi desconectado intencionalmente (OTA/Sleep).");
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        init_sntp();
        
        mqtt_app_start();

        // 1. Iniciar MQTT y Sensores (si no están corriendo)
        if (!s_msg_task) {
            xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &s_msg_task);
        }
    }
}

/* ---------- Inicialización Wi-Fi (STA) ---------- */
static void wifi_init_apsta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    /* Configuramos la STA con credenciales por defecto */
    wifi_config_t sta_config = { 0 };
    strncpy((char*)sta_config.sta.ssid, DEFAULT_STA_SSID, sizeof(sta_config.sta.ssid) - 1);
    sta_config.sta.ssid[sizeof(sta_config.sta.ssid)-1] = '\0';
    strncpy((char*)sta_config.sta.password, DEFAULT_STA_PASS, sizeof(sta_config.sta.password) - 1);
    sta_config.sta.password[sizeof(sta_config.sta.password)-1] = '\0';

    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (strlen((char*)sta_config.sta.ssid) > 0) {
        ESP_LOGI(TAG, "Trying STA connect to SSID: %s", sta_config.sta.ssid);
        esp_wifi_connect();
    } else {
        ESP_LOGW(TAG, "No STA credentials provided");
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. I2C y Hardware
    i2cm_init();
    ec_sensor_init();

    // 3. Cargar calibración
    if (ec_sensor_load_calib() == ESP_OK && ec_sensor_is_calibrated()) {
        ESP_LOGI(TAG, "Calibración cargada. Iniciando medición.");
    } else {
        ESP_LOGW(TAG, "Sin calibración válida. Entrando en modo calibración interactiva...");
        ec_sensor_run_interactive_calibration();
    }

    control_gpio_init();

    // 4. Iniciar WiFi (Esto arrancará las tareas MQTT/HTTP/Sensor cuando conecte)
    wifi_init_apsta();
}
