#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static DWORD g_orig_out_mode = 0;
static DWORD g_orig_in_mode = 0;
static UINT g_orig_cp = 0;
static UINT g_orig_output_cp = 0;
static bool g_initialized = false;
static int g_last_width = 80;
static int g_last_height = 25;
static HANDLE g_hInConsole = INVALID_HANDLE_VALUE;

static HANDLE get_console_output_handle(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(hOut, &mode)) {
            return hOut;
        }
    }
    return CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
}

void terminal_lock_buffer_size(void) {
    HANDLE hOut = get_console_output_handle();
    if (hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            if (w > 10 && h > 3) {
                COORD sz;
                sz.X = (SHORT)w;
                sz.Y = (SHORT)h;
                SetConsoleScreenBufferSize(hOut, sz);
            }
        }
        if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);
    }
}

void terminal_init(void) {
    if (g_initialized) return;

    g_orig_cp = GetConsoleCP();
    g_orig_output_cp = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = get_console_output_handle();
    if (hOut != INVALID_HANDLE_VALUE) {
        GetConsoleMode(hOut, &g_orig_out_mode);
        DWORD out_mode = g_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, out_mode);
        if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);
    }

    // Use alternate screen buffer so standard console scroll history is hidden and isolated
    printf("\x1b[?1049h");
    fflush(stdout);

    // Open CONIN$ with mouse input enabled
    g_hInConsole = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (g_hInConsole != INVALID_HANDLE_VALUE) {
        GetConsoleMode(g_hInConsole, &g_orig_in_mode);
        DWORD in_mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
        SetConsoleMode(g_hInConsole, in_mode);
    }

    terminal_lock_buffer_size();
    g_last_width = terminal_get_width();
    g_last_height = terminal_get_height();
    g_initialized = true;
}

void terminal_cleanup(void) {
    if (!g_initialized) return;

    terminal_show_cursor();

    // Leave alternate screen buffer
    printf("\x1b[?1049l");
    fflush(stdout);

    HANDLE hOut = get_console_output_handle();
    if (hOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hOut, g_orig_out_mode);
        if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);
    }

    if (g_hInConsole != INVALID_HANDLE_VALUE) {
        SetConsoleMode(g_hInConsole, g_orig_in_mode);
        CloseHandle(g_hInConsole);
        g_hInConsole = INVALID_HANDLE_VALUE;
    }

    SetConsoleCP(g_orig_cp);
    SetConsoleOutputCP(g_orig_output_cp);
    g_initialized = false;
}

void terminal_clear(void) {
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

int terminal_get_width(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = get_console_output_handle();
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);
        if (w > 20) return w;
    }
    if (hOut != INVALID_HANDLE_VALUE && hOut != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(hOut);
    }
    return 80;
}

int terminal_get_height(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = get_console_output_handle();
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        int h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);
        if (h > 5) return h;
    }
    if (hOut != INVALID_HANDLE_VALUE && hOut != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(hOut);
    }
    return 25;
}

bool terminal_check_resized(void) {
    int cur_w = terminal_get_width();
    int cur_h = terminal_get_height();
    if (cur_w != g_last_width || cur_h != g_last_height) {
        g_last_width = cur_w;
        g_last_height = cur_h;
        terminal_lock_buffer_size();
        return true;
    }
    return false;
}

bool terminal_read_key(KeyEvent *event, int timeout_ms) {
    if (!event) return false;
    event->type = KEY_NONE;
    event->ch = 0;

    HANDLE hIn = (g_hInConsole != INVALID_HANDLE_VALUE) ? g_hInConsole : GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return false;

    DWORD waited = 0;
    while (1) {
        DWORD num_events = 0;
        if (GetNumberOfConsoleInputEvents(hIn, &num_events) && num_events > 0) {
            INPUT_RECORD ir;
            DWORD read_count = 0;
            if (ReadConsoleInputW(hIn, &ir, 1, &read_count) && read_count > 0) {
                // 1. Mouse wheel event (Map mouse wheel up/down to KEY_UP/KEY_DOWN)
                if (ir.EventType == MOUSE_EVENT) {
                    if (ir.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) {
                        int delta = (short)HIWORD(ir.Event.MouseEvent.dwButtonState);
                        if (delta > 0) {
                            event->type = KEY_UP;
                            return true;
                        } else if (delta < 0) {
                            event->type = KEY_DOWN;
                            return true;
                        }
                    }
                }
                // 2. Keyboard event
                else if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                    WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                    WCHAR wch = ir.Event.KeyEvent.uChar.UnicodeChar;

                    switch (vk) {
                        case VK_UP:
                            event->type = KEY_UP;
                            return true;
                        case VK_DOWN:
                            event->type = KEY_DOWN;
                            return true;
                        case VK_LEFT:
                            event->type = KEY_LEFT;
                            return true;
                        case VK_RIGHT:
                            event->type = KEY_RIGHT;
                            return true;
                        case VK_RETURN:
                            event->type = KEY_ENTER;
                            return true;
                        case VK_ESCAPE:
                            event->type = KEY_ESC;
                            return true;
                        case VK_BACK:
                            event->type = KEY_BACKSPACE;
                            return true;
                        case VK_TAB:
                            event->type = KEY_TAB;
                            return true;
                        default:
                            if (wch >= 32) {
                                event->type = KEY_CHAR;
                                event->ch = (int)wch;
                                return true;
                            }
                            break;
                    }
                }
            }
        }

        if ((int)waited >= timeout_ms) break;
        Sleep(10);
        waited += 10;
    }

    return false;
}

#else
// POSIX implementation
void terminal_init(void) {}
void terminal_cleanup(void) {}
void terminal_clear(void) { printf("\033[2J\033[H"); fflush(stdout); }
void terminal_hide_cursor(void) { printf("\033[?25l"); fflush(stdout); }
void terminal_show_cursor(void) { printf("\033[?25h"); fflush(stdout); }
void terminal_set_cursor(int row, int col) { printf("\033[%d;%dH", row, col); fflush(stdout); }
int terminal_get_width(void) { return 80; }
int terminal_get_height(void) { return 25; }
bool terminal_check_resized(void) { return false; }
bool terminal_read_key(KeyEvent *event, int timeout_ms) { (void)event; (void)timeout_ms; return false; }
#endif
