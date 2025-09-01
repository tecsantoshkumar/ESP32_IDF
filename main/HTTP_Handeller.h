#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_tls_crypto.h"
#include "sdkconfig.h"
#include "esp_check.h"        // for ESP_RETURN_ON_FALSE
#include <sys/param.h>        // for MIN macro
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"   // declares example_uri_decode()

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN 64

// Function prototypes
httpd_handle_t start_webserver(void);

#if !CONFIG_IDF_TARGET_LINUX
void disconnect_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

void connect_handler(void* arg, esp_event_base_t event_base,
                     int32_t event_id, void* event_data);

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
#endif

#ifdef __cplusplus
}
#endif

#endif // HTTP_HANDLER_H
