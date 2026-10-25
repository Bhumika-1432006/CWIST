/**
 * @file test_otel.c
 * @brief OpenTelemetry middleware: W3C traceparent extraction and the trace id
 *        handlers read through cwist_mw_otel_get_trace_id().
 */
#define _POSIX_C_SOURCE 200809L
#include <cwist/sys/app/app.h>
#include <cwist/sys/app/middleware.h>
#include <cwist/sys/app/test_client.h>
#include <stdio.h>
#include <string.h>

#define FAIL_IF(cond, msg)                      \
    do {                                        \
        if (cond) {                             \
            fprintf(stderr, "FAIL: %s\n", msg); \
            return 1;                           \
        }                                       \
    } while (0)

static void trace_id_handler(cwist_http_request *req, cwist_http_response *res) {
    const char *id = cwist_mw_otel_get_trace_id(req);
    cwist_sstring_assign(res->body, id ? id : "none");
    res->status_code = CWIST_HTTP_OK;
}

static int is_lower_hex(const char *s, size_t len) {
    if (strlen(s) != len) return 0;
    for (size_t i = 0; i < len; i++) {
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
    }
    return 1;
}

/* GET / with an optional traceparent header; copies the response body. */
static int get_trace_id(cwist_test_client *client, const char *traceparent, char *out,
                        size_t cap) {
    cwist_test_client_kv hdr = {"traceparent", traceparent};
    cwist_test_client_request_options opts = {
        .headers = traceparent ? &hdr : NULL,
        .header_count = traceparent ? 1 : 0,
    };
    cwist_http_response *res = cwist_test_client_request_ex(client, CWIST_HTTP_GET, "/", &opts);
    if (!res) return -1;
    int ok = res->status_code == CWIST_HTTP_OK && res->body && res->body->data;
    if (ok) snprintf(out, cap, "%s", res->body->data);
    cwist_http_response_destroy(res);
    return ok ? 0 : -1;
}

int main(void) {
    cwist_app *app = cwist_app_create();
    FAIL_IF(!app, "app create");
    cwist_middleware_func mw = cwist_mw_otel_trace(NULL);
    FAIL_IF(!mw, "middleware create");
    cwist_app_use(app, mw);
    cwist_app_get(app, "/", trace_id_handler);

    cwist_test_client *client = cwist_test_client_create(app);
    FAIL_IF(!client, "client create");

    char id[64];
    FAIL_IF(get_trace_id(client, "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01", id,
                         sizeof(id)) != 0,
            "request with traceparent");
    FAIL_IF(strcmp(id, "4bf92f3577b34da6a3ce929d0e0e4736") != 0, "trace id from traceparent");

    FAIL_IF(get_trace_id(client, NULL, id, sizeof(id)) != 0, "request without traceparent");
    FAIL_IF(!is_lower_hex(id, 32), "fresh 128-bit trace id without traceparent");

    cwist_test_client_destroy(client);
    cwist_app_destroy(app);
    printf("test_otel: OK\n");
    return 0;
}
