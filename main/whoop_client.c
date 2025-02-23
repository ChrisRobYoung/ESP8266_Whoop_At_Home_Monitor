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
    whoop_data_t whoop_data;
} whoop_rest_client_t;

whoop_rest_client_t g_whoop_rest_client = {0};
esp_http_client_handle_t client;
char post_data[1024];

#define GET_REQUIRED_JSON_OBJ( parent_obj, obj_assign_to, obj_name ) do { if( !( (obj_assign_to) = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { \
                                            ESP_LOGI(TAG, "Could not find required JSON obj: %s", (obj_name) ); status = -1; goto Error; } } while(0);

#define ASSIGN_OPTIONAL_JSON_INT( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                        if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valueint;  }\
                                        } while(0);
#define ASSIGN_OPTIONAL_JSON_FLOAT( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                            if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = (float) ( (obj_assign_to)->valuedouble );} \
                                            } while(0);
#define ASSIGN_OPTIONAL_JSON_STR( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                                if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valuestring; } \
                                                else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                                } while(0);
#define ASSIGN_REQUIRED_JSON_INT( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                        if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valueint;  }\
                                        else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                        } while(0);
#define ASSIGN_REQUIRED_JSON_FLOAT( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                            if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = (float) ( (obj_assign_to)->valuedouble );} \
                                            else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                            } while(0);
#define ASSIGN_REQUIRED_JSON_STR( parent_obj, obj_assign_to, obj_name,  assign_to )   do { \
                                                if( ( obj_assign_to = cJSON_GetObjectItem( (parent_obj), (obj_name) ) ) ) { assign_to = obj_assign_to->valuestring; } \
                                                else { ESP_LOGI(TAG, "Could not find required parameter: %s", obj_name); status = -1; goto Error; } \
                                                } while(0);

static int parse_cycle_json_data(const char *json_data, whoop_cycle_data_t *cycle)
{
    int status = 0;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *item = NULL;
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    int record_id = 0;
    ASSIGN_REQUIRED_JSON_INT(record, item, "id", record_id);
    if( record_id == cycle->id)
    {
        ESP_LOGI(TAG, "Cycle already recorded or not formatted correctly.");
        status = -1;
        goto Error;
    }
    cycle->id = record_id;

    char *score_state = NULL;
    ASSIGN_REQUIRED_JSON_STR(record, item, "score_state", score_state);
    if(strncmp(score_state, "SCORED",7) )
    {
        ESP_LOGI(TAG, "Cycle score state: %s ", item->valuestring);
        cycle->score_state = !strcmp(score_state, "PENDING") ? WHOOP_SCORE_STATE_PENDING : WHOOP_SCORE_STATE_UNSCORABLE;
        status = -1;
        goto Error;
    }
    cycle->score_state = WHOOP_SCORE_STATE_SCORED;
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "strain", cycle->score.strain);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "kilojoule", cycle->score.kilojoule);
    ASSIGN_REQUIRED_JSON_INT(score, item, "average_heart_rate", cycle->score.average_heart_rate);
    ASSIGN_REQUIRED_JSON_INT(score, item, "max_heart_rate", cycle->score.max_heart_rate);
    
Error:
    cJSON_Delete(json);
    return status;
}
static int parse_recovery_json_data(const char *json_data, whoop_recovery_data_t *recovery)
{
    int status = 0;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *item = NULL;
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);
    ASSIGN_REQUIRED_JSON_INT(record, item, "cycle_id", recovery->cycle_id);
    ASSIGN_REQUIRED_JSON_INT(record, item, "sleep_id", recovery->sleep_id);

    char *score_state = NULL;
    ASSIGN_REQUIRED_JSON_STR(record, item, "score_state", score_state);
    if(strncmp(score_state, "SCORED",7) )
    {
        ESP_LOGI(TAG, "Recovery score state: %s ", item->valuestring);
        recovery->score_state = !strcmp(score_state, "PENDING") ? WHOOP_SCORE_STATE_PENDING : WHOOP_SCORE_STATE_UNSCORABLE;
        status = -1;
        goto Error;
    }
    recovery->score_state = WHOOP_SCORE_STATE_SCORED;
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    ASSIGN_REQUIRED_JSON_INT(score, item, "user_calibrating", recovery->score.user_calibrating);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "recovery_score", recovery->score.recovery_score);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "resting_heart_rate", recovery->score.resting_heart_rate);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "hrv_rmssd_milli", recovery->score.hrv_rmssd_milli);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "spo2_percentage", recovery->score.spo2_percentage);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "skin_temp_celsius", recovery->score.skin_temp_celsius);
    
Error:
    cJSON_Delete(json);
    return status;
}

static int parse_sleep_json_data(const char *json_data, whoop_sleep_data_t *sleep)
{
    int status = 0;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *stage_summary = NULL;
    const cJSON *sleep_needed = NULL;
    const cJSON *item = NULL;
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    int record_id = 0;
    ASSIGN_REQUIRED_JSON_INT(record, item, "id", record_id);
    if( record_id == sleep->id)
    {
        ESP_LOGI(TAG, "Sleep already recorded or not formatted correctly.");
        status = -1;
        goto Error;
    }
    sleep->id = record_id;
    
    ASSIGN_REQUIRED_JSON_INT(record, item, "nap", sleep->nap);

    char *score_state = NULL;
    ASSIGN_REQUIRED_JSON_STR(record, item, "score_state", score_state);
    if(strncmp(score_state, "SCORED",7) )
    {
        ESP_LOGI(TAG, "Sleep score state: %s ", item->valuestring);
        sleep->score_state = !strcmp(score_state, "PENDING") ? WHOOP_SCORE_STATE_PENDING : WHOOP_SCORE_STATE_UNSCORABLE;
        status = -1;
        goto Error;
    }
    sleep->score_state = WHOOP_SCORE_STATE_SCORED;
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    GET_REQUIRED_JSON_OBJ(score, stage_summary, "stage_summary");
    GET_REQUIRED_JSON_OBJ(score, sleep_needed, "sleep_needed");
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_in_bed_time_milli", sleep->score.stage_summary.total_in_bed_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_awake_time_milli", sleep->score.stage_summary.total_awake_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_no_data_time_milli", sleep->score.stage_summary.total_no_data_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_light_sleep_time_milli", sleep->score.stage_summary.total_light_sleep_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_slow_wave_sleep_time_milli", sleep->score.stage_summary.total_slow_wave_sleep_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "total_rem_sleep_time_milli", sleep->score.stage_summary.total_rem_sleep_time_milli);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "sleep_cycle_count", sleep->score.stage_summary.sleep_cycle_count);
    ASSIGN_REQUIRED_JSON_INT(stage_summary, item, "disturbance_count", sleep->score.stage_summary.disturbance_count);
    
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "baseline_milli", sleep->score.sleep_needed.baseline_milli);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_sleep_debt_milli", sleep->score.sleep_needed.need_from_sleep_debt_milli);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_recent_strain_milli", sleep->score.sleep_needed.need_from_recent_strain_milli);
    ASSIGN_REQUIRED_JSON_INT(sleep_needed, item, "need_from_recent_nap_milli", sleep->score.sleep_needed.need_from_recent_nap_milli);
    
    
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "respiratory_rate", sleep->score.respiratory_rate);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_performance_percentage", sleep->score.sleep_performance_percentage);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_consistency_percentage", sleep->score.sleep_consistency_percentage);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "sleep_efficiency_percentage", sleep->score.sleep_efficiency_percentage);
    
Error:
    cJSON_Delete(json);
    return status;
}
static int parse_workout_json_data(const char *json_data, whoop_workout_data_t *workout)
{
    int status = 0;
    cJSON *json = cJSON_Parse(json_data);
    if(!json)
    {
        ESP_LOGI(TAG, "Could not parse JSON.");
        return -1;
    }

    const cJSON *records = NULL;
    const cJSON *record = NULL;
    const cJSON *score = NULL;
    const cJSON *zone_duration = NULL;
    const cJSON *item = NULL;
    GET_REQUIRED_JSON_OBJ(json, records, "records");
    record = cJSON_GetArrayItem(records, 0);

    int record_id = 0;
    ASSIGN_REQUIRED_JSON_INT(record, item, "id", record_id);
    if( record_id == workout->id)
    {
        ESP_LOGI(TAG, "Workout already recorded or not formatted correctly.");
        status = -1;
        goto Error;
    }
    workout->id = record_id;
    
    ASSIGN_REQUIRED_JSON_INT(record, item, "sport_id", workout->sport_id);

    char *score_state = NULL;
    ASSIGN_REQUIRED_JSON_STR(record, item, "score_state", score_state);
    if(strncmp(score_state, "SCORED",7) )
    {
        ESP_LOGI(TAG, "Workout score state: %s ", item->valuestring);
        workout->score_state = !strcmp(score_state, "PENDING") ? WHOOP_SCORE_STATE_PENDING : WHOOP_SCORE_STATE_UNSCORABLE;
        status = -1;
        goto Error;
    }
    workout->score_state = WHOOP_SCORE_STATE_SCORED;
    
    GET_REQUIRED_JSON_OBJ(record, score, "score");
    GET_REQUIRED_JSON_OBJ(score, zone_duration, "zone_duration");

    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "strain", workout->score.strain);
    ASSIGN_REQUIRED_JSON_INT(score, item, "average_heart_rate", workout->score.average_heart_rate);
    ASSIGN_REQUIRED_JSON_INT(score, item, "max_heart_rate", workout->score.max_heart_rate);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "kilojoule", workout->score.kilojoule);
    ASSIGN_REQUIRED_JSON_FLOAT(score, item, "percent_recorded", workout->score.percent_recorded);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "distance_meter", workout->score.distance_meter);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "altitude_gain_meter", workout->score.altitude_gain_meter);
    ASSIGN_OPTIONAL_JSON_FLOAT(score, item, "altitude_change_meter", workout->score.altitude_change_meter);
    
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_zero_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[0]);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_one_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[1]);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_two_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[2]);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_three_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[3]);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_four_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[4]);
    ASSIGN_REQUIRED_JSON_INT(zone_duration,item, "zone_five_milli", workout->score.zone_duration.zone_zero_to_five_time_milli[5]);
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
        strcpy(whoop_rest_client->refresh_token, cJSON_GetStringValue(cJSON_GetObjectItem(json, "refresh_token")));
        const cJSON *expires = cJSON_GetObjectItem(json, "expires_in");
        if(expires)
            whoop_rest_client->expires_in = (int) expires->valuedouble;
        cJSON_Delete(json);
    }
}


void print_whoop_data(void)
{
    whoop_data_t *whoop_data = &g_whoop_rest_client.whoop_data;
    //Sleep data
    if(whoop_data->sleep.id)
    {
        whoop_sleep_data_t *sleep = &whoop_data->sleep;
        ESP_LOGI(TAG, "Sleep ID: %d", sleep->id);
        ESP_LOGI(TAG, sleep->nap ? "This was a nap." : "This was not a nap.");
        if(sleep->score_state == WHOOP_SCORE_STATE_SCORED)
        {
            ESP_LOGI(TAG, "\tRespiratory Rate: %0.2f", sleep->score.respiratory_rate);
            ESP_LOGI(TAG, "\tSleep Consistency Percentage: %0.2f", sleep->score.sleep_consistency_percentage);
            ESP_LOGI(TAG, "\tSleep Efficiency Percentage: %0.2f", sleep->score.sleep_efficiency_percentage);
            ESP_LOGI(TAG, "\tSleep Performance Percentage: %0.2f", sleep->score.sleep_performance_percentage);
            ESP_LOGI(TAG, "\tStage Summary:");
            ESP_LOGI(TAG, "\t\tSleep Cycle Count: %d", sleep->score.stage_summary.sleep_cycle_count);
            ESP_LOGI(TAG, "\t\tTotal Awake Time [ms]: %d", sleep->score.stage_summary.total_awake_time_milli);
            ESP_LOGI(TAG, "\t\tTotal In Bed Time [ms]: %d", sleep->score.stage_summary.total_in_bed_time_milli);
            ESP_LOGI(TAG, "\t\tTotal Light Sleep Time [ms]: %d", sleep->score.stage_summary.total_light_sleep_time_milli);
            ESP_LOGI(TAG, "\t\tTotal No Data Time [ms]: %d", sleep->score.stage_summary.total_no_data_time_milli);
            ESP_LOGI(TAG, "\t\tTotal Slow Wave Sleep Time [ms]: %d", sleep->score.stage_summary.total_slow_wave_sleep_time_milli);
            ESP_LOGI(TAG, "\t\tTotal REM Sleep Time [ms]: %d", sleep->score.stage_summary.total_rem_sleep_time_milli);
            ESP_LOGI(TAG, "\t\tDisturbance Count: %d", sleep->score.stage_summary.disturbance_count);
            ESP_LOGI(TAG, "\tSleep Needed:");
            ESP_LOGI(TAG, "\t\tBaseline [ms]: %d", sleep->score.sleep_needed.baseline_milli);
            ESP_LOGI(TAG, "\t\tNeed From Recent Nap [ms]: %d", sleep->score.sleep_needed.need_from_recent_nap_milli);
            ESP_LOGI(TAG, "\t\tNeed From Recent Strain [ms]: %d", sleep->score.sleep_needed.need_from_recent_strain_milli);
            ESP_LOGI(TAG, "\t\tNeed From Sleep Debt [ms]: %d", sleep->score.sleep_needed.need_from_sleep_debt_milli);
        }
    }
    else
    {
        ESP_LOGI(TAG, "No sleep data recorded yet");
    }

    if(whoop_data->workout.id)
    {
        whoop_workout_data_t *workout = &whoop_data->workout;
        ESP_LOGI(TAG, "Workout ID: %d", workout->id);
        ESP_LOGI(TAG, "Workout Sport ID: %d", workout->sport_id);
        if(workout->score_state == WHOOP_SCORE_STATE_SCORED)
        {
            ESP_LOGI(TAG, "\tAltitude Change Meter: %.2f", workout->score.altitude_change_meter);
            ESP_LOGI(TAG, "\tAltitue Gain Meter: %.2f", workout->score.altitude_gain_meter);
            ESP_LOGI(TAG, "\tAverage Heart Rate: %d", workout->score.average_heart_rate);
            ESP_LOGI(TAG, "\tDistance Meter: %.2f", workout->score.distance_meter);
            ESP_LOGI(TAG, "\tKilojoule: %.2f", workout->score.kilojoule);
            ESP_LOGI(TAG, "\tMax Heart Rate: %d", workout->score.max_heart_rate);
            ESP_LOGI(TAG, "\tPercent Recorded: %.2f", workout->score.percent_recorded);
            ESP_LOGI(TAG, "\tStrain: %.2f", workout->score.strain);
            ESP_LOGI(TAG, "\tTime in Zone:");
            for(int i = 0; i <6; i++)
            {
                ESP_LOGI(TAG, "\t\tZone %d: %d", i, workout->score.zone_duration.zone_zero_to_five_time_milli[i]);
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "No workout data recorded yet");
    }

    if(whoop_data->cycle.id)
    {
        whoop_cycle_data_t *cycle = &whoop_data->cycle;
        ESP_LOGI(TAG, "Cycle ID: %d", cycle->id);
        if(cycle->score_state == WHOOP_SCORE_STATE_SCORED)
        {
            ESP_LOGI(TAG, "\tAverage Heart Rate: %d", cycle->score.average_heart_rate);
            ESP_LOGI(TAG, "\tKilojoule: %.2f", cycle->score.kilojoule);
            ESP_LOGI(TAG, "\tMax Heart Rate: %d", cycle->score.max_heart_rate);
            ESP_LOGI(TAG, "\tStrain: %.2f", cycle->score.strain);
        }
    }
    else
    {
        ESP_LOGI(TAG, "No cycle data recorded yet");
    }

    if(whoop_data->recovery.cycle_id)
    {
        whoop_recovery_data_t *recovery = &whoop_data->recovery;
        ESP_LOGI(TAG, "Recovery Cycle ID: %d", recovery->cycle_id);
        ESP_LOGI(TAG, "Recovery Sleep ID: %d", recovery->sleep_id);
        if(recovery->score_state == WHOOP_SCORE_STATE_SCORED)
        {
            ESP_LOGI(TAG, "\tRecovery Score: %.2f", recovery->score.recovery_score);
            ESP_LOGI(TAG, "\tRMS Heart Rate Variability [ms]: %.2f", recovery->score.hrv_rmssd_milli);
            ESP_LOGI(TAG, "\tResting Heart Rate: %.2f", recovery->score.resting_heart_rate);
            ESP_LOGI(TAG, "\tSkin Temp [c]: %.2f", recovery->score.skin_temp_celsius);
            ESP_LOGI(TAG, "\tSPO2 Percentage: %.2f", recovery->score.spo2_percentage);
            ESP_LOGI(TAG, "\tUser is calibrating?: %s", recovery->score.user_calibrating ? "Yes":"No");
        }
    }
    else
    {
        ESP_LOGI(TAG, "No recovery data recorded yet");
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

int perform_https_and_check_error(esp_http_client_handle_t client)
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

void handle_whoop_api_response_data(int response_code, whoop_api_request_type_n request, whoop_rest_client_t *data)
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
            status = parse_cycle_json_data(data->server_response, &data->whoop_data.cycle);
            break;
        case WHOOP_API_REQUEST_TYPE_RECOVERY:
            status = parse_recovery_json_data(data->server_response, &data->whoop_data.recovery);
            break;
        case WHOOP_API_REQUEST_TYPE_SLEEP:
            status = parse_sleep_json_data(data->server_response, &data->whoop_data.sleep);
            break;
        case WHOOP_API_REQUEST_TYPE_WORKOUT:
            status = parse_workout_json_data(data->server_response, &data->whoop_data.workout);
            break;
    }
    if(status)
    {
        ESP_LOGI(TAG, "Encountered an error when parsing data.");
    }
}

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
        strcpy(post_data, "grant_type=refresh_token&client_id=" CLIENT_ID "&client_secret=" CLIENT_SECRET "&code=");
        loc = post_data + strlen(post_data);
        strcpy(loc, code_or_token);
        ESP_LOGI(TAG, "Sending https post string: %s",post_data);
     // grant_type=refresh_token&refresh_token=tGzv3JOkF0XG5Qx2TlKWIA&client_id=..&client_secret=..
    }
    
    else if (token_request_type == TOKEN_REQUEST_TYPE_AUTH_CODE)
    {
        strcpy(post_data, "grant_type=authorization_code&client_id=" CLIENT_ID "&client_secret=" CLIENT_SECRET "&redirect_uri=http://localhost:3100&code=");
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
    if(response_code == 200) parse_token_json_response(&g_whoop_rest_client);

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