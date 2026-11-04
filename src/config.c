#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#endif

static void trim_whitespace(char *str) {
    if (!str) return;
    
    char *start = str;
    while (*start && (isspace((unsigned char)*start) || *start == '"' || *start == '\'')) {
        start++;
    }
    
    char *end = start + strlen(start);
    while (end > start && (isspace((unsigned char)*(end - 1)) || *(end - 1) == '"' || *(end - 1) == '\'')) {
        end--;
    }
    *end = '\0';
    
    if (start != str) {
        memmove(str, start, end - start + 1);
    }
}

static bool read_token_from_env_file(const char *filepath, char *token_out, size_t max_len) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return false;

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim_whitespace(key);
        trim_whitespace(val);

        if (_stricmp(key, "DISCORD_TOKEN") == 0 && strlen(val) > 0) {
            strncpy_s(token_out, max_len, val, _TRUNCATE);
            found = true;
            break;
        }
    }
    fclose(fp);
    return found;
}

static bool save_token_to_env_file(const char *filepath, const char *token) {
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filepath);
        return false;
    }
    fprintf(fp, "# Discord CLI Configuration\n");
    fprintf(fp, "DISCORD_TOKEN=%s\n", token);
    fclose(fp);
    return true;
}

#ifdef _WIN32
static void paste_from_clipboard(char *buffer, size_t *idx, size_t max_len) {
    if (!OpenClipboard(NULL)) return;

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char *clip_text = (char *)GlobalLock(hData);
        if (clip_text) {
            for (size_t i = 0; clip_text[i] != '\0'; i++) {
                char c = clip_text[i];
                if (c == '\r' || c == '\n') break;
                if ((unsigned char)c >= 32 && *idx < max_len - 1) {
                    buffer[(*idx)++] = c;
                    putchar('*');
                }
            }
            fflush(stdout);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}
#endif

static void read_masked_input(char *buffer, size_t max_len) {
    size_t idx = 0;
    while (1) {
        int ch = _getch();
        if (ch == 13 || ch == 10) { // Enter
            buffer[idx] = '\0';
            printf("\n");
            break;
        } else if (ch == 8) { // Backspace
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (ch == 3) { // Ctrl+C
            printf("\nCancelled.\n");
            exit(1);
        } else if (ch == 22) { // Ctrl+V
#ifdef _WIN32
            paste_from_clipboard(buffer, &idx, max_len);
#endif
        } else if (ch == 0 || ch == 224) { // Extended key
            int ext = _getch();
#ifdef _WIN32
            // 82 = Insert key. Check if Shift is held down (Shift + Insert)
            if (ext == 82) {
                SHORT shiftState = GetKeyState(VK_SHIFT);
                if (shiftState & 0x8000) {
                    paste_from_clipboard(buffer, &idx, max_len);
                }
            }
#else
            (void)ext;
#endif
        } else if (ch >= 32 && ch <= 126 && idx < max_len - 1) {
            buffer[idx++] = (char)ch;
            printf("*");
            fflush(stdout);
        }
    }
    trim_whitespace(buffer);
}

bool config_get_token(char *token_out, size_t max_len) {
    const char *env_filename = ".env";

    if (read_token_from_env_file(env_filename, token_out, max_len)) {
        return true;
    }

    printf("\n");
    printf("===================================================\n");
    printf("               DISCORD CLI - SETUP                 \n");
    printf("===================================================\n");
    printf("No Discord token found in '%s'.\n", env_filename);
    printf("Paste your Discord token (Ctrl+V / Shift+Insert supported):\n> ");
    fflush(stdout);

    char input_token[MAX_TOKEN_LEN];
    read_masked_input(input_token, sizeof(input_token));

    if (strlen(input_token) == 0) {
        fprintf(stderr, "Invalid or empty token!\n");
        return false;
    }

    if (save_token_to_env_file(env_filename, input_token)) {
        printf("Token successfully saved to '%s'!\n\n", env_filename);
    }

    strncpy_s(token_out, max_len, input_token, _TRUNCATE);
    return true;
}
