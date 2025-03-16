#include "esp_event.h"
#include "esp_log.h"
#include <esp_http_server.h>

#include "whoop_data.h"
#include "whoop_client.h"

static const char *TAG="WHOOP REST SERVER";

#define AUTH_ENDPOINT_CBK "/authenticate/callback"
const char * REDIRECT_URI_ESP8266_WHOOP = "https://api.prod.whoop.com/oauth/oauth2/auth"
                "?client_id=02d55f66-a1a6-4970-8e8b-dd31837233ee&response_type=code&"
                "redirect_uri=http://localhost:3100&"
                "state=aaaaaaaa&"
                "scope=offline%20read:recovery%20read:cycles%20read:workout%20read:sleep";


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

void init_whoop_server(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    server = start_webserver();
}

void discard_whoop_server(void)
{
    stop_webserver(server);
}