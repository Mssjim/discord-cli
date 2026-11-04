#include "api.h"
#include "../../lib/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

static bool http_request_winhttp(
    const char *method,
    const char *path,
    const char *token,
    const char *body,
    char **out_response,
    int *out_status_code
) {
    if (out_response) *out_response = NULL;
    if (out_status_code) *out_status_code = 0;

    HINTERNET hSession = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) DiscordCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"discord.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Convert method and path to wide char
    wchar_t wMethod[16];
    wchar_t wPath[512];
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wMethod, 16);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, 512);

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        wMethod,
        wPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Prepare headers
    char headers_a[512];
    snprintf(headers_a, sizeof(headers_a),
             "Authorization: %s\r\nContent-Type: application/json\r\n", token);
    wchar_t headers_w[512];
    MultiByteToWideChar(CP_UTF8, 0, headers_a, -1, headers_w, 512);

    DWORD body_len = body ? (DWORD)strlen(body) : 0;
    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers_w,
        -1L,
        body ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA,
        body_len,
        body_len,
        0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (!bResults) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Status code
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
    if (out_status_code) *out_status_code = (int)statusCode;

    // Read response body if requested
    if (out_response) {
        size_t total_alloc = 4096;
        size_t total_read = 0;
        char *buffer = (char *)malloc(total_alloc);
        if (buffer) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                if (total_read + dwSize + 1 > total_alloc) {
                    total_alloc = (total_read + dwSize + 1) * 2;
                    char *new_buf = (char *)realloc(buffer, total_alloc);
                    if (!new_buf) {
                        free(buffer);
                        buffer = NULL;
                        break;
                    }
                    buffer = new_buf;
                }

                if (WinHttpReadData(hRequest, buffer + total_read, dwSize, &dwDownloaded)) {
                    total_read += dwDownloaded;
                } else {
                    break;
                }
            } while (dwSize > 0);

            if (buffer) {
                buffer[total_read] = '\0';
                *out_response = buffer;
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

#else
// Linux / POSIX fallback stub
static bool http_request_winhttp(
    const char *method,
    const char *path,
    const char *token,
    const char *body,
    char **out_response,
    int *out_status_code
) {
    if (out_response) *out_response = NULL;
    if (out_status_code) *out_status_code = 501;
    return false;
}
#endif

bool discord_api_fetch_messages(const char *token, const char *channel_id, int limit, DiscordMessage *out_msgs, int *out_count) {
    if (!token || !channel_id || !out_msgs || !out_count) return false;
    *out_count = 0;

    char path[256];
    snprintf(path, sizeof(path), "/api/v9/channels/%s/messages?limit=%d", channel_id, (limit > 0 ? limit : 50));

    char *response = NULL;
    int status = 0;
    if (!http_request_winhttp("GET", path, token, NULL, &response, &status) || !response) {
        if (response) free(response);
        return false;
    }

    if (status < 200 || status >= 300) {
        free(response);
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(root);
    if (count > MAX_CACHED_MESSAGES) count = MAX_CACHED_MESSAGES;

    int out_idx = 0;
    for (int i = count - 1; i >= 0; i--) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item) continue;

        cJSON *mid = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *content = cJSON_GetObjectItemCaseSensitive(item, "content");
        cJSON *author = cJSON_GetObjectItemCaseSensitive(item, "author");
        cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(item, "timestamp");

        DiscordMessage *msg = &out_msgs[out_idx++];
        memset(msg, 0, sizeof(DiscordMessage));

        if (cJSON_IsString(mid)) strncpy_s(msg->id, sizeof(msg->id), mid->valuestring, _TRUNCATE);
        if (cJSON_IsString(content)) strncpy_s(msg->content, sizeof(msg->content), content->valuestring, _TRUNCATE);

        if (author) {
            cJSON *aid = cJSON_GetObjectItemCaseSensitive(author, "id");
            cJSON *aname = cJSON_GetObjectItemCaseSensitive(author, "username");
            cJSON *agname = cJSON_GetObjectItemCaseSensitive(author, "global_name");

            if (cJSON_IsString(aid)) strncpy_s(msg->author_id, sizeof(msg->author_id), aid->valuestring, _TRUNCATE);
            if (cJSON_IsString(aname)) strncpy_s(msg->author_name, sizeof(msg->author_name), aname->valuestring, _TRUNCATE);
            if (cJSON_IsString(agname) && agname->valuestring) {
                strncpy_s(msg->author_global_name, sizeof(msg->author_global_name), agname->valuestring, _TRUNCATE);
            } else {
                strncpy_s(msg->author_global_name, sizeof(msg->author_global_name), msg->author_name, _TRUNCATE);
            }
        }

        if (cJSON_IsString(timestamp) && strlen(timestamp->valuestring) >= 16) {
            const char *t = strchr(timestamp->valuestring, 'T');
            if (t && strlen(t) >= 6) {
                snprintf(msg->timestamp, sizeof(msg->timestamp), "%.5s", t + 1);
            } else {
                strncpy_s(msg->timestamp, sizeof(msg->timestamp), "--:--", _TRUNCATE);
            }
        } else {
            strncpy_s(msg->timestamp, sizeof(msg->timestamp), "--:--", _TRUNCATE);
        }
    }

    *out_count = out_idx;
    cJSON_Delete(root);
    return true;
}

bool discord_api_send_message(const char *token, const char *channel_id, const char *content) {
    if (!token || !channel_id || !content || strlen(content) == 0) return false;

    char path[256];
    snprintf(path, sizeof(path), "/api/v9/channels/%s/messages", channel_id);

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "content", content);
    char *json_body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    if (!json_body) return false;

    int status = 0;
    char *response = NULL;
    bool ok = http_request_winhttp("POST", path, token, json_body, &response, &status);

    free(json_body);
    if (response) free(response);

    return ok && (status >= 200 && status < 300);
}

bool discord_api_ack_message(const char *token, const char *channel_id, const char *message_id) {
    if (!token || !channel_id || !message_id || strlen(message_id) == 0) return false;

    char path[256];
    snprintf(path, sizeof(path), "/api/v9/channels/%s/messages/%s/ack", channel_id, message_id);

    const char *body = "{\"token\":null}";
    int status = 0;
    char *response = NULL;
    bool ok = http_request_winhttp("POST", path, token, body, &response, &status);
    if (response) free(response);

    return ok && (status >= 200 && status < 300);
}

bool discord_api_fetch_guilds(const char *token, DiscordGuild *out_guilds, int max_guilds, int *out_count) {
    if (!token || !out_guilds || !out_count) return false;
    *out_count = 0;

    char *response = NULL;
    int status = 0;
    if (!http_request_winhttp("GET", "/api/v9/users/@me/guilds", token, NULL, &response, &status) || !response) {
        if (response) free(response);
        return false;
    }

    if (status < 200 || status >= 300) {
        free(response);
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(root);
    if (count > max_guilds) count = max_guilds;

    int out_idx = 0;
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item) continue;

        cJSON *gid = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *gname = cJSON_GetObjectItemCaseSensitive(item, "name");

        if (!cJSON_IsString(gid)) continue;

        DiscordGuild *g = &out_guilds[out_idx++];
        memset(g, 0, sizeof(DiscordGuild));
        strncpy_s(g->id, sizeof(g->id), gid->valuestring, _TRUNCATE);
        if (cJSON_IsString(gname)) {
            strncpy_s(g->name, sizeof(g->name), gname->valuestring, _TRUNCATE);
        }
    }

    *out_count = out_idx;
    cJSON_Delete(root);
    return true;
}

bool discord_api_fetch_dms(const char *token, DiscordDM *out_dms, int max_dms, int *out_count) {
    if (!token || !out_dms || !out_count) return false;
    *out_count = 0;

    char *response = NULL;
    int status = 0;
    if (!http_request_winhttp("GET", "/api/v9/users/@me/channels", token, NULL, &response, &status) || !response) {
        if (response) free(response);
        return false;
    }

    if (status < 200 || status >= 300) {
        free(response);
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(root);
    if (count > max_dms) count = max_dms;

    int out_idx = 0;
    for (int i = 0; i < count; i++) {
        cJSON *chan = cJSON_GetArrayItem(root, i);
        if (!chan) continue;

        cJSON *cid = cJSON_GetObjectItemCaseSensitive(chan, "id");
        cJSON *last_msg = cJSON_GetObjectItemCaseSensitive(chan, "last_message_id");
        cJSON *recipients = cJSON_GetObjectItemCaseSensitive(chan, "recipients");
        cJSON *cname = cJSON_GetObjectItemCaseSensitive(chan, "name");

        if (!cJSON_IsString(cid)) continue;

        DiscordDM *dm = &out_dms[out_idx++];
        memset(dm, 0, sizeof(DiscordDM));
        strncpy_s(dm->id, sizeof(dm->id), cid->valuestring, _TRUNCATE);

        if (cJSON_IsString(last_msg)) {
            strncpy_s(dm->last_message_id, sizeof(dm->last_message_id), last_msg->valuestring, _TRUNCATE);
        }

        int num_recipients = (cJSON_IsArray(recipients)) ? cJSON_GetArraySize(recipients) : 0;
        bool has_custom_name = (cJSON_IsString(cname) && cname->valuestring && strlen(cname->valuestring) > 0);

        if (num_recipients > 0) {
            if (has_custom_name) {
                strncpy_s(dm->display_name, sizeof(dm->display_name), cname->valuestring, _TRUNCATE);
            } else {
                dm->display_name[0] = '\0';
            }

            dm->username[0] = '\0';
            for (int r = 0; r < num_recipients; r++) {
                cJSON *rec = cJSON_GetArrayItem(recipients, r);
                if (!rec) continue;
                cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
                cJSON *rname = cJSON_GetObjectItemCaseSensitive(rec, "username");
                cJSON *rgname = cJSON_GetObjectItemCaseSensitive(rec, "global_name");

                if (r == 0 && cJSON_IsString(rid)) {
                    strncpy_s(dm->recipient_id, sizeof(dm->recipient_id), rid->valuestring, _TRUNCATE);
                }

                const char *u = cJSON_IsString(rname) ? rname->valuestring : "";
                const char *g = (cJSON_IsString(rgname) && rgname->valuestring && strlen(rgname->valuestring) > 0) ? rgname->valuestring : u;

                if (!has_custom_name) {
                    if (r > 0 && strlen(dm->display_name) > 0) {
                        strncat(dm->display_name, ", ", sizeof(dm->display_name) - strlen(dm->display_name) - 1);
                    }
                    strncat(dm->display_name, g, sizeof(dm->display_name) - strlen(dm->display_name) - 1);
                }

                if (strlen(u) > 0) {
                    if (r > 0 && strlen(dm->username) > 0) {
                        strncat(dm->username, ", ", sizeof(dm->username) - strlen(dm->username) - 1);
                    }
                    strncat(dm->username, u, sizeof(dm->username) - strlen(dm->username) - 1);
                }
            }
        } else if (has_custom_name) {
            strncpy_s(dm->display_name, sizeof(dm->display_name), cname->valuestring, _TRUNCATE);
            strncpy_s(dm->username, sizeof(dm->username), "group", _TRUNCATE);
        } else {
            strncpy_s(dm->display_name, sizeof(dm->display_name), "Group DM", _TRUNCATE);
            strncpy_s(dm->username, sizeof(dm->username), "group", _TRUNCATE);
        }
    }

    *out_count = out_idx;
    cJSON_Delete(root);
    return true;
}