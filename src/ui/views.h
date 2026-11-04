#ifndef VIEWS_H
#define VIEWS_H

#include "../discord/types.h"
#include "terminal.h"

typedef enum {
    VIEW_MAIN_MENU = 0,
    VIEW_DMS_LIST,
    VIEW_GUILD_CHANNELS,
    VIEW_CHAT
} ViewMode;

typedef struct {
    ViewMode current_view;
    int main_selected_idx;      // 0 = DMs, 1..N = Guilds
    int dm_selected_idx;        // 0..N = DMs
    int guild_selected_idx;     // index into state->guilds
    int channel_selected_idx;   // 0..N = Channels of selected guild
    
    // Chat input buffer
    char input_buffer[1024];
    int input_len;

    // View state
    int scroll_offset;
    int chat_scroll_offset;     // Messages scrolled up from the bottom in chat view
    bool hide_header;           // [H] Toggle to hide/show top menu header
} UIState;

void ui_init(UIState *ui);
void ui_render(UIState *ui, DiscordState *state);
bool ui_handle_input(UIState *ui, DiscordState *state, const char *token, const KeyEvent *event);

#endif // VIEWS_H
