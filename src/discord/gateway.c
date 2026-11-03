#include "gateway.h"
#include "state.h"
#include "../../lib/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

static HANDLE g_gateway_thread = NULL;
static HANDLE g_heartbeat_thread = NULL;
static volatile bool g_running = false;
static HINTERNET g_hWebSocket = NULL;
static int g_last_sequence = -1;
static int g_heartbeat_interval_ms = 41250;
static char g_token[256];
static DiscordState *g_state = NULL;

static void send_ws_text(HINTERNET hWs, const char *text) {
    if (!hWs || !text) return;
    WinHttpWebSocketSend(hWs, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                         (PVOID)text, (DWORD)strlen(text));
}

static DWORD WINAPI heartbeat_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    while (g_running) {
        Sleep(g_heartbeat_interval_ms > 0 ? g_heartbeat_interval_ms : 40000);
        if (!g_running || !g_hWebSocket) break;

        char hb_payload[64];
        if (g_last_sequence >= 0) {
            snprintf(hb_payload, sizeof(hb_payload), "{\"op\":1,\"d\":%d}", g_last_sequence);
        } else {
            snprintf(hb_payload, sizeof(hb_payload), "{\"op\":1,\"d\":null}");
        }
        send_ws_text(g_hWebSocket, hb_payload);
    }
    return 0;
}

static void send_identify(HINTERNET hWs, const char *token) {
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "op", 2);

    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "token", token);
    cJSON_AddNumberToObject(d, "capabilities", 16381);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "os", "Windows");
    cJSON_AddStringToObject(props, "browser", "Discord Client");
    cJSON_AddStringToObject(props, "release_channel", "stable");
    cJSON_AddStringToObject(props, "client_version", "1.0.9168");
    cJSON_AddStringToObject(props, "os_version", "10.0.19045");
    cJSON_AddStringToObject(props, "os_arch", "x64");
    cJSON_AddStringToObject(props, "system_locale", "en-US");
    cJSON_AddItemToObject(d, "properties", props);

    cJSON *presence = cJSON_CreateObject();
    cJSON_AddStringToObject(presence, "status", "online");
    cJSON_AddNumberToObject(presence, "since", 0);
    cJSON_AddItemToObject(presence, "activities", cJSON_CreateArray());
    cJSON_AddBoolToObject(presence, "afk", false);
    cJSON_AddItemToObject(d, "presence", presence);

    cJSON_AddBoolToObject(d, "compress", false);

    cJSON *cstate = cJSON_CreateObject();
    cJSON_AddItemToObject(cstate, "guild_versions", cJSON_CreateObject());
    cJSON_AddStringToObject(cstate, "highest_last_message_id", "0");
    cJSON_AddNumberToObject(cstate, "read_state_version", 0);
    cJSON_AddNumberToObject(cstate, "user_guild_settings_version", -1);
    cJSON_AddNumberToObject(cstate, "user_settings_version", -1);
    cJSON_AddItemToObject(d, "client_state", cstate);

    cJSON_AddItemToObject(payload, "d", d);

    char *json_str = cJSON_PrintUnformatted(payload);
    if (json_str) {
        send_ws_text(hWs, json_str);
        free(json_str);
    }
    cJSON_Delete(payload);
}

static void handle_gateway_message(const char *msg, DiscordState *state) {
    if (!msg || !state) return;

    cJSON *root = cJSON_Parse(msg);
    if (!root) return;

    cJSON *op = cJSON_GetObjectItemCaseSensitive(root, "op");
    cJSON *s = cJSON_GetObjectItemCaseSensitive(root, "s");
    cJSON *t = cJSON_GetObjectItemCaseSensitive(root, "t");

    if (cJSON_IsNumber(s)) {
        g_last_sequence = s->valueint;
    }

    int opcode = cJSON_IsNumber(op) ? op->valueint : -1;

    switch (opcode) {
        case 10: { // HELLO
            cJSON *d = cJSON_GetObjectItemCaseSensitive(root, "d");
            if (d) {
                cJSON *hb_int = cJSON_GetObjectItemCaseSensitive(d, "heartbeat_interval");
                if (cJSON_IsNumber(hb_int)) {
                    g_heartbeat_interval_ms = hb_int->valueint;
                }
            }

            // Immediately send heartbeat 1
            char first_hb[64];
            snprintf(first_hb, sizeof(first_hb), "{\"op\":1,\"d\":null}");
            send_ws_text(g_hWebSocket, first_hb);

            // Start background heartbeat loop
            if (!g_heartbeat_thread) {
                g_heartbeat_thread = CreateThread(NULL, 0, heartbeat_thread_proc, NULL, 0, NULL);
            }

            // Send identify
            send_identify(g_hWebSocket, g_token);
            break;
        }

        case 11: // HEARTBEAT_ACK
            break;

        case 0: { // DISPATCH
            if (cJSON_IsString(t) && t->valuestring) {
                if (strcmp(t->valuestring, "READY") == 0) {
                    discord_state_parse_ready(state, msg);
                } else if (strcmp(t->valuestring, "MESSAGE_CREATE") == 0) {
                    discord_state_handle_message_create(state, msg);
                } else if (strcmp(t->valuestring, "MESSAGE_ACK") == 0) {
                    discord_state_handle_message_ack(state, msg);
                }
            }
            break;
        }

        case 1: { // Discord requested instant Heartbeat
            char instant_hb[64];
            if (g_last_sequence >= 0) {
                snprintf(instant_hb, sizeof(instant_hb), "{\"op\":1,\"d\":%d}", g_last_sequence);
            } else {
                snprintf(instant_hb, sizeof(instant_hb), "{\"op\":1,\"d\":null}");
            }
            send_ws_text(g_hWebSocket, instant_hb);
            break;
        }

        case 7: // RECONNECT
        case 9: // INVALID_SESSION
            // In a simple CLI, we can signal reconnect
            break;

        default:
            break;
    }

    cJSON_Delete(root);
}

static DWORD WINAPI gateway_thread_proc(LPVOID lpParam) {
    (void)lpParam;

    HINTERNET hSession = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) DiscordCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return 1;

    HINTERNET hConnect = WinHttpConnect(hSession, L"gateway.discord.gg", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return 1;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        L"/?v=9&encoding=json",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    BOOL opt = WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    if (!opt) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    g_hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
    WinHttpCloseHandle(hRequest);

    if (!g_hWebSocket) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    // Accumulator buffer for assembling multi-fragment WebSocket frames
    size_t buffer_capacity = 65536;
    size_t message_len = 0;
    char *message_buffer = (char *)malloc(buffer_capacity);

    BYTE chunk[8192];
    DWORD bytes_read = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE buffer_type;

    while (g_running) {
        DWORD dwError = WinHttpWebSocketReceive(g_hWebSocket, chunk, sizeof(chunk), &bytes_read, &buffer_type);
        if (dwError != ERROR_SUCCESS || bytes_read == 0) {
            break;
        }

        if (buffer_type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            break;
        }

        // Expand buffer if needed
        if (message_len + bytes_read + 1 > buffer_capacity) {
            buffer_capacity = (message_len + bytes_read + 1) * 2;
            char *new_buf = (char *)realloc(message_buffer, buffer_capacity);
            if (!new_buf) break;
            message_buffer = new_buf;
        }

        memcpy(message_buffer + message_len, chunk, bytes_read);
        message_len += bytes_read;

        // If this is the final fragment of the message, process it
        if (buffer_type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
            buffer_type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            message_buffer[message_len] = '\0';
            handle_gateway_message(message_buffer, g_state);
            message_len = 0;
        }
    }

    if (message_buffer) free(message_buffer);

    if (g_hWebSocket) {
        WinHttpWebSocketClose(g_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
        WinHttpCloseHandle(g_hWebSocket);
        g_hWebSocket = NULL;
    }

    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
}

#else
// Linux stub
static DWORD WINAPI gateway_thread_proc(LPVOID lpParam) { (void)lpParam; return 0; }
#endif

bool discord_gateway_start(const char *token, DiscordState *state) {
    if (!token || !state) return false;
    strncpy_s(g_token, sizeof(g_token), token, _TRUNCATE);
    g_state = state;
    g_running = true;

#ifdef _WIN32
    g_gateway_thread = CreateThread(NULL, 0, gateway_thread_proc, NULL, 0, NULL);
    return g_gateway_thread != NULL;
#else
    return false;
#endif
}

void discord_gateway_stop(void) {
    g_running = false;
#ifdef _WIN32
    if (g_hWebSocket) {
        WinHttpWebSocketClose(g_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
    }
    if (g_gateway_thread) {
        WaitForSingleObject(g_gateway_thread, 2000);
        CloseHandle(g_gateway_thread);
        g_gateway_thread = NULL;
    }
    if (g_heartbeat_thread) {
        WaitForSingleObject(g_heartbeat_thread, 2000);
        CloseHandle(g_heartbeat_thread);
        g_heartbeat_thread = NULL;
    }
#endif
}
