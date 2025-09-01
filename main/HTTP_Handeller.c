#include "HTTP_Handeller.h"
#include "esp_http_server.h"

static const char *TAG = "HTTP";

/* ==================== BASIC AUTH (Optional) ==================== */
#if CONFIG_EXAMPLE_BASIC_AUTH

typedef struct {
    char *username;
    char *password;
} basic_auth_info_t;

#define HTTPD_401 "401 UNAUTHORIZED"

static char *http_auth_basic(const char *username, const char *password)
{
    size_t out_len = 0;
    char *user_info = NULL;
    char *digest = NULL;

    if (asprintf(&user_info, "%s:%s", username, password) < 0 || !user_info) {
        ESP_LOGE(TAG, "Failed to allocate memory for user info");
        return NULL;
    }

    size_t encoded_len = 0;
    esp_crypto_base64_encode(NULL, 0, &encoded_len, (const unsigned char *)user_info, strlen(user_info));
    
    digest = calloc(1, 6 + encoded_len + 1); // "Basic " + base64 + null
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, encoded_len, &out_len,
                                 (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

static esp_err_t basic_auth_get_handler(httpd_req_t *req)
{
    basic_auth_info_t *auth_info = req->user_ctx;
    size_t buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    char *buf = NULL;

    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) return ESP_ERR_NO_MEM;

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) != ESP_OK) {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(auth_info->username, auth_info->password);
        if (!auth_credentials) {
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            char *resp = NULL;
            if (asprintf(&resp, "{\"authenticated\": true,\"user\": \"%s\"}", auth_info->username) < 0 || !resp) {
                free(auth_credentials);
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_send(req, resp, strlen(resp));
            free(resp);
        }
        free(auth_credentials);
        free(buf);
    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

static httpd_uri_t basic_auth_uri = {
    .uri       = "/basic_auth",
    .method    = HTTP_GET,
    .handler   = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server)
{
    basic_auth_info_t *info = calloc(1, sizeof(basic_auth_info_t));
    if (!info) return;

    info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
    info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;

    basic_auth_uri.user_ctx = info;
    httpd_register_uri_handler(server, &basic_auth_uri);
}

#endif // CONFIG_EXAMPLE_BASIC_AUTH

/* ==================== HELLO GET ==================== */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;

    const char *headers[] = {"Host", "Test-Header-1", "Test-Header-2"};
    for (int i = 0; i < sizeof(headers)/sizeof(headers[0]); ++i) {
        buf_len = httpd_req_get_hdr_value_len(req, headers[i]) + 1;
        if (buf_len > 1) {
            buf = malloc(buf_len);
            ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
            if (httpd_req_get_hdr_value_str(req, headers[i], buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found header => %s: %s", headers[i], buf);
            }
            free(buf);
        }
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN], dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            const char *keys[] = {"query1","query2","query3"};
            for (int i = 0; i < 3; i++) {
                if (httpd_query_key_value(buf, keys[i], param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => %s=%s", keys[i], param);
                    example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                    ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
                }
            }
        }
        free(buf);
    }

    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = "Hello World!"
};

/* ==================== ECHO POST ==================== */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "===================================");
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
};

/* ==================== ANY HANDLER ==================== */
static esp_err_t any_handler(httpd_req_t *req)
{
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t any = {
    .uri       = "/any",
    .method    = HTTP_ANY,
    .handler   = any_handler,
    .user_ctx  = "Hello World!"
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    ESP_LOGW("HTTP", "404 error handler triggered, URI: %s", req->uri);
    const char *resp_str = "The requested resource was not found on this server.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ==================== CTRL PUT HANDLER ==================== */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    if (buf == '0') {
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    } else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
};

/* ==================== START/STOP WEBSERVER ==================== */
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

#if CONFIG_IDF_TARGET_LINUX
    config.server_port = 8001; // Non-privileged port for Linux
#endif
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &any);
#if CONFIG_EXAMPLE_ENABLE_SSE_HANDLER
        // SSE handler registration
#endif
#if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
#endif
        return server;
    }

    ESP_LOGE(TAG, "Failed to start server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX
static esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_stop(server);
}

void disconnect_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) *server = NULL;
    }
}

void connect_handler(void* arg, esp_event_base_t event_base,
                     int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
#endif
