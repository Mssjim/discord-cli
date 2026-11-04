#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>

typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_ENTER,
    KEY_ESC,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_CHAR,
    KEY_MOUSE_CLICK
} KeyType;

typedef struct {
    KeyType type;
    int ch; // UTF-8 or ASCII character value if KEY_CHAR
    int mouse_x; // Column (0-based) for mouse event
    int mouse_y; // Row (0-based) for mouse event
} KeyEvent;

void terminal_init(void);
void terminal_cleanup(void);
void terminal_clear(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);
void terminal_set_cursor(int row, int col);
int terminal_get_width(void);
int terminal_get_height(void);
bool terminal_check_resized(void);

// Checks if a key or mouse event is available; if timeout_ms > 0, waits up to timeout_ms
bool terminal_read_key(KeyEvent *event, int timeout_ms);

#endif // TERMINAL_H
