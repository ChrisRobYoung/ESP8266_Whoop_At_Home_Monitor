#include <string.h>
#include "whoop_data.h"

// Defines
#define MAX_NUMBER_RECORDINGS 5

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
    int n_cycle_data_recorded;
    whoop_cycle_data_t **cycle_list;
    int n_workout_data_recorded;
    whoop_workout_data_t **workout_list;
    int n_recovery_data_recorded;
    whoop_recovery_data_t **recovery_list;
    int n_sleep_data_recorded;
    whoop_sleep_data_t **sleep_list;
} whoop_data_t;

// Local Global Variables
whoop_sleep_data_t g_sleep_data_list[MAX_NUMBER_RECORDINGS];
whoop_cycle_data_t g_cycle_data_list[MAX_NUMBER_RECORDINGS];
whoop_workout_data_t g_workout_data_list[MAX_NUMBER_RECORDINGS];
whoop_recovery_data_t g_recovery_data_list[MAX_NUMBER_RECORDINGS];

whoop_sleep_data_t *g_sleep_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_cycle_data_t *g_cycle_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_workout_data_t *g_workout_data_ptr_list[MAX_NUMBER_RECORDINGS];
whoop_recovery_data_t *g_recovery_data_ptr_list[MAX_NUMBER_RECORDINGS];

whoop_data_t g_whoop_data;

// Local functions
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

// Global functions
int init_whoop_data(void)
{
    memset(g_sleep_data_ptr_list, 0, sizeof(whoop_sleep_data_t *) *MAX_NUMBER_RECORDINGS);
    memset(g_cycle_data_ptr_list, 0, sizeof(whoop_cycle_data_t *) * MAX_NUMBER_RECORDINGS);
    memset(g_workout_data_ptr_list, 0, sizeof(whoop_workout_data_t *) * MAX_NUMBER_RECORDINGS);
    memset(g_recovery_data_ptr_list, 0, sizeof(whoop_recovery_data_t *) * MAX_NUMBER_RECORDINGS);

    memset(g_sleep_data_list, 0, sizeof(whoop_sleep_data_t) *MAX_NUMBER_RECORDINGS);
    memset(g_cycle_data_list, 0, sizeof(whoop_cycle_data_t) * MAX_NUMBER_RECORDINGS);
    memset(g_workout_data_list, 0, sizeof(whoop_workout_data_t) * MAX_NUMBER_RECORDINGS);
    memset(g_recovery_data_list, 0, sizeof(whoop_recovery_data_t) * MAX_NUMBER_RECORDINGS);

    g_whoop_data.n_cycle_data_recorded = 0;
    g_whoop_data.n_recovery_data_recorded = 0;
    g_whoop_data.n_sleep_data_recorded = 0;
    g_whoop_data.n_workout_data_recorded = 0;
    g_whoop_data.recovery_list = g_recovery_data_ptr_list;
    g_whoop_data.sleep_list = g_sleep_data_ptr_list;
    g_whoop_data.cycle_list = g_cycle_data_ptr_list;
    g_whoop_data.workout_list = g_workout_data_ptr_list;
}
int discard_whoop_data(void)
{   
    int index;
    for(index = 0; index <g_whoop_data.n_cycle_data_recorded; index++)  discard_cycle_data(g_whoop_data.cycle_list[index]);
    for(index = 0; index <g_whoop_data.n_recovery_data_recorded; index++)  discard_recovery_data(g_whoop_data.recovery_list[index]);    
    for(index = 0; index <g_whoop_data.n_sleep_data_recorded; index++)  discard_sleep_data(g_whoop_data.sleep_list[index]);    
    for(index = 0; index <g_whoop_data.n_workout_data_recorded; index++)  discard_workout_data(g_whoop_data.workout_list[index]);        
}

/*Sleep ID or 0 for most recent*/
int get_whoop_sleep_handle_by_id(int id, whoop_data_handle_t *handle)
{}
int create_whoop_sleep_data(int id, whoop_data_handle_t *handle)
{}

/*Cycle ID or 0 for most recent*/
int get_whoop_cycle_handle_by_id(int id, whoop_data_handle_t *handle)
{}
int create_whoop_cycle_data(int id, whoop_data_handle_t *handle)
{}

/*Workout ID or 0 for most recent*/
int get_whoop_workout_handle_by_id(int id, whoop_data_handle_t *handle)
{}
int create_whoop_workout_data(int id, whoop_data_handle_t *handle)
{}

/*ID can be either sleep or cycle id related to recovery or 0 for most recent*/
int get_whoop_recovery_handle_by_id(int id, whoop_data_handle_t *handle)
{}
int create_whoop_recovery_data(int sleep_id, int cycle_id, whoop_data_handle_t *handle)
{}


int set_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, ...)
{}
int get_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, void *data_out)
{}