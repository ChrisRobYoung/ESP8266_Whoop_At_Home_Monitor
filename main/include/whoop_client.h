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

//void print_whoop_data_old(void);
void whoop_get_token(const char *code_or_token, int token_request_type);
void whoop_get_data(whoop_api_request_type_n request_type);
void init_whoop_tls_client(void);
void end_whoop_tls_client(void);


#endif //_WHOOP_CLIENT_H_