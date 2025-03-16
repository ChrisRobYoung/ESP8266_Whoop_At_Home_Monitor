/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)
  
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "mdns.h"

#include "whoop_data.h"
#include "whoop_client.h"
#include "gpio_manager.h"
#include "i2c_led.h"
#include "whoop_esp_server.h"

//Timer information
TimerHandle_t task_timer_handle;
static int g_last_button_state = 0;

#define MDNS_HOSTNAME "esp8266-whoop-api"

#define GOT_IPV4_BIT BIT(0)
static const char *TAG="WHOOP APP";

static void initialise_mdns(void)
{
    char* hostname = MDNS_HOSTNAME;
    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set("ESP8266 mDNS Instance") );

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board","esp8266"},
        {"u","user"},
        {"p","password"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("ESP8266-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );
}

static EventGroupHandle_t s_connect_event_group;
static ip4_addr_t s_ip_addr;
static char s_connection_name[32] = CONFIG_WIFI_SSID;
static char s_connection_passwd[32] = CONFIG_WIFI_PASSWORD;

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }
    ESP_ERROR_CHECK(esp_wifi_connect());
}


static void on_got_ip(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xEventGroupSetBits(s_connect_event_group, GOT_IPV4_BIT);
}


static void start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = { 0 };

    strncpy((char *)&wifi_config.sta.ssid, s_connection_name, 32);
    strncpy((char *)&wifi_config.sta.password, s_connection_passwd, 32);

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_connect());
}

esp_err_t connect_to_wifi(void)
{
    if (s_connect_event_group != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_connect_event_group = xEventGroupCreate();
    start();
    xEventGroupWaitBits(s_connect_event_group, GOT_IPV4_BIT, true, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to %s", s_connection_name);
    ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
    return ESP_OK;
}

static void recovery_to_led(float recovery_score)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if(recovery_score > 67.0)
    {
        g = 255;
    }
    else if (recovery_score > 34.0)
    {
        r = 255;
        g = 255;
    }
    else
    {
        r = 255;
    }
    set_rgb_led_value(r, g, b);
}

static void strain_to_led(float strain)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if(strain < 10.0)
    {
        g = 255;
    }
    else if (strain < 14.0)
    {
        r = 255;
        g = 255;
    }
    else if (strain < 19.0)
    {
        r = 255;
        g = 127;
    }
    else
    {
        r = 255;
    }
    set_rgb_led_value(r, g, b);
}

static void sleep_percentage_to_led(float sleep_percentage)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if(sleep_percentage > 90.0)
    {
        g = 255;
    }
    else if (sleep_percentage > 70.0)
    {
        r = 255;
        g = 255;
    }
    else
    {
        r = 255;
    }
    set_rgb_led_value(r, g, b);
}

typedef enum current_data_selection
{
    DATA_SELECTION_RECOVERY,
    DATA_SELECTION_SLEEP,
    DATA_SELECTION_CYCLE,
    DATA_SELECTION_WORKOUT
} current_data_selection_t;

current_data_selection_t g_data_selection = DATA_SELECTION_WORKOUT;
char *data_selection_text[] = {"Selected recovery", "Selected Sleep", "Selected Cycle", "Selected Workout"};
 void vTimerCallback( TimerHandle_t xTimer )
 {
    int button_state = 0;
    float data_float = 0.0;
    char data_str[17];
    whoop_data_handle_t handle = NULL;
    get_touch_button_state(&button_state);
    int data_selection = g_data_selection;
    if(button_state != g_last_button_state)
    {
        i2c_lcd_1602_clear();
        i2c_lcd_1602_home();
        g_last_button_state = button_state;
        for(int i =1; i <= 4; i++)
        {
            data_selection = (g_data_selection + i) % 4;
            switch(data_selection)
            {
                case DATA_SELECTION_RECOVERY:
                    get_whoop_recovery_handle_by_id(0, &handle);
                    break;
                case DATA_SELECTION_SLEEP:
                    get_whoop_sleep_handle_by_id(0, &handle);
                    break;
                case DATA_SELECTION_CYCLE:
                    get_whoop_cycle_handle_by_id(0, &handle);
                    break;
                case DATA_SELECTION_WORKOUT:
                    get_whoop_workout_handle_by_id(0, &handle);
                    break;
            }
            if(handle) break;
        }
        g_data_selection = data_selection;
        ESP_LOGI(TAG, data_selection_text[g_data_selection]);
        
        if(!handle) return;
        switch(data_selection)
        {
            case DATA_SELECTION_RECOVERY:
                get_whoop_data(handle, WHOOP_DATA_OPT_RECOVERY_RECOVERY_SCORE, &data_float);
                recovery_to_led(data_float);
                i2c_lcd_1602_print("Recovery", strlen("Recovery"));
                i2c_lcd_1602_setCursor(0,1);
                sprintf(data_str, "Score: %0.2f",data_float);
                i2c_lcd_1602_print(data_str, strlen(data_str));
                break;
            case DATA_SELECTION_SLEEP:
                get_whoop_data(handle, WHOOP_DATA_OPT_SLEEP_SLEEP_PERFORMANCE_PERCENTAGE, &data_float);
                sleep_percentage_to_led(data_float);
                i2c_lcd_1602_print("Sleep", strlen("Sleep"));
                i2c_lcd_1602_setCursor(0,1);
                sprintf(data_str, "Perf: %0.2f%%",data_float);
                i2c_lcd_1602_print(data_str, strlen(data_str));
                break;
            case DATA_SELECTION_CYCLE:
                get_whoop_data(handle, WHOOP_DATA_OPT_CYCLE_STRAIN, &data_float);
                strain_to_led(data_float);
                i2c_lcd_1602_print("Cycle", strlen("Cycle"));
                i2c_lcd_1602_setCursor(0,1);
                sprintf(data_str, "Strain: %0.2f",data_float);
                i2c_lcd_1602_print(data_str, strlen(data_str));
                break;
            case DATA_SELECTION_WORKOUT:
                get_whoop_data(handle, WHOOP_DATA_OPT_WORKOUT_STRAIN, &data_float);
                strain_to_led(data_float);
                i2c_lcd_1602_print("Workout", strlen("Workout"));
                i2c_lcd_1602_setCursor(0,1);
                sprintf(data_str, "Strain: %0.2f",data_float);
                i2c_lcd_1602_print(data_str, strlen(data_str));
                break;
        }
    }
 }


void app_main()
{
    //ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();

    ESP_ERROR_CHECK(connect_to_wifi());
    init_whoop_data();
    
    
    initialize_gpio();
    get_touch_button_state(&g_last_button_state);

    i2c_lcd_1602_init();
    i2c_lcd_1602_print("No Data.", 8);
    
    init_whoop_server();
    init_whoop_tls_client();
    //Start task loop
    task_timer_handle = xTimerCreate("Timer", pdMS_TO_TICKS(100) , pdTRUE,( void * ) 0,vTimerCallback);
    xTimerStart( task_timer_handle, 0 );
}
