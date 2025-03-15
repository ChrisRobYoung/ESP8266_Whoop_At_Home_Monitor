/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)
  
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <esp_http_server.h>

#include "whoop_data.h"
#include "whoop_client.h"
#include "gpio_manager.h"

//Timer information
TimerHandle_t task_timer_handle;
static int g_last_button_state = 0;

#define MDNS_HOSTNAME "esp8266-whoop-api"
#define AUTH_ENDPOINT_CBK "/authenticate/callback"
const char * REDIRECT_URI_ESP8266_WHOOP = "https://api.prod.whoop.com/oauth/oauth2/auth"
                "?client_id=02d55f66-a1a6-4970-8e8b-dd31837233ee&response_type=code&"
                "redirect_uri=http://localhost:3100&"
                "state=aaaaaaaa&"
                "scope=offline%20read:recovery%20read:cycles%20read:workout%20read:sleep";

#define GOT_IPV4_BIT BIT(0)
static const char *TAG="APP";

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
        {"board","esp32"},
        {"u","user"},
        {"p","password"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );
    //add another TXT item
    ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/foobar") );
    //change TXT item value
    ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "u", "admin") );
    //free(hostname);
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

static void stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));


    ESP_ERROR_CHECK(esp_wifi_deinit());
}

esp_err_t example_connect(void)
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

esp_err_t example_disconnect(void)
{
    if (s_connect_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    vEventGroupDelete(s_connect_event_group);
    s_connect_event_group = NULL;
    stop();
    ESP_LOGI(TAG, "Disconnected from %s", s_connection_name);
    s_connection_name[0] = '\0';
    return ESP_OK;
}

esp_err_t auth_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Location", REDIRECT_URI_ESP8266_WHOOP);

    /* Send response with custom headers and body set as the
        * string passed in user context*/
    //const char* resp_str = "Redirecting";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t auth = {
    .uri       = "/authenticate",
    .method    = HTTP_GET,
    .handler   = auth_get_handler,
    .user_ctx  = NULL
};

esp_err_t auth_cbk_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    /* Set some custom headers */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[254];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "code", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => param=%s", param);
                whoop_get_token(param, TOKEN_REQUEST_TYPE_AUTH_CODE);
                //whoop_get_data(WHOOP_API_REQUEST_TYPE_CYCLE);
                //whoop_get_data(WHOOP_API_REQUEST_TYPE_RECOVERY);
                //whoop_get_data(WHOOP_API_REQUEST_TYPE_SLEEP);
                //whoop_get_data(WHOOP_API_REQUEST_TYPE_WORKOUT);
            }
            if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => state=%s", param);
            }
            if (httpd_query_key_value(buf, "scope", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => scope=%s", param);
            }
        }
        free(buf);
    }

    const char *req_response = "Recieved authentication code.";
    httpd_resp_send(req, req_response, strlen(req_response));

    return ESP_OK;
}

httpd_uri_t auth_cbk = {
    .uri       = "/authenticate/callback",
    .method    = HTTP_GET,
    .handler   = auth_cbk_get_handler,
    .user_ctx  = NULL
};

esp_err_t whoop_sleep_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    whoop_get_data(WHOOP_API_REQUEST_TYPE_SLEEP);
    httpd_resp_set_hdr(req, "User", "ESP8266");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t whoop_sleep_cbk = {
    .uri       = "/whoop/sleep",
    .method    = HTTP_GET,
    .handler   = whoop_sleep_get_handler,
    .user_ctx  = NULL
};

esp_err_t whoop_recover_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    whoop_get_data(WHOOP_API_REQUEST_TYPE_RECOVERY);
    httpd_resp_set_hdr(req, "User", "ESP8266");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t whoop_recover_cbk = {
    .uri       = "/whoop/recovery",
    .method    = HTTP_GET,
    .handler   = whoop_recover_get_handler,
    .user_ctx  = NULL
};

esp_err_t whoop_workout_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    whoop_get_data(WHOOP_API_REQUEST_TYPE_WORKOUT);
    httpd_resp_set_hdr(req, "User", "ESP8266");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t whoop_workout_cbk = {
    .uri       = "/whoop/workout",
    .method    = HTTP_GET,
    .handler   = whoop_workout_get_handler,
    .user_ctx  = NULL
};

esp_err_t whoop_cycle_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    whoop_get_data(WHOOP_API_REQUEST_TYPE_CYCLE);
    httpd_resp_set_hdr(req, "User", "ESP8266");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t whoop_cycle_cbk = {
    .uri       = "/whoop/cycle",
    .method    = HTTP_GET,
    .handler   = whoop_cycle_get_handler,
    .user_ctx  = NULL
};

esp_err_t whoop_print_get_handler(httpd_req_t *req)
{
    whoop_data_handle_t handle = NULL;
    if(!get_whoop_cycle_handle_by_id(0, &handle))
    {
        print_whoop_cycle_data(handle);
    }
    else
    {
        ESP_LOGI(TAG, "No cycle data recorded yet");
    }
    if(!get_whoop_workout_handle_by_id(0, &handle))
    {
        print_whoop_workout_data(handle);
    }
    else
    {
        ESP_LOGI(TAG, "No workout data recorded yet");
    }
    if(!get_whoop_sleep_handle_by_id(0, &handle))
    {
        print_whoop_sleep_data(handle);
    }
    else
    {
        ESP_LOGI(TAG, "No sleep data recorded yet");
    }
    if(!get_whoop_recovery_handle_by_id(0, &handle))
    {
        print_whoop_recovery_data(handle);
    }
    else
    {
        ESP_LOGI(TAG, "No workout data recorded yet");
    }
    httpd_resp_set_hdr(req, "User", "ESP8266");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t whoop_print_cbk = {
    .uri       = "/whoop/print",
    .method    = HTTP_GET,
    .handler   = whoop_print_get_handler,
    .user_ctx  = NULL
};

esp_err_t refresh_token_cbk_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    /* Set some custom headers */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[254];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "token", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => token=%s", param);
                whoop_get_token(param, TOKEN_REQUEST_TYPE_REFRESH);
            }
        }
        free(buf);
    }

    const char *req_response = "Recieved refresh token.";
    httpd_resp_send(req, req_response, strlen(req_response));

    return ESP_OK;
}

httpd_uri_t refresh_cbk = {
    .uri       = "/refresh_token",
    .method    = HTTP_GET,
    .handler   = refresh_token_cbk_get_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &auth);
        httpd_register_uri_handler(server, &auth_cbk);
        httpd_register_uri_handler(server, &whoop_cycle_cbk);
        httpd_register_uri_handler(server, &whoop_recover_cbk);
        httpd_register_uri_handler(server, &whoop_sleep_cbk);
        httpd_register_uri_handler(server, &whoop_workout_cbk);
        httpd_register_uri_handler(server, &whoop_print_cbk);
        httpd_register_uri_handler(server, &refresh_cbk);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
    end_whoop_tls_client();
}

static httpd_handle_t server = NULL;

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
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
    whoop_data_handle_t handle = NULL;
    get_touch_button_state(&button_state);
    int data_selection = g_data_selection;
    if(button_state != g_last_button_state)
    {
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
                break;
            case DATA_SELECTION_SLEEP:
                get_whoop_data(handle, WHOOP_DATA_OPT_SLEEP_SLEEP_PERFORMANCE_PERCENTAGE, &data_float);
                sleep_percentage_to_led(data_float);
                break;
            case DATA_SELECTION_CYCLE:
                get_whoop_data(handle, WHOOP_DATA_OPT_CYCLE_STRAIN, &data_float);
                strain_to_led(data_float);
                break;
            case DATA_SELECTION_WORKOUT:
                get_whoop_data(handle, WHOOP_DATA_OPT_WORKOUT_STRAIN, &data_float);
                strain_to_led(data_float);
                break;
        }
    }
 }


void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();

    ESP_ERROR_CHECK(example_connect());
    init_whoop_data();
    init_whoop_tls_client();
    
    initialize_gpio();
    get_touch_button_state(&g_last_button_state);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    server = start_webserver();
    
    //Start task loop
    task_timer_handle = xTimerCreate("Timer", pdMS_TO_TICKS(100) , pdTRUE,( void * ) 0,vTimerCallback);
    xTimerStart( task_timer_handle, 0 );
}
