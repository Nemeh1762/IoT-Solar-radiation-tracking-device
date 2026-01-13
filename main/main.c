/* ======================= Libraries ======================= */
// Standard C library for printing and debugging
#include <stdio.h>

// FreeRTOS libraries for task management and delays
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF error handling and logging
#include "esp_err.h"
#include "esp_log.h"

// LEDC driver used to generate PWM signals for servo control
#include "driver/ledc.h"

// ADC driver for reading analog values from LDR sensors
#include "esp_adc/adc_oneshot.h"

// WiFi and networking libraries for IoT communication
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"


/* ======================= WiFi & Cloud Configuration ======================= */
// WiFi credentials
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

// ThingSpeak API key for cloud data transmission
#define THINGSPEAK_API_KEY "JA0W2AWRBR5KH8ZF"


/* ======================= WiFi Initialization ======================= */
// Initializes WiFi, connects ESP32 to the network, and enables internet access
static void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}


/* ======================= ThingSpeak Data Transmission ======================= */
// Sends LDR readings, sun direction, and servo angle to ThingSpeak cloud platform
static void send_to_thingspeak(int east, int west, int sun, int angle) {

    char url[256];
    snprintf(url, sizeof(url),
        "http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d",
        THINGSPEAK_API_KEY,
        east,
        west,
        sun,
        angle
    );

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("\nThingSpeak update sent successfully\n");
    } else {
        printf("\nThingSpeak send failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

/* ======================= Servo Configuration ======================= */
// GPIO and PWM configuration parameters for servo motor control
#define SERVO_PIN            27
#define SERVO_TIMER          LEDC_TIMER_0
#define SERVO_MODE           LEDC_LOW_SPEED_MODE
#define SERVO_CHANNEL        LEDC_CHANNEL_0
#define SERVO_DUTY_RES       LEDC_TIMER_13_BIT
#define SERVO_FREQ_HZ        50

#define SERVO_MIN_US         500
#define SERVO_MAX_US         2400
#define SERVO_PERIOD_US      20000
#define SERVO_MAX_DUTY       8191

/* ======================= SUN LOGIC ======================= */
// Logical representation of sun position


#define SUN_EAST        0
#define SUN_OVERHEAD    1
#define SUN_WEST        2

#define LDR_THRESHOLD   150   // Noise filtering threshold

/* ======================= HELPER FUNCTIONS ======================= */
static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ======================= SERVO CONTROL ======================= */
static void set_servo_angle(int angle) {
    angle = clamp_int(angle, 0, 180);

    int pulse_width_us =
        SERVO_MIN_US + (angle * (SERVO_MAX_US - SERVO_MIN_US)) / 180;

    int duty = (pulse_width_us * SERVO_MAX_DUTY) / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL));
}

/* ======================= ADC (LDR) CONFIG ======================= */
/*
  GPIO34 -> LDR1 (East)
  GPIO32 -> LDR2 (West)
*/
#define LDR_EAST_CH    ADC_CHANNEL_5  // GPIO34
#define LDR_WEST_CH    ADC_CHANNEL_4   // GPIO32

static adc_oneshot_unit_handle_t adc1_handle = NULL;

static void adc_init_oneshot(void) {

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR_EAST_CH, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR_WEST_CH, &chan_cfg));
}

static void read_ldr_values(int *east, int *west) {
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR_EAST_CH, east));
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR_WEST_CH, west));
}

/* ======================= FR2: SUN DIRECTION ======================= */
static int detect_sun_direction(int east, int west) {

    int horizontal_diff = east - west;

    if (horizontal_diff > LDR_THRESHOLD) {
        return SUN_EAST;
    }
    if (horizontal_diff < -LDR_THRESHOLD) {
        return SUN_WEST;
    }
 

    return SUN_OVERHEAD; // Default safe position
}

/* ======================= FR3: TILT CALCULATION ======================= */
static int compute_tilt_angle(int sun_direction) {

    switch (sun_direction) {
        case SUN_EAST:
            return 30;     // Morning
        case SUN_WEST:
            return 150;    // Afternoon
        case SUN_OVERHEAD:
        default:
            return 90;     // Midday
    }
}

/* ======================= MAIN APPLICATION ======================= */
void app_main(void) {

    /* Servo timer */
    ledc_timer_config_t servo_timer = {
        .speed_mode       = SERVO_MODE,
        .duty_resolution  = SERVO_DUTY_RES,
        .timer_num        = SERVO_TIMER,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&servo_timer));

    /* Servo channel */
    ledc_channel_config_t servo_channel = {
        .gpio_num   = SERVO_PIN,
        .speed_mode = SERVO_MODE,
        .channel    = SERVO_CHANNEL,
        .timer_sel  = SERVO_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&servo_channel));

    /* ADC init */
    adc_init_oneshot();

    printf( "Smart Canopy System Started\n");
    fflush(stdout);
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(5000)); // wait WiFi connect



    while (1) {
        printf("Loop running...\n");
        fflush(stdout);

        int ldr_east, ldr_west;

        read_ldr_values(&ldr_east, &ldr_west);

        int sun_dir = detect_sun_direction(
            ldr_east, ldr_west
        );

        int tilt_angle = compute_tilt_angle(sun_dir);

        set_servo_angle(tilt_angle);

        printf(
            "E=%d W=%d  | SunDir=%d | Tilt=%d",
            ldr_east, ldr_west, 
            sun_dir, tilt_angle
        );
        fflush(stdout);
        
        send_to_thingspeak(
            ldr_east,
            ldr_west,
            sun_dir,
            tilt_angle
        );

        vTaskDelay(pdMS_TO_TICKS(15000)); // Meets NFR2
    }
}