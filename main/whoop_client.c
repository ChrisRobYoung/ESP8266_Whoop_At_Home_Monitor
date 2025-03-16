/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "cJSON.h"
#include "whoop_client.h"

#include "esp_http_client.h"
#include "whoop_data.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

//Replace with config variables
#define CLIENT_ID CONFIG_CLIENT_ID
#define CLIENT_SECRET CONFIG_CLIENT_SECRET

static const char *TAG = "WHOOP CLIENT";
extern const char whoop_we1_pem_start[] asm("_binary_whoop_we1_pem_start");
extern const char whoop_we1_pem_end[]   asm("_binary_whoop_we1_pem_end");

typedef enum whoop_event_type
{
    WHOOP_EVENT_TYPE_GET_SLEEP,
    WHOOP_EVENT_TYPE_GET_WORKOUT,
    WHOOP_EVENT_TYPE_GET_RECOVERY,
    WHOOP_EVENT_TYPE_GET_CYCLE,
    WHOOP_EVENT_TYPE_ACCESS_TOKEN
} whoop_event_type_n;

typedef struct whoop_rest_client
{
    char *server_response;
    char access_token[128];
    int expires_in;
    char refresh_token[128];
} whoop_rest_client_t;

whoop_rest_client_t g_whoop_rest_client = {0};
esp_http_client_handle_t client;
char post_data[1024];

#define GET_REQUIRED_JSON_OBJ( parent_obj, obj_assign_to, obj_name ) do { if( !( (obj_assign_to) = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { \
                                            ESP_LOGI(TAG, "Could not find required JSON obj: %s", (obj_name) ); status = -1; goto Error; } } while(0);

#define ASSIGN_OPTIONAL_JSON_INT( parent_obj, obj_assign_to, obj_name,  handle, data_opt )   do { \
                                        if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) \ ) \
                                        { set_whoop_data( (handle) , (data_opt), obj_assign_to->valueint);  }\
                                        } while(0);
#define ASSIGN_OPTIONAL_JSON_FLOAT( parent_obj, obj_assign_to, obj_name,  handle, data_opt )   do { \
                                            if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) \
                                            { set_whoop_data( (handle) , (data_opt), (float) ( obj_assign_to->valuedouble ) );} \
                                               } while(0);
#define GET_OPTIONAL_JSON_STR( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                                if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valuestring; } \
                                                else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                                } while(0);
#define ASSIGN_REQUIRED_JSON_INT( parent_obj, obj_assign_to, obj_name,  handle, data_opt )   do { \
                                        if( !( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) \
                                        || ( set_whoop_data(handle, data_opt,  obj_assign_to->valueint) ) )\
                                        { ESP_LOGI(TAG, "Error finding or setting following parameter: %s", obj_name); status = -1; goto Error; } \
                                        } while(0);
#define ASSIGN_REQUIRED_JSON_FLOAT( parent_obj, obj_assign_to, obj_name,  handle, data_opt )   do { \
                                            if( !( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) \
                                            || ( set_whoop_data(handle, data_opt,  (float) ( obj_assign_to->valuedouble ) ) ) )\
                                            { ESP_LOGI(TAG, "Error finding or setting following parameter: %s", obj_name); status = -1; goto Error; } \
                                            } while(0);
#define GET_REQUIRED_JSON_STR( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                                if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valuestring; } \
                                                else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                                } while(0);


//Local functions
static whoop_score_state_n parse_string_to_score_state(const char *str)
{
    if(!strncmp(str, "SCORED",7) )
    {
        return WHOOP_SCORE_STATE_SCORED;
    }
    else if(!strcmp(str, "PENDING"))
    {
        return WHOOP_SCORE_STATE_PENDING;
    }
    else
    {
        return WHOOP_SCORE_STATE_UNSCORABLE;
    }
}

static int parse_cycle_json_data(const char *json_data)
{
    whoop_data_handle_t handle = NULL;
    whoop_score_state_n score_state_value = 0;
    char *score_state_str = NULL;
    int status = 0;
    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *item = NULL;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    if( !( item = cJSON_GetObjectItem( record, "id" ) ) )
    {   
        ESP_LOGI(TAG, "Could not find required parameter: id");
        goto Error; 
    }
    else
    {
        if( ( status = get_whoop_cycle_handle_by_id(item->valueint, &handle) ) ) 
        {
            if( ( status = create_whoop_cycle_data(item->valueint, &handle) ) ) 
            {
                ESP_LOGI(TAG, "Could not create cycle.");
                goto Error;
            }
        }
        else
        {
            ESP_LOGI(TAG, "Cycle already recorded.");
        }
    }
    
    GET_REQUIRED_JSON_STR(record, item, "score_state", score_state_str);
    score_state_value = parse_string_to_score_state(score_state_str);
    set_whoop_data(handle, WHOOP_DATA_OPT_CYCLE_SCORE_STATE, score_state_value);
    if(score_state_value != WHOOP_SCORE_STATE_SCORED )
    {
        ESP_LOGI(TAG, "Cycle not scored.");
        goto Error;
    }
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    ASSIGN_REQUIRED_JSON_INT(score, item, "average_heart_rate", handle, WHOOP_DATA_OPT_CYCLE_AVERAGE_HEART_RATE);
    ASSIGN_REQUIRED_JSON_INT(score, item, "max_heart_rate", handle, WHOOP_DATA_OPT_CYCLE_MAX_HEART_RATE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "strain", handle, WHOOP_DATA_OPT_CYCLE_STRAIN);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "kilojoule", handle, WHOOP_DATA_OPT_CYCLE_KILOJOULE);

Error:
    cJSON_Delete(json);
    return status;
}

static int parse_recovery_json_data(const char *json_data)
{
    whoop_data_handle_t handle = NULL;
    whoop_score_state_n score_state_value = 0;
    char *score_state_str = NULL;
    int status = 0;
    int sleep_id = 0;
    int cycle_id = 0;
    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *item = NULL;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);
    if( !( item = cJSON_GetObjectItem( record, "sleep_id" ) ) )
    {
        ESP_LOGI(TAG, "Could not find required parameter: id");
        goto Error; 
    }
    sleep_id = item->valueint;   
    if( !( item = cJSON_GetObjectItem( record, "cycle_id" ) ) )
    {
        ESP_LOGI(TAG, "Could not find required parameter: id");
        goto Error; 
    }
    cycle_id = item->valueint; 

    if( ( status = get_whoop_recovery_handle_by_id(sleep_id, &handle) ) ||  ( status = get_whoop_recovery_handle_by_id(cycle_id, &handle) ) ) 
    {
        if( ( status = create_whoop_recovery_data(sleep_id, cycle_id, &handle) ) ) 
        {
            ESP_LOGI(TAG, "Could not create recovery.");
            goto Error;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Recovery already recorded.");
    }

    GET_REQUIRED_JSON_STR(record, item, "score_state", score_state_str);
    score_state_value = parse_string_to_score_state(score_state_str);
    set_whoop_data(handle, WHOOP_DATA_OPT_RECOVERY_SCORE_STATE, score_state_value);
    if(score_state_value != WHOOP_SCORE_STATE_SCORED )
    {
        ESP_LOGI(TAG, "Recovery not scored.");
        goto Error;
    }
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    ASSIGN_REQUIRED_JSON_INT(score, item, "user_calibrating", handle, WHOOP_DATA_OPT_RECOVERY_USER_CALIBRATING);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "recovery_score", handle, WHOOP_DATA_OPT_RECOVERY_RECOVERY_SCORE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "resting_heart_rate", handle, WHOOP_DATA_OPT_RECOVERY_RESTING_HEART_RATE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "hrv_rmssd_milli", handle, WHOOP_DATA_OPT_RECOVERY_HRV_RMSSD_MILLI);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "spo2_percentage", handle, WHOOP_DATA_OPT_RECOVERY_SPO2_PERCENTAGE);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "skin_temp_celsius", handle, WHOOP_DATA_OPT_RECOVERY_SKIN_TEMP_CELCIUS);
    
Error:
    cJSON_Delete(json);
    return status;
}

static int parse_sleep_json_data(const char *json_data)
{
    whoop_data_handle_t handle = NULL;
    whoop_score_state_n score_state_value = 0;
    char *score_state_str = NULL;
    int status = 0;
    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *item = NULL;
    const cJSON *sleep_needed = NULL;
    const cJSON *stage_summary = NULL;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    if( !( item = cJSON_GetObjectItem( record, "id" ) ) )
    {   
        ESP_LOGI(TAG, "Could not find required parameter: id");
        goto Error; 
    }
    else
    {
        if( ( status = get_whoop_sleep_handle_by_id(item->valueint, &handle) ) ) 
        {
            if( ( status = create_whoop_sleep_data(item->valueint, &handle) ) ) 
            {
                ESP_LOGI(TAG, "Could not create sleep.");
                goto Error;
            }
        }
        else
        {
            ESP_LOGI(TAG, "Sleep already recorded.");
        }
    }

    GET_REQUIRED_JSON_STR(record, item, "score_state", score_state_str);
    score_state_value = parse_string_to_score_state(score_state_str);
    set_whoop_data(handle, WHOOP_DATA_OPT_SLEEP_SCORE_STATE, score_state_value);
    if(score_state_value != WHOOP_SCORE_STATE_SCORED )
    {
        ESP_LOGI(TAG, "Sleep not scored.");
        goto Error;
    }
    ASSIGN_REQUIRED_JSON_INT(record, item, "nap", handle, WHOOP_DATA_OPT_SLEEP_NAP_BOOL);

    GET_REQUIRED_JSON_OBJ(record, score, "score");
    GET_REQUIRED_JSON_OBJ(score, stage_summary, "stage_summary");
    GET_REQUIRED_JSON_OBJ(score, sleep_needed, "sleep_needed");
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_in_bed_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_IN_BED_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_awake_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_AWAKE_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_no_data_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_NO_DATA_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_light_sleep_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_LIGHT_SLEEP_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_slow_wave_sleep_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_SLOW_WAVE_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_rem_sleep_time_milli", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_REM_SLEEP_TIME_MILLI);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "sleep_cycle_count", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_SLEEP_CYCLE_COUNT);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "disturbance_count", handle, WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_DISTURBANCE_COUNT);
    
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "baseline_milli", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_BASELINE_MILLI);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_sleep_debt_milli", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_SLEEP_DEBT_MILLI);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_recent_strain_milli", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_RECENT_STRAIN_DEBT_MILLI);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_recent_nap_milli", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_RECENT_NAP_DEBT_MILLI);
    
    
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "respiratory_rate", handle, WHOOP_DATA_OPT_SLEEP_RESPIRATORY_RATE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_performance_percentage", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_PERFORMANCE_PERCENTAGE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_consistency_percentage", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_CONSISTENCY_PERCENTAGE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_efficiency_percentage", handle, WHOOP_DATA_OPT_SLEEP_SLEEP_EFFICIENCY_PERCENTAGE);
    
Error:
    cJSON_Delete(json);
    return status;
}

static int parse_workout_json_data(const char *json_data)
{
    whoop_data_handle_t handle = NULL;
    whoop_score_state_n score_state_value = 0;
    char *score_state_str = NULL;
    int status = 0;
    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *zone_duration = NULL;
    const cJSON *item = NULL;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    if( !( item = cJSON_GetObjectItem( record, "id" ) ) )
    {   
        ESP_LOGI(TAG, "Could not find required parameter: id");
        goto Error; 
    }
    else
    {
        if( ( status = get_whoop_workout_handle_by_id(item->valueint, &handle) ) ) 
        {
            if( ( status = create_whoop_workout_data(item->valueint, &handle) ) ) 
            {
                ESP_LOGI(TAG, "Could not create workout.");
                goto Error;
            }
        }
        else
        {
            ESP_LOGI(TAG, "Workout already recorded.");
        }
    }
    ASSIGN_REQUIRED_JSON_INT(record, item, "sport_id", handle, WHOOP_DATA_OPT_WORKOUT_SPORT_ID);

    GET_REQUIRED_JSON_STR(record, item, "score_state", score_state_str);
    score_state_value = parse_string_to_score_state(score_state_str);
    set_whoop_data(handle, WHOOP_DATA_OPT_WORKOUT_SCORE_STATE, score_state_value);
    if(score_state_value != WHOOP_SCORE_STATE_SCORED )
    {
        ESP_LOGI(TAG, "Workout not scored.");
        goto Error;
    }
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    GET_REQUIRED_JSON_OBJ(score, zone_duration, "zone_duration");

    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "strain", handle, WHOOP_DATA_OPT_WORKOUT_STRAIN);
    ASSIGN_REQUIRED_JSON_INT(score, item, "average_heart_rate", handle, WHOOP_DATA_OPT_WORKOUT_AVERAGE_HEART_RATE);
    ASSIGN_REQUIRED_JSON_INT(score, item, "max_heart_rate", handle, WHOOP_DATA_OPT_WORKOUT_MAX_HEART_RATE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "kilojoule", handle, WHOOP_DATA_OPT_WORKOUT_KILOJOULE);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "percent_recorded", handle, WHOOP_DATA_OPT_WORKOUT_PERCENT_RECORDED);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "distance_meter", handle, WHOOP_DATA_OPT_WORKOUT_DISTANCE_METER);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "altitude_gain_meter", handle, WHOOP_DATA_OPT_WORKOUT_ALTITUDE_GAIN_METER);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "altitude_change_meter", handle, WHOOP_DATA_OPT_WORKOUT_ALTITUDE_CHANGE_METER);
    
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_zero_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_ZERO);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_one_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_ONE);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_two_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_TWO);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_three_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_THREE);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_four_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_FOUR);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_five_milli", handle, WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_FIVE);
Error:
    cJSON_Delete(json);
    return status;
}

static void parse_token_json_response(whoop_rest_client_t *whoop_rest_client)
{
    cJSON *json = cJSON_Parse(whoop_rest_client->server_response);
    if(json)
    {
        strcpy(whoop_rest_client->access_token, cJSON_GetStringValue(cJSON_GetObjectItem(json, "access_token")));
        ESP_LOGI(TAG, "Access token: %s", whoop_rest_client->access_token);
        strcpy(whoop_rest_client->refresh_token, cJSON_GetStringValue(cJSON_GetObjectItem(json, "refresh_token")));
        ESP_LOGI(TAG, "Refresh token: %s", whoop_rest_client->refresh_token);
        const cJSON *expires = cJSON_GetObjectItem(json, "expires_in");
        if(expires)
            whoop_rest_client->expires_in = (int) expires->valuedouble;
        cJSON_Delete(json);
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    whoop_rest_client_t *event_data = (whoop_rest_client_t *) evt->user_data;
    // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            if(esp_http_client_is_chunked_response(evt->client))
            {
                if (event_data->server_response == NULL) {
                    event_data->server_response = (char *) malloc(evt->data_len);
                    output_len = 0;
                }
                else 
                {
                    event_data->server_response = (char *) realloc(event_data->server_response, output_len + evt->data_len + 1);
                }
                if (event_data->server_response == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
            }
            else
            {
                if (event_data->server_response == NULL) {
                    event_data->server_response = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (event_data->server_response == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
            }
            memcpy(event_data->server_response + output_len, evt->data, evt->data_len);
            output_len += evt->data_len;
            ESP_LOGI(TAG, "Wrote %d bytes to user data", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (event_data->server_response != NULL) {
                event_data->server_response[output_len] = '\0';
                // Response is accumulated in event_data->server_response. Uncomment the below line to print the accumulated response
                ESP_LOGI(TAG, event_data->server_response);
                output_len = 0;
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

static int perform_https_and_check_error(esp_http_client_handle_t client)
{
    esp_err_t err = esp_http_client_perform(client);
    int response_code = 400;
    if (err == ESP_OK) {
        response_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                response_code,
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    return response_code;
}

static void handle_whoop_api_response_data(int response_code, whoop_api_request_type_n request, whoop_rest_client_t *data)
{
    if(response_code != 401 && response_code != 200 ) 
    {
        ESP_LOGI(TAG, "Response code not 401 or 200. Response code: %d", response_code);
        return;
    }
    if(response_code == 401)
    {
        ESP_LOGI(TAG, "Recieved 401 code... Refreshing token.");
        whoop_get_token(data->refresh_token, TOKEN_REQUEST_TYPE_REFRESH);
        return;
    }
    int status = 0;
    switch(request)
    {
        case WHOOP_API_REQUEST_TYPE_CYCLE:
            status = parse_cycle_json_data(data->server_response);
            break;
        case WHOOP_API_REQUEST_TYPE_RECOVERY:
            status = parse_recovery_json_data(data->server_response);
            break;
        case WHOOP_API_REQUEST_TYPE_SLEEP:
            status = parse_sleep_json_data(data->server_response);
            break;
        case WHOOP_API_REQUEST_TYPE_WORKOUT:
            status = parse_workout_json_data(data->server_response);
            break;
    }
    if(status)
    {
        ESP_LOGI(TAG, "Encountered an error when parsing data.");
    }
}

//Public functions
void whoop_get_data(whoop_api_request_type_n request_type)
{
    int response_code = 400;
    char *authorization_string = (char *) malloc(strlen("Bearer ") + strlen(g_whoop_rest_client.access_token) + 1);
    if(!authorization_string) return;

    strcpy(authorization_string, "Bearer ");
    strcpy(authorization_string+ strlen(authorization_string), g_whoop_rest_client.access_token);
    esp_http_client_set_header(client, "Authorization", authorization_string);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    switch(request_type)
    {
        case WHOOP_API_REQUEST_TYPE_CYCLE:
            esp_http_client_set_url(client, "/developer/v1/cycle?limit=1");
            break;
        case WHOOP_API_REQUEST_TYPE_RECOVERY:
            esp_http_client_set_url(client, "/developer/v1/recovery?limit=1");
            break;
        case WHOOP_API_REQUEST_TYPE_SLEEP:
            esp_http_client_set_url(client, "/developer/v1/activity/sleep?limit=1");
            break;
        case WHOOP_API_REQUEST_TYPE_WORKOUT:
            esp_http_client_set_url(client, "/developer/v1/activity/workout?limit=1");
            break;
        default:
            break;
    }

    response_code = perform_https_and_check_error(client);
    handle_whoop_api_response_data(response_code, request_type, &g_whoop_rest_client);
    if(g_whoop_rest_client.server_response)
    {
        free(g_whoop_rest_client.server_response);
        g_whoop_rest_client.server_response = NULL;
    }

    //clean up
    esp_http_client_delete_header(client, "Authorization");
    free(authorization_string);
}

void whoop_get_token(const char *code_or_token, int token_request_type)
{
    int response_code = 400;
    char *loc = NULL;
    if(token_request_type == TOKEN_REQUEST_TYPE_REFRESH)
    {
        strcpy(post_data, "grant_type=refresh_token&client_id=" CLIENT_ID "&client_secret=" CLIENT_SECRET "&refresh_token=");
        loc = post_data + strlen(post_data);
        strcpy(loc, code_or_token);
        ESP_LOGI(TAG, "Sending https post string: %s",post_data);
     // grant_type=refresh_token&refresh_token=tGzv3JOkF0XG5Qx2TlKWIA&client_id=..&client_secret=..
    }
    
    else if (token_request_type == TOKEN_REQUEST_TYPE_AUTH_CODE)
    {
        strcpy(post_data, "grant_type=authorization_code&client_id=" CLIENT_ID "&client_secret=" CLIENT_SECRET "&scope=offline&redirect_uri=http://localhost:3100&code=");
        loc = post_data + strlen(post_data);
        strcpy(loc, code_or_token);
        ESP_LOGI(TAG, "Sending https post string: %s",post_data);
        //grant_type=authorization_code&code=SplxlOBeZQQYbYS6WxSbIA&client_id=..&client_secret=...&
     //&redirect_uri=http://localhost:3100
    }
    else
    {
        ESP_LOGI(TAG, "Invalid token request code");
        return;
    }
    esp_http_client_set_url(client, "/oauth/oauth2/token");
    esp_http_client_set_header(client, "content-type", "application/x-www-form-urlencoded");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    response_code = perform_https_and_check_error(client);
    if(response_code == 200)
    {
        parse_token_json_response(&g_whoop_rest_client);
    }

    if(g_whoop_rest_client.server_response)
    {
        free(g_whoop_rest_client.server_response);
        g_whoop_rest_client.server_response = NULL;
    }
    //clean up
    esp_http_client_delete_header(client, "content-type");
}

esp_http_client_config_t whoop_config = {
    .host = "api.prod.whoop.com",
    .path = "/",
    .user_data = (void *) &g_whoop_rest_client,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = _http_event_handler,
    .cert_pem = whoop_we1_pem_start,
};
void init_whoop_tls_client(void)
{
    client = esp_http_client_init(&whoop_config);
}

void end_whoop_tls_client(void)
{
    esp_http_client_cleanup(client);
}