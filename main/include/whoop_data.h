#ifndef _WHOOP_DATA_H_
#define _WHOOP_DATA_H_

typedef void * whoop_data_handle_t;

typedef enum whoop_data_status
{
    WHOOP_DATA_STATUS_OK =                      0,

    WHOOP_DATA_STATUS_ID_NOT_FOUND =            -100,
    WHOOP_DATA_STATUS_NO_RECORDINGS,
    WHOOP_DATA_STATUS_INVALID_OPTION
} whoop_data_status_n;


typedef enum whoop_score_state
{
    WHOOP_SCORE_STATE_SCORED,
    WHOOP_SCORE_STATE_PENDING,
    WHOOP_SCORE_STATE_UNSCORABLE
} whoop_score_state_n;

typedef enum whoop_data_opt
{
    WHOOP_DATA_OPT_SLEEP_ID =                                               0x1000,
    WHOOP_DATA_OPT_SLEEP_SCORE_STATE =                                      0x1001,
    WHOOP_DATA_OPT_SLEEP_NAP_BOOL =                                         0x1002,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_IN_BED_TIME_MILLI =            0x1003,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_AWAKE_TIME_MILLI =             0x1004,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_NO_DATA_TIME_MILLI =           0x1005,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_LIGHT_SLEEP_TIME_MILLI =       0x1006,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_SLOW_WAVE_TIME_MILLI =         0x1007,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_TOTAL_REM_SLEEP_TIME_MILLI =         0x1008,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_SLEEP_CYCLE_COUNT =                  0x1009,
    WHOOP_DATA_OPT_SLEEP_STAGE_SUMMARY_DISTURBANCE_COUNT =                  0x100a,
    WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_BASELINE_MILLI =                      0x100b,
    WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_SLEEP_DEBT_MILLI =               0x100c,
    WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_RECENT_STRAIN_DEBT_MILLI =       0x100d,
    WHOOP_DATA_OPT_SLEEP_SLEEP_NEEDED_FROM_RECENT_NAP_DEBT_MILLI =          0x100e,
    WHOOP_DATA_OPT_SLEEP_RESPIRATORY_RATE =                                 0x1100,
    WHOOP_DATA_OPT_SLEEP_SLEEP_PERFORMANCE_PERCENTAGE =                     0x1101,
    WHOOP_DATA_OPT_SLEEP_SLEEP_CONSISTENCY_PERCENTAGE =                     0x1102,
    WHOOP_DATA_OPT_SLEEP_SLEEP_EFFICIENCY_PERCENTAGE =                      0x1103,

    WHOOP_DATA_OPT_CYCLE_ID =                                               0x2000,
    WHOOP_DATA_OPT_CYCLE_SCORE_STATE =                                      0x2001,
    WHOOP_DATA_OPT_CYCLE_AVERAGE_HEART_RATE =                               0x2002,
    WHOOP_DATA_OPT_CYCLE_MAX_HEART_RATE =                                   0x2003,
    WHOOP_DATA_OPT_CYCLE_STRAIN =                                           0x2100,
    WHOOP_DATA_OPT_CYCLE_KILOJOULE =                                        0x2101,

    WHOOP_DATA_OPT_WORKOUT_ID =                                             0x4000,
    WHOOP_DATA_OPT_WORKOUT_SCORE_STATE =                                    0x4001,
    WHOOP_DATA_OPT_WORKOUT_SPORT_ID =                                       0x4002,
    WHOOP_DATA_OPT_WORKOUT_AVERAGE_HEART_RATE =                             0x4003,
    WHOOP_DATA_OPT_WORKOUT_MAX_HEART_RATE =                                 0x4004,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_ZERO =                             0x4005,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_ONE =                              0x4006,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_TWO =                              0x4007,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_THREE =                            0x4008,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_FOUR =                             0x4009,
    WHOOP_DATA_OPT_WORKOUT_ZONE_DURATION_FIVE =                             0x400a,
    WHOOP_DATA_OPT_WORKOUT_STRAIN =                                         0x4100,
    WHOOP_DATA_OPT_WORKOUT_KILOJOULE =                                      0x4101,
    WHOOP_DATA_OPT_WORKOUT_PERCENT_RECORDED =                               0x4102,
    WHOOP_DATA_OPT_WORKOUT_DISTANCE_METER =                                 0x4103,
    WHOOP_DATA_OPT_WORKOUT_ALTITUDE_GAIN_METER =                            0x4104,
    WHOOP_DATA_OPT_WORKOUT_ALTITUDE_CHANGE_METER =                          0x4105,



    WHOOP_DATA_OPT_RECOVERY_CYCLE_ID =                                       0x8000,
    WHOOP_DATA_OPT_RECOVERY_SLEEP_ID =                                       0x8001,
    WHOOP_DATA_OPT_RECOVERY_SCORE_STATE =                                    0x8002,
    WHOOP_DATA_OPT_RECOVERY_USER_CALIBRATING =                               0x8003,
    WHOOP_DATA_OPT_RECOVERY_RECOVERY_SCORE =                                 0x8100,
    WHOOP_DATA_OPT_RECOVERY_RESTING_HEART_RATE =                             0x8101,
    WHOOP_DATA_OPT_RECOVERY_HRV_RMSSD_MILLI =                                0x8102,
    WHOOP_DATA_OPT_RECOVERY_SPO2_PERCENTAGE =                                0x8103,
    WHOOP_DATA_OPT_RECOVERY_SKIN_TEMP_CELCIUS =                             0x8104
} whoop_data_opt_n;

int init_whoop_data(void);
int discard_whoop_data(void);

/*Sleep ID or 0 for most recent*/
int get_whoop_sleep_handle_by_id(int id, whoop_data_handle_t *handle);
int create_whoop_sleep_data(int id, whoop_data_handle_t *handle);

/*Cycle ID or 0 for most recent*/
int get_whoop_cycle_handle_by_id(int id, whoop_data_handle_t *handle);
int create_whoop_cycle_data(int id, whoop_data_handle_t *handle);

/*Workout ID or 0 for most recent*/
int get_whoop_workout_handle_by_id(int id, whoop_data_handle_t *handle);
int create_whoop_workout_data(int id, whoop_data_handle_t *handle);

/*ID can be either sleep or cycle id related to recovery or 0 for most recent*/
int get_whoop_recovery_handle_by_id(int id, whoop_data_handle_t *handle);
int create_whoop_recovery_data(int sleep_id, int cycle_id, whoop_data_handle_t *handle);


int set_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, ...);
int get_whoop_data(whoop_data_handle_t handle, whoop_data_opt_n whoop_data_opt, void *data_out);

void print_whoop_cycle_data(whoop_data_handle_t handle);
void print_whoop_sleep_data(whoop_data_handle_t handle);
void print_whoop_recovery_data(whoop_data_handle_t handle);
void print_whoop_workout_data(whoop_data_handle_t handle);
void print_whoop_data_all(void);

#endif //_WHOOP_DATA_H_