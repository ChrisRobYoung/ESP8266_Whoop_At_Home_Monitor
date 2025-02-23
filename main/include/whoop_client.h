#ifndef _WHOOP_CLIENT_H_
#define _WHOOP_CLIENT_H_

typedef enum whoop_api_request_type
{
    WHOOP_API_REQUEST_TYPE_SLEEP,
    WHOOP_API_REQUEST_TYPE_WORKOUT,
    WHOOP_API_REQUEST_TYPE_RECOVERY,
    WHOOP_API_REQUEST_TYPE_CYCLE
} whoop_api_request_type_n;

enum token_request_type {
    TOKEN_REQUEST_TYPE_AUTH_CODE = 0,
    TOKEN_REQUEST_TYPE_REFRESH = 1
};

typedef enum whoop_score_state
{
    WHOOP_SCORE_STATE_SCORED,
    WHOOP_SCORE_STATE_PENDING,
    WHOOP_SCORE_STATE_UNSCORABLE
} whoop_score_state_n;

typedef struct whoop_recovery_score{
    int user_calibrating;
    float recovery_score;
    float resting_heart_rate;
    float hrv_rmssd_milli;
    float spo2_percentage;
    float skin_temp_celsius;
} whoop_recover_score_t;

typedef struct whoop_recovery_data {
    int cycle_id;
    int sleep_id;
    whoop_score_state_n score_state;
    whoop_recover_score_t score;
} whoop_recovery_data_t;

typedef struct whoop_cycle_score
{
    float strain;
    float kilojoule;
    int average_heart_rate;
    int max_heart_rate;
} whoop_cycle_score_t;

typedef struct whoop_cycle_data {
    int id;
    whoop_score_state_n score_state;
    whoop_cycle_score_t score;
} whoop_cycle_data_t;

typedef struct whoop_workout_score_zone_duration{
    int zone_zero_to_five_time_milli[6];
} whoop_workout_score_zone_duration_t;

typedef struct whoop_workout_score{
    float strain;
    int average_heart_rate;
    int max_heart_rate;
    float kilojoule;
    float percent_recorded;
    float distance_meter;
    float altitude_gain_meter;
    float altitude_change_meter;
    whoop_workout_score_zone_duration_t zone_duration;
} whoop_workout_score_t;

typedef struct whoop_workout_data {
    int id;
    int sport_id;
    whoop_score_state_n score_state;
    whoop_workout_score_t score;
} whoop_workout_data_t;

typedef struct whoop_sleep_score_stage_summary {
    int total_in_bed_time_milli;
    int total_awake_time_milli;
    int total_no_data_time_milli;
    int total_light_sleep_time_milli;
    int total_slow_wave_sleep_time_milli;
    int total_rem_sleep_time_milli;
    int sleep_cycle_count;
    int disturbance_count;
} whoop_sleep_score_stage_summary_t;

typedef struct whoop_sleep_score_sleep_needed{
    int baseline_milli;
    int need_from_sleep_debt_milli;
    int need_from_recent_strain_milli;
    int need_from_recent_nap_milli;
} whoop_sleep_score_sleep_needed_t;

typedef struct whoop_sleep_score{
    whoop_sleep_score_stage_summary_t stage_summary;
    whoop_sleep_score_sleep_needed_t sleep_needed;
    float respiratory_rate;
    float sleep_performance_percentage;
    float sleep_consistency_percentage;
    float sleep_efficiency_percentage;
} whoop_sleep_score_t;

typedef struct whoop_sleep_data {
    int id;
    int nap;
    whoop_score_state_n score_state;
    whoop_sleep_score_t score;
} whoop_sleep_data_t;

typedef struct whoop_data {
    whoop_recovery_data_t recovery;
    whoop_sleep_data_t sleep;
    whoop_cycle_data_t cycle;
    whoop_workout_data_t workout;
} whoop_data_t;

void print_whoop_data(void);
void whoop_get_token(const char *code_or_token, int token_request_type);
void whoop_get_data(whoop_api_request_type_n request_type);
void init_whoop_tls_client(void);
void end_whoop_tls_client(void);


#endif //_WHOOP_CLIENT_H_