#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>

static DWORD g_orig_out_mode = 0;
static DWORD g_orig_in_mode = 0;
static UINT g_orig_cp = 0;
static UINT g_orig_output_cp = 0;
static bool g_initialized = false;

void terminal_init(void) {
    if (g_initialized) return;

    g_orig_cp = GetConsoleCP();
    g_orig_output_cp = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        GetConsoleMode(hOut, &g_orig_out_mode);
        DWORD out_mode = g_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, out_mode);
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        GetConsoleMode(hIn, &g_orig_in_mode);
    }

    g_initialized = true;
}

void terminal_cleanup(void) {
    if (!g_initialized) return;

    terminal_show_cursor();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hOut, g_orig_out_mode);
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hIn, g_orig_in_mode);
    }

    SetConsoleCP(g_orig_cp);
    SetConsoleOutputCP(g_orig_output_cp);
    g_initialized = false;
}

void terminal_clear(void) {
    // ANSI clear screen and move cursor to (1,1)
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

void terminal_hide_cursor(void) {
    printf("\x1b[?25l");
    fflush(stdout);
}

void terminal_show_cursor(void) {
    printf("\x1b[?25h");
    fflush(stdout);
}

void terminal_set_cursor(int row, int col) {
    printf("\x1b[%d;%dH", row, col);
    fflush(stdout);
}

bool terminal_read_key(KeyEvent *event, int timeout_ms) {
    if (!event) return false;
    event->type = KEY_NONE;
    event->ch = 0;

    int waited = 0;
    while (!_kbhit()) {
        if (timeout_ms <= 0) return false;
        Sleep(20);
        waited += 20;
        if (waited >= timeout_ms) return false;
    }

    int c = _getch();
    if (c == 0 || c == 224) { // Extended key prefix
        int ext = _getch();
        switch (ext) {
            case 72: event->type = KEY_UP; break;
            case 80: event->type = KEY_DOWN; break;
            case 75: event->type = KEY_LEFT; break;
            case 77: event->type = KEY_RIGHT; break;
            default: break;
        }
        return true;
    }

    switch (c) {
        case 13: // Enter
            event->type = KEY_ENTER;
            break;
        case 27: // Esc
            event->type = KEY_ESC;
            break;
        case 8:  // Backspace
            event->type = KEY_BACKSPACE;
            break;
        case 9:  // Tab
            event->type = KEY_TAB;
            break;
        default:
            if (c >= 32 && c <= 255) {
                event->type = KEY_CHAR;
                event->ch = c;
            }
            break;
    }

    return true;
}

#else
// POSIX implementation
void terminal_init(void) {}
void terminal_cleanup(void) {}
void terminal_clear(void) { printf("\033[2J\033[H"); fflush(stdout); }
void terminal_hide_cursor(void) { printf("\033[?25l"); fflush(stdout); }
void terminal_show_cursor(void) { printf("\033[?25h"); fflush(stdout); }
void terminal_set_cursor(int row, int col) { printf("\033[%d;%dH", row, col); fflush(stdout); }
bool terminal_read_key(KeyEvent *event, int timeout_ms) { (void)event; (void)timeout_ms; return false; }
#endif

