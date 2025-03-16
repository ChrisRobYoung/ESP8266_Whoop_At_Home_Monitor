#include <string.h>
#include <stdarg.h>
#include "esp_log.h"
#include "whoop_data.h"

// Defines
#define MAX_NUMBER_RECORDINGS 5

#define WHOOP_SLEEP_BIT_ID  0x0001
#define WHOOP_CYCLE_BIT_ID  0x0002
#define WHOOP_WORKOUT_BIT_ID  0x0004
#define WHOOP_RECOVERY_BIT_ID  0x0008

#define MIN(data_1, data_2) ( ( (data_1) < (data_2) ) ? (data_1) : (data_2) ) 
// Types 
typedef struct whoop_recovery_ints
{
    int cycle_id;
    int sleep_id;
    int score_state;
    int user_calibrating;
} whoop_recovery_ints_t;

typedef struct whoop_recovery_floats
{
    float recovery_score;
    float resting_heart_rate;
    float hrv_rmssd_milli;
    float spo2_percentage;
    float skin_temp_celsius;
} whoop_recovery_floats_t;

typedef struct whoop_recovery_data {
    whoop_recovery_ints_t recovery_ints;
    whoop_recovery_floats_t recovery_floats;
} whoop_recovery_data_t;

typedef struct whoop_cycle_ints
{
    int id;
    int score_state;
    int average_heart_rate;
    int max_heart_rate;
} whoop_cycle_ints_t;

typedef struct whoop_cycle_floats
{
    float strain;
    float kilojoule;
} whoop_cycle_floats_t;

typedef struct whoop_cycle_data {
    whoop_cycle_ints_t cycle_ints;
    whoop_cycle_floats_t cycle_floats;
} whoop_cycle_data_t;

typedef struct whoop_workout_ints
{
    int id;
    int score_state;
    int sport_id;
    int average_heart_rate;
    int max_heart_rate;
    int zone_zero_to_five_time_milli[6];
} whoop_workout_ints_t;

typedef struct whoop_workout_floats
{
    float strain;
    float kilojoule;
    float percent_recorded;
    float distance_meter;
    float altitude_gain_meter;
    float altitude_change_meter;
} whoop_workout_floats_t;

typedef struct whoop_workout_data {
    whoop_workout_ints_t workout_ints;
    whoop_workout_floats_t workout_floats;
} whoop_workout_data_t;

typedef struct whoop_sleep_ints {
    int id;
    int score_state;
    int nap;
    int total_in_bed_time_milli;
    int total_awake_time_milli;
    int total_no_data_time_milli;
    int total_light_sleep_time_milli;
    int total_slow_wave_sleep_time_milli;
    int total_rem_sleep_time_milli;
    int sleep_cycle_count;
    int disturbance_count;
    int baseline_milli;
    int need_from_sleep_debt_milli;
    int need_from_recent_strain_milli;
    int need_from_recent_nap_milli;
} whoop_sleep_ints_t;

typedef struct whoop_sleep_floats{
    float respiratory_rate;
    float sleep_performance_percentage;
    float sleep_consistency_percentage;
    float sleep_efficiency_percentage;
} whoop_sleep_floats_t;

typedef struct whoop_sleep_data {
    whoop_sleep_ints_t sleep_ints;
    whoop_sleep_floats_t sleep_floats;
} whoop_sleep_data_t;

typedef struct whoop_data {
    whoop_cycle_data_t **cycle_list;
    whoop_workout_data_t **workout_list;
    whoop_recovery_data_t **recovery_list;
    whoop_sleep_data_t **sleep_list;
} whoop_data_t;

// Local Global Variables
static const char *TAG = "WHOOP DATA";

whoop_sleep_data_t g_sleep_data_list[MAX_NUMBER_RECORDINGS];
whoop_cycle_data_t g_cycle_data_list[MAX_NUMBER_RECORDINGS];
whoop_workout_data_t g_workout_data_list[MAX_NUMBER_RECORDINGS];
whoop_recovery_data_t g_recovery_data_list[MAX_NUMBER_RECORDINGS];

whoop_sleep_data_t *g_sleep_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_cycle_data_t *g_cycle_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_workout_data_t *g_workout_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_recovery_data_t *g_recovery_data_ptr_list[MAX_NUMBER_RECORDINGS];

static int g_cycle_data_record_count = 0;
static int g_workout_data_record_count = 0;
static int g_sleep_data_record_count = 0;
static int g_recovery_data_record_count = 0;

whoop_data_handle_t g_most_recent_sleep = NULL;
whoop_data_handle_t g_most_recent_cycle = NULL;
whoop_data_handle_t g_most_recent_workout = NULL;
whoop_data_handle_t g_most_recent_recovery = NULL;


whoop_data_t g_whoop_data;

// Local functions  (Place holders if memory management is introduced)
void discard_cycle_data(whoop_cycle_data_t *cycle_data)
{

}
void discard_workout_data(whoop_workout_data_t *workout_data)
{

}
void discard_sleep_data(whoop_sleep_data_t *sleep_data)
{

}
void discard_recovery_data(whoop_recovery_data_t *recovery_data)
{

}

int deconstruct_whoop_data_opt(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, int **int_array_out, float **float_array_out, int *data_is_int_bool_out, int *data_offset_out)
{
    whoop_sleep_data_t *sleep_data = NULL;
    whoop_cycle_data_t *cycle_data = NULL;
    whoop_recovery_data_t *recovery_data = NULL;
    whoop_workout_data_t *workout_data = NULL;
    //ESP_LOGI(TAG, "Assiging data opt: %x", whoop_data_opt);
    switch( (whoop_data_opt & 0xf000) >> 12)
    {
        case WHOOP_SLEEP_BIT_ID:
            sleep_data = (whoop_sleep_data_t *) handle;
            *int_array_out = &sleep_data->sleep_ints.id;
            *float_array_out = &sleep_data->sleep_floats.respiratory_rate;
            //ESP_LOGI(TAG, "Data opt sleep data.");
            break;
        case WHOOP_CYCLE_BIT_ID:
            cycle_data = (whoop_cycle_data_t *) handle;
            *int_array_out = &cycle_data->cycle_ints.id;
            *float_array_out = &cycle_data->cycle_floats.strain;    
            //ESP_LOGI(TAG, "Data opt is sleep data.");
            break;
        case WHOOP_RECOVERY_BIT_ID:
            recovery_data = (whoop_recovery_data_t *) handle;
            *int_array_out = &recovery_data->recovery_ints.cycle_id;
            *float_array_out = &recovery_data->recovery_floats.recovery_score;  
            //ESP_LOGI(TAG, "Data opt is recovery data.");  
            break;
        case WHOOP_WORKOUT_BIT_ID:
            workout_data = (whoop_workout_data_t *) handle;
            *int_array_out = &workout_data->workout_ints.id;
            *float_array_out = &workout_data->workout_floats.strain;    
            //ESP_LOGI(TAG, "Data opt is workout data.");
            break;
        default:
            return WHOOP_DATA_STATUS_INVALID_OPTION;
    }

    *data_is_int_bool_out = (whoop_data_opt & 0x0100) ? 0 : 1;
    *data_offset_out = whoop_data_opt & 0x00ff;
    //ESP_LOGI(TAG, "Data opt integer bool: %d", *data_is_int_bool_out);
    //ESP_LOGI(TAG, "Data offset value: %d", *data_offset_out);
    return WHOOP_DATA_STATUS_OK;
}

// Global functions
int init_whoop_data(void)
{
    for(int index = 0; index < MAX_NUMBER_RECORDINGS; index++)
    {
        g_sleep_data_ptr_list[index] =  &g_sleep_data_list[index];
        g_cycle_data_ptr_list[index] = &g_cycle_data_list[index];
        g_workout_data_ptr_list[index] = &g_workout_data_list[index];
        g_recovery_data_ptr_list[index] = &g_recovery_data_list[index];
    }
    g_whoop_data.recovery_list = g_recovery_data_ptr_list;
    g_whoop_data.sleep_list = g_sleep_data_ptr_list;
    g_whoop_data.cycle_list = g_cycle_data_ptr_list;
    g_whoop_data.workout_list = g_workout_data_ptr_list;
    return WHOOP_DATA_STATUS_OK;
}
int discard_whoop_data(void)
{   
    int index;
    for(index = 0; index < MAX_NUMBER_RECORDINGS; index++)  discard_cycle_data(g_whoop_data.cycle_list[index]);
    for(index = 0; index < MAX_NUMBER_RECORDINGS; index++)  discard_recovery_data(g_whoop_data.recovery_list[index]);    
    for(index = 0; index < MAX_NUMBER_RECORDINGS; index++)  discard_sleep_data(g_whoop_data.sleep_list[index]);    
    for(index = 0; index < MAX_NUMBER_RECORDINGS; index++)  discard_workout_data(g_whoop_data.workout_list[index]); 
    return WHOOP_DATA_STATUS_OK;      
}

/*Sleep ID or 0 for most recent*/
int get_whoop_sleep_handle_by_id(int id, whoop_data_handle_t *handle)
{
    if(g_most_recent_sleep == NULL)
        return WHOOP_DATA_STATUS_NO_RECORDINGS;
    if(id == 0)
    {
        *handle = g_most_recent_sleep;
        return WHOOP_DATA_STATUS_OK;
    }
    int count = MIN(g_sleep_data_record_count, MAX_NUMBER_RECORDINGS);
    for(int index = 0; index < count; index++)
    {
        if(g_whoop_data.sleep_list[index]->sleep_ints.id == id)
        {
            *handle = (whoop_data_handle_t) g_whoop_data.sleep_list[index];
            return WHOOP_DATA_STATUS_OK;
        }
    }
    return WHOOP_DATA_STATUS_ID_NOT_FOUND;
}

int create_whoop_sleep_data(int id, whoop_data_handle_t *handle)
{
    whoop_sleep_data_t *sleep_to_write = g_whoop_data.sleep_list[g_sleep_data_record_count % MAX_NUMBER_RECORDINGS];
    memset( sleep_to_write, 0, sizeof(whoop_sleep_data_t) );
    sleep_to_write->sleep_ints.id = id;
    g_sleep_data_record_count++;
    g_most_recent_sleep = (whoop_data_handle_t) sleep_to_write;
    if(handle) *handle = g_most_recent_sleep;
    return WHOOP_DATA_STATUS_OK;
}

/*sleep ID or 0 for most recent*/
int get_whoop_cycle_handle_by_id(int id, whoop_data_handle_t *handle)
{
    if(g_most_recent_cycle == NULL)
        return WHOOP_DATA_STATUS_NO_RECORDINGS;
    if(id == 0)
    {
        *handle = g_most_recent_cycle;
        return WHOOP_DATA_STATUS_OK;
    }
    int count = MIN(g_cycle_data_record_count, MAX_NUMBER_RECORDINGS);
    for(int index = 0; index < count; index++)
    {
        if(g_whoop_data.cycle_list[index]->cycle_ints.id == id)
        {
            *handle = (whoop_data_handle_t) g_whoop_data.cycle_list[index];
            return WHOOP_DATA_STATUS_OK;
        }
    }
    return WHOOP_DATA_STATUS_ID_NOT_FOUND;
}
int create_whoop_cycle_data(int id, whoop_data_handle_t *handle)
{
    whoop_cycle_data_t *cycle_to_write = g_whoop_data.cycle_list[g_cycle_data_record_count % MAX_NUMBER_RECORDINGS];
    memset( cycle_to_write, 0, sizeof(whoop_cycle_data_t) );
    cycle_to_write->cycle_ints.id = id;
    g_cycle_data_record_count++;
    g_most_recent_cycle = (whoop_data_handle_t) cycle_to_write;
    if(handle) *handle = g_most_recent_cycle;
    return WHOOP_DATA_STATUS_OK;
}

/*Workout ID or 0 for most recent*/
int get_whoop_workout_handle_by_id(int id, whoop_data_handle_t *handle)
{
    if(g_most_recent_workout == NULL)
        return WHOOP_DATA_STATUS_NO_RECORDINGS;
    if(id == 0)
    {
        *handle = g_most_recent_workout;
        return WHOOP_DATA_STATUS_OK;
    }
    int count = MIN(g_workout_data_record_count, MAX_NUMBER_RECORDINGS);
    for(int index = 0; index < count; index++)
    {
        if(g_whoop_data.workout_list[index]->workout_ints.id == id)
        {
            *handle = (whoop_data_handle_t) g_whoop_data.workout_list[index];
            return WHOOP_DATA_STATUS_OK;
        }
    }
    return WHOOP_DATA_STATUS_ID_NOT_FOUND;
}
int create_whoop_workout_data(int id, whoop_data_handle_t *handle)
{
    whoop_workout_data_t *workout_to_write = g_whoop_data.workout_list[g_workout_data_record_count % MAX_NUMBER_RECORDINGS];
    memset( workout_to_write, 0, sizeof(whoop_workout_data_t) );
    workout_to_write->workout_ints.id = id;
    g_workout_data_record_count++;
    g_most_recent_workout = (whoop_data_handle_t) workout_to_write;
    if(handle) *handle = g_most_recent_workout;
    return WHOOP_DATA_STATUS_OK;
}

/*ID can be either sleep or sleep id related to recovery or 0 for most recent*/
int get_whoop_recovery_handle_by_id(int id, whoop_data_handle_t *handle)
{
    if(g_most_recent_recovery == NULL)
        return WHOOP_DATA_STATUS_NO_RECORDINGS;
    if(id == 0)
    {
        *handle = g_most_recent_recovery;
        return WHOOP_DATA_STATUS_OK;
    }
    int count = MIN(g_recovery_data_record_count, MAX_NUMBER_RECORDINGS);
    for(int index = 0; index < count; index++)
    {
        if(g_whoop_data.recovery_list[index]->recovery_ints.sleep_id == id 
            || g_whoop_data.recovery_list[index]->recovery_ints.cycle_id == id)
        {
            *handle = (whoop_data_handle_t) g_whoop_data.recovery_list[index];
            return WHOOP_DATA_STATUS_OK;
        }
    }
    return WHOOP_DATA_STATUS_ID_NOT_FOUND;
}
int create_whoop_recovery_data(int sleep_id, int cycle_id, whoop_data_handle_t *handle)
{
    whoop_recovery_data_t *recovery_to_write = g_whoop_data.recovery_list[g_recovery_data_record_count % MAX_NUMBER_RECORDINGS];
    memset( recovery_to_write, 0, sizeof(whoop_recovery_data_t) );
    recovery_to_write->recovery_ints.sleep_id = sleep_id;
    recovery_to_write->recovery_ints.cycle_id = cycle_id;
    g_recovery_data_record_count++;
    g_most_recent_recovery = (whoop_data_handle_t) recovery_to_write;
    if(handle) *handle = g_most_recent_recovery;
    return WHOOP_DATA_STATUS_OK;
}


int set_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, ...)
{
    int *int_array_ptr = NULL;
    float *float_array_ptr = NULL;
    int data_is_int = 0;
    int data_array_offset = 0;
    if(deconstruct_whoop_data_opt(handle, whoop_data_opt, &int_array_ptr, &float_array_ptr, &data_is_int, &data_array_offset))
    {
        ESP_LOGI(TAG, "Invalid Data Option: %x ", whoop_data_opt);
        return WHOOP_DATA_STATUS_INVALID_OPTION;
    }
    va_list argptr;
    va_start (argptr, whoop_data_opt);

    if(data_is_int)
    {
        int data_value = va_arg(argptr, int);
        *(int_array_ptr + data_array_offset) = data_value;
    }
    else
    {
        float data_value = (float) va_arg(argptr, double); 
        *(float_array_ptr + data_array_offset) = data_value;
    }
    va_end (argptr);
    return WHOOP_DATA_STATUS_OK;
}

int get_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, void *data_out)
{
    int *int_array_ptr = NULL;
    float *float_array_ptr = NULL;
    int data_is_int = 0;
    int data_array_offset = 0;
    if(deconstruct_whoop_data_opt(handle, whoop_data_opt, &int_array_ptr, &float_array_ptr, &data_is_int, &data_array_offset))
        return WHOOP_DATA_STATUS_INVALID_OPTION;
  
    if(data_is_int)
        *( (int *) data_out ) = *(int_array_ptr + data_array_offset);
    else
        *( (float *) data_out ) = *(float_array_ptr + data_array_offset);

    return WHOOP_DATA_STATUS_OK;
}

void print_whoop_cycle_data(whoop_data_handle_t handle)
{
    whoop_cycle_data_t *sleep = (whoop_cycle_data_t *) handle;
    ESP_LOGI(TAG, "Cycle ID: %d", sleep->cycle_ints.id);
    if(sleep->cycle_ints.score_state == WHOOP_SCORE_STATE_SCORED)
    {
        ESP_LOGI(TAG, "\tAverage Heart Rate: %d", sleep->cycle_ints.average_heart_rate);
        ESP_LOGI(TAG, "\tMax Heart Rate: %d", sleep->cycle_ints.max_heart_rate);
        ESP_LOGI(TAG, "\tStrain: %.2f", sleep->cycle_floats.strain);
        ESP_LOGI(TAG, "\tKilojoule: %.2f", sleep->cycle_floats.kilojoule);
    }
    else
    {
        ESP_LOGI(TAG, "\tCycle not scored.");
    }
}

void print_whoop_workout_data(whoop_data_handle_t handle)
{
    whoop_workout_data_t *workout = (whoop_workout_data_t *) handle;
    ESP_LOGI(TAG, "Workout ID: %d", workout->workout_ints.id);
    ESP_LOGI(TAG, "\tSport ID: %d", workout->workout_ints.sport_id);
    if(workout->workout_ints.score_state == WHOOP_SCORE_STATE_SCORED)
    {
        ESP_LOGI(TAG, "\tAverage Heart Rate: %d", workout->workout_ints.average_heart_rate);
        ESP_LOGI(TAG, "\tMax Heart Rate: %d", workout->workout_ints.max_heart_rate);
        for(int zone = 0; zone < 6; zone++)
        {
            ESP_LOGI(TAG, "\tTime in zone %d [ms]: %d", zone, workout->workout_ints.zone_zero_to_five_time_milli[zone]);
        }
        ESP_LOGI(TAG, "\tStrain: %.2f", workout->workout_floats.strain);
        ESP_LOGI(TAG, "\tKilojoule: %.2f", workout->workout_floats.kilojoule);
        ESP_LOGI(TAG, "\tPercent Recorded: %.2f", workout->workout_floats.percent_recorded);
        ESP_LOGI(TAG, "\tDistance Meter: %.2f", workout->workout_floats.distance_meter);
        ESP_LOGI(TAG, "\tAltitude Gain Meter: %.2f", workout->workout_floats.altitude_gain_meter);
        ESP_LOGI(TAG, "\tAltitude Change Meter: %.2f", workout->workout_floats.altitude_change_meter);
    }
    else
    {
        ESP_LOGI(TAG, "\tCycle not scored.");
    }
}

void print_whoop_sleep_data(whoop_data_handle_t handle)
{
    whoop_sleep_data_t *sleep = (whoop_sleep_data_t *) handle;
    ESP_LOGI(TAG, "Sleep ID: %d", sleep->sleep_ints.id);
    if(sleep->sleep_ints.score_state == WHOOP_SCORE_STATE_SCORED)
    {
        ESP_LOGI(TAG, "\tWas nap: %d", sleep->sleep_ints.nap);
        ESP_LOGI(TAG, "\tTotal time in bed [ms]: %d", sleep->sleep_ints.total_in_bed_time_milli);
        ESP_LOGI(TAG, "\tTotal awake time [ms]: %d", sleep->sleep_ints.total_awake_time_milli);
        ESP_LOGI(TAG, "\tTotal no data time [ms]: %d", sleep->sleep_ints.total_no_data_time_milli);
        ESP_LOGI(TAG, "\tTotal light sleep time [ms]: %d", sleep->sleep_ints.total_light_sleep_time_milli);
        ESP_LOGI(TAG, "\tTotal slow wave time [ms]: %d", sleep->sleep_ints.total_slow_wave_sleep_time_milli);
        ESP_LOGI(TAG, "\tTotal rem time [ms]: %d", sleep->sleep_ints.total_rem_sleep_time_milli);
        ESP_LOGI(TAG, "\tSleep cycle count: %d", sleep->sleep_ints.sleep_cycle_count);
        ESP_LOGI(TAG, "\tDisturbance count: %d", sleep->sleep_ints.disturbance_count);
        ESP_LOGI(TAG, "\tBaseline sleep needed [ms]: %d", sleep->sleep_ints.baseline_milli);
        ESP_LOGI(TAG, "\tNeed from sleep debt [ms]: %d", sleep->sleep_ints.need_from_sleep_debt_milli);
        ESP_LOGI(TAG, "\tNeed from recent strain [ms]: %d", sleep->sleep_ints.need_from_recent_strain_milli);
        ESP_LOGI(TAG, "\tNeed from recent nap [ms]: %d", sleep->sleep_ints.need_from_recent_nap_milli);

        ESP_LOGI(TAG, "\tRespiratory rate: %.2f", sleep->sleep_floats.respiratory_rate);
        ESP_LOGI(TAG, "\tSleep performance percentage: %.2f", sleep->sleep_floats.sleep_performance_percentage);
        ESP_LOGI(TAG, "\tSleep consistency percentage: %.2f", sleep->sleep_floats.sleep_consistency_percentage);
        ESP_LOGI(TAG, "\tSleep efficiency percentage: %.2f", sleep->sleep_floats.sleep_efficiency_percentage);
    }
    else
    {
        ESP_LOGI(TAG, "\tSleep not scored.");
    }
}

void print_whoop_recovery_data(whoop_data_handle_t handle)
{
    whoop_recovery_data_t *recovery = (whoop_recovery_data_t *) handle;
    ESP_LOGI(TAG, "Recovery Sleep ID: %d", recovery->recovery_ints.sleep_id);
    ESP_LOGI(TAG, "Recovery Cycle ID: %d", recovery->recovery_ints.cycle_id);
    if(recovery->recovery_ints.score_state == WHOOP_SCORE_STATE_SCORED)
    {
        ESP_LOGI(TAG, "\tUser calibrating: %d", recovery->recovery_ints.user_calibrating);
        ESP_LOGI(TAG, "\tRecovery score: %.2f", recovery->recovery_floats.recovery_score);
        ESP_LOGI(TAG, "\tResting heart rate: %.2f", recovery->recovery_floats.resting_heart_rate);
        ESP_LOGI(TAG, "\tHRV [ms]: %.2f", recovery->recovery_floats.hrv_rmssd_milli);
        ESP_LOGI(TAG, "\tSP02 percentage: %.2f", recovery->recovery_floats.spo2_percentage);
        ESP_LOGI(TAG, "\tSkin temp [c]: %.2f", recovery->recovery_floats.skin_temp_celsius);
    
    }
    else
    {
        ESP_LOGI(TAG, "\tRecovery not scored.");
    }
}

void print_whoop_data_all(void)
{
    //implement later
}