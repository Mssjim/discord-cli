#include "views.h"
#include "../discord/api.h"
#include "../discord/state.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_DIM     "\x1b[2m"
#define COLOR_INVERT  "\x1b[7m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_GREEN   "\x1b[32m"

void ui_init(UIState *ui) {
    if (!ui) return;
    memset(ui, 0, sizeof(UIState));
    ui->current_view = VIEW_MAIN_MENU;
    ui->hide_header = false;
}

static int utf8_display_width(const char *utf8_str) {
    if (!utf8_str) return 0;
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wlen <= 1) return 0;
    wchar_t *wstr = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (!wstr) return (int)strlen(utf8_str);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);

    int width = 0;
    for (int i = 0; i < wlen - 1; i++) {
        wchar_t ch = wstr[i];
        if ((ch >= 0x1100 && ch <= 0x115F) ||
            (ch >= 0x2E80 && ch <= 0xA4CF) ||
            (ch >= 0xAC00 && ch <= 0xD7A3) ||
            (ch >= 0xF900 && ch <= 0xFAFF) ||
            (ch >= 0xFE10 && ch <= 0xFE19) ||
            (ch >= 0xFE30 && ch <= 0xFE6F) ||
            (ch >= 0xFF00 && ch <= 0xFF60) ||
            (ch >= 0xFFE0 && ch <= 0xFFE6) ||
            (ch >= 0xD800 && ch <= 0xDBFF)) {
            width += 2;
            if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < wlen - 1) {
                i++; // skip low surrogate
            }
        } else {
            width += 1;
        }
    }
    free(wstr);
    return width;
#else
    return (int)strlen(utf8_str);
#endif
}

static void render_header(const char *title, const char *hint, bool hide_header) {
    if (hide_header) return;

    int term_width = terminal_get_width();
    if (term_width < 40) term_width = 40;

    int title_len = (int)strlen(title);
    int hint_len = (int)strlen(hint);
    int space = term_width - title_len - hint_len - 1;
    if (space < 2) space = 2;

    printf("%s%s%s%*s%s%s%s\n",
           COLOR_BOLD, COLOR_CYAN, title,
           space, "",
           COLOR_DIM, hint, COLOR_RESET);

    // Separator line across full width
    for (int i = 0; i < term_width - 1; i++) {
        putchar('-');
    }
    printf("\n");
}

static void render_row(bool is_selected, const char *label, int unread_count) {
    int term_width = terminal_get_width();
    if (term_width < 40) term_width = 40;

    char count_str[16] = "";
    int count_width = 0;
    if (unread_count > 0) {
        snprintf(count_str, sizeof(count_str), "[%d]", unread_count);
        count_width = (int)strlen(count_str);
    }

    int prefix_width = 2; // "> " or "  "
    int right_margin = 1;
    int max_label_width = term_width - prefix_width - (count_width > 0 ? (count_width + 1) : 0) - right_margin;
    if (max_label_width < 8) max_label_width = 8;

    // Truncate label according to actual display width
    char disp_label[256];
    strncpy_s(disp_label, sizeof(disp_label), label, _TRUNCATE);

    int label_disp_width = utf8_display_width(disp_label);
    while (label_disp_width > max_label_width && strlen(disp_label) > 0) {
        disp_label[strlen(disp_label) - 1] = '\0';
        label_disp_width = utf8_display_width(disp_label);
    }

    int padding = term_width - prefix_width - label_disp_width - count_width - right_margin;
    if (padding < 1) padding = 1;

    if (is_selected) {
        printf("%s> %s%*s%s%s\n",
               COLOR_INVERT, disp_label, padding, "", count_str, COLOR_RESET);
    } else {
        printf("  %s%*s%s%s%s\n",
               disp_label, padding, "",
               (unread_count > 0 ? COLOR_YELLOW : COLOR_DIM), count_str, COLOR_RESET);
    }
}

static void render_scroll_hint(int current_idx, int total_items, int start_idx, int end_idx) {
    int term_width = terminal_get_width();
    if (total_items <= (end_idx - start_idx)) return;

    char hint[64];
    snprintf(hint, sizeof(hint), "-- [%d/%d] %s%s--",
             current_idx + 1, total_items,
             (start_idx > 0 ? "^ " : ""),
             (end_idx < total_items ? "v " : ""));
    int hint_len = (int)strlen(hint);
    int left_pad = (term_width - hint_len) / 2;
    if (left_pad < 0) left_pad = 0;

    printf("%s%*s%s%s\n", COLOR_DIM, left_pad, "", hint, COLOR_RESET);
}

static void render_main_menu(UIState *ui, DiscordState *state) {
    char header_title[128];
    if (strlen(state->username) > 0) {
        snprintf(header_title, sizeof(header_title), "Discord CLI (@%s)", state->username);
    } else {
        snprintf(header_title, sizeof(header_title), "Discord CLI");
    }
    render_header(header_title, "[^/v] Nav  [Enter] Open  [H] Hide  [q] Quit", ui->hide_header);

    int total_items = 1 + state->guild_count;
    if (ui->main_selected_idx >= total_items) {
        ui->main_selected_idx = total_items > 0 ? total_items - 1 : 0;
    }

    int term_height = terminal_get_height();
    int reserved_lines = ui->hide_header ? 2 : 4;
    int visible_rows = term_height - reserved_lines;
    if (visible_rows < 3) visible_rows = 3;

    // Adjust scroll offset to keep selected item visible
    if (ui->main_selected_idx < ui->scroll_offset) {
        ui->scroll_offset = ui->main_selected_idx;
    }
    if (ui->main_selected_idx >= ui->scroll_offset + visible_rows) {
        ui->scroll_offset = ui->main_selected_idx - visible_rows + 1;
    }
    if (ui->scroll_offset < 0) ui->scroll_offset = 0;

    int end_idx = ui->scroll_offset + visible_rows;
    if (end_idx > total_items) end_idx = total_items;

    for (int i = ui->scroll_offset; i < end_idx; i++) {
        if (i == 0) {
            render_row(ui->main_selected_idx == 0, "Direct Messages", state->dm_total_unread);
        } else {
            int g_idx = i - 1;
            char display_name[128];
            if (strlen(state->guilds[g_idx].name) > 0) {
                snprintf(display_name, sizeof(display_name), "%s", state->guilds[g_idx].name);
            } else {
                snprintf(display_name, sizeof(display_name), "Server %s", state->guilds[g_idx].id);
            }
            render_row(ui->main_selected_idx == i, display_name, state->guilds[g_idx].unread_count);
        }
    }

    render_scroll_hint(ui->main_selected_idx, total_items, ui->scroll_offset, end_idx);
}

static void render_dms_list(UIState *ui, DiscordState *state) {
    render_header("Direct Messages", "[^/v] Nav  [Enter] Chat  [H] Hide  [Esc] Back", ui->hide_header);

    if (state->dm_count == 0) {
        printf("  %sNo direct messages found.%s\n", COLOR_DIM, COLOR_RESET);
        return;
    }

    if (ui->dm_selected_idx >= state->dm_count) {
        ui->dm_selected_idx = state->dm_count - 1;
    }

    int term_height = terminal_get_height();
    int reserved_lines = ui->hide_header ? 2 : 4;
    int visible_rows = term_height - reserved_lines;
    if (visible_rows < 3) visible_rows = 3;

    if (ui->dm_selected_idx < ui->scroll_offset) {
        ui->scroll_offset = ui->dm_selected_idx;
    }
    if (ui->dm_selected_idx >= ui->scroll_offset + visible_rows) {
        ui->scroll_offset = ui->dm_selected_idx - visible_rows + 1;
    }
    if (ui->scroll_offset < 0) ui->scroll_offset = 0;

    int end_idx = ui->scroll_offset + visible_rows;
    if (end_idx > state->dm_count) end_idx = state->dm_count;

    for (int i = ui->scroll_offset; i < end_idx; i++) {
        char label[512];
        if (strlen(state->dms[i].username) > 0 && strcmp(state->dms[i].username, "group") != 0) {
            snprintf(label, sizeof(label), "%s (@%s)", state->dms[i].display_name, state->dms[i].username);
        } else {
            snprintf(label, sizeof(label), "%s", state->dms[i].display_name);
        }
        render_row(ui->dm_selected_idx == i, label, state->dms[i].unread_count);
    }

    render_scroll_hint(ui->dm_selected_idx, state->dm_count, ui->scroll_offset, end_idx);
}

static void render_guild_channels(UIState *ui, DiscordState *state) {
    if (ui->guild_selected_idx < 0 || ui->guild_selected_idx >= state->guild_count) {
        ui->current_view = VIEW_MAIN_MENU;
        return;
    }

    DiscordGuild *guild = &state->guilds[ui->guild_selected_idx];
    char title[128];
    snprintf(title, sizeof(title), "Server: %s", guild->name);
    render_header(title, "[^/v] Nav  [Enter] Select  [H] Hide  [Esc] Back", ui->hide_header);

    if (guild->channel_count == 0) {
        printf("  %sNo text channels available.%s\n", COLOR_DIM, COLOR_RESET);
        return;
    }

    if (ui->channel_selected_idx >= guild->channel_count) {
        ui->channel_selected_idx = guild->channel_count - 1;
    }

    int term_height = terminal_get_height();
    int reserved_lines = ui->hide_header ? 2 : 4;
    int visible_rows = term_height - reserved_lines;
    if (visible_rows < 3) visible_rows = 3;

    if (ui->channel_selected_idx < ui->scroll_offset) {
        ui->scroll_offset = ui->channel_selected_idx;
    }
    if (ui->channel_selected_idx >= ui->scroll_offset + visible_rows) {
        ui->scroll_offset = ui->channel_selected_idx - visible_rows + 1;
    }
    if (ui->scroll_offset < 0) ui->scroll_offset = 0;

    int end_idx = ui->scroll_offset + visible_rows;
    if (end_idx > guild->channel_count) end_idx = guild->channel_count;

    for (int i = ui->scroll_offset; i < end_idx; i++) {
        char label[128];
        snprintf(label, sizeof(label), "#%s", guild->channels[i].name);
        render_row(ui->channel_selected_idx == i, label, guild->channels[i].unread_count);
    }

    render_scroll_hint(ui->channel_selected_idx, guild->channel_count, ui->scroll_offset, end_idx);
}

static void render_chat(UIState *ui, DiscordState *state) {
    char title[128];
    snprintf(title, sizeof(title), "# %s", state->active_channel_name);
    render_header(title, "[Esc] Back to menu", ui->hide_header);

    int term_height = terminal_get_height();
    int term_width = terminal_get_width();

    int reserved_chat = ui->hide_header ? 3 : 5;
    int max_chat_lines = term_height - reserved_chat;
    if (max_chat_lines < 2) max_chat_lines = 2;

    int start_msg = 0;
    if (state->current_message_count > max_chat_lines) {
        start_msg = state->current_message_count - max_chat_lines;
    }

    if (state->current_message_count == 0) {
        printf("  %sNo recent messages or loading...%s\n\n", COLOR_DIM, COLOR_RESET);
    } else {
        for (int i = start_msg; i < state->current_message_count; i++) {
            DiscordMessage *m = &state->current_messages[i];
            bool is_self = (strlen(state->user_id) > 0 && strcmp(m->author_id, state->user_id) == 0);

            printf("%s[%s]%s %s%s%s: %s\n",
                   COLOR_DIM, m->timestamp, COLOR_RESET,
                   (is_self ? COLOR_GREEN : COLOR_CYAN),
                   (strlen(m->author_global_name) > 0 ? m->author_global_name : m->author_name),
                   COLOR_RESET,
                   m->content);
        }
    }

    // Input prompt at bottom
    printf("\n");
    for (int i = 0; i < term_width - 1; i++) putchar('-');
    printf("\nMessage: %s", ui->input_buffer);
    fflush(stdout);
}

void ui_render(UIState *ui, DiscordState *state) {
    if (!ui || !state) return;

    DISCORD_MUTEX_LOCK(&state->lock);

    terminal_clear();

    if (!state->is_ready) {
        printf("\n  Connecting to Discord...\n");
        printf("  Waiting for Gateway handshake...\n");
        fflush(stdout);
        DISCORD_MUTEX_UNLOCK(&state->lock);
        return;
    }

    switch (ui->current_view) {
        case VIEW_MAIN_MENU:
            terminal_hide_cursor();
            render_main_menu(ui, state);
            break;
        case VIEW_DMS_LIST:
            terminal_hide_cursor();
            render_dms_list(ui, state);
            break;
        case VIEW_GUILD_CHANNELS:
            terminal_hide_cursor();
            render_guild_channels(ui, state);
            break;
        case VIEW_CHAT:
            terminal_show_cursor();
            render_chat(ui, state);
            break;
    }

    state->needs_redraw = false;
    DISCORD_MUTEX_UNLOCK(&state->lock);
}

static void open_channel(UIState *ui, DiscordState *state, const char *token, const char *channel_id, const char *channel_name) {
    discord_state_set_active_channel(state, channel_id, channel_name);
    ui->current_view = VIEW_CHAT;
    ui->input_len = 0;
    ui->input_buffer[0] = '\0';
    ui->scroll_offset = 0;

    DiscordMessage fetched[MAX_CACHED_MESSAGES];
    int count = 0;
    if (discord_api_fetch_messages(token, channel_id, 30, fetched, &count)) {
        discord_state_set_messages(state, fetched, count);
        if (count > 0) {
            discord_api_ack_message(token, channel_id, fetched[count - 1].id);
        }
    }
}

bool ui_handle_input(UIState *ui, DiscordState *state, const char *token, const KeyEvent *event) {
    if (!ui || !state || !event || event->type == KEY_NONE) return true;

    DISCORD_MUTEX_LOCK(&state->lock);
    ViewMode mode = ui->current_view;
    DISCORD_MUTEX_UNLOCK(&state->lock);

    switch (mode) {
        case VIEW_MAIN_MENU: {
            if (event->type == KEY_CHAR && (event->ch == 'q' || event->ch == 'Q')) {
                return false; // Exit app
            }
            if (event->type == KEY_CHAR && (event->ch == 'h' || event->ch == 'H')) {
                ui->hide_header = !ui->hide_header;
                state->needs_redraw = true;
                return true;
            }
            if (event->type == KEY_UP) {
                if (ui->main_selected_idx > 0) ui->main_selected_idx--;
                state->needs_redraw = true;
            } else if (event->type == KEY_DOWN) {
                int total_items = 1 + state->guild_count;
                if (ui->main_selected_idx < total_items - 1) ui->main_selected_idx++;
                state->needs_redraw = true;
            } else if (event->type == KEY_ENTER) {
                if (ui->main_selected_idx == 0) {
                    ui->current_view = VIEW_DMS_LIST;
                    ui->dm_selected_idx = 0;
                    ui->scroll_offset = 0;
                    state->needs_redraw = true;
                } else {
                    int guild_idx = ui->main_selected_idx - 1;
                    if (guild_idx >= 0 && guild_idx < state->guild_count) {
                        ui->guild_selected_idx = guild_idx;
                        ui->channel_selected_idx = 0;
                        ui->scroll_offset = 0;
                        ui->current_view = VIEW_GUILD_CHANNELS;
                        state->needs_redraw = true;
                    }
                }
            } else if (event->type == KEY_MOUSE_CLICK) {
                int header_rows = ui->hide_header ? 0 : 2;
                int term_height = terminal_get_height();
                int reserved_lines = ui->hide_header ? 2 : 4;
                int visible_rows = term_height - reserved_lines;
                if (visible_rows < 3) visible_rows = 3;

                int total_items = 1 + state->guild_count;
                int end_idx = ui->scroll_offset + visible_rows;
                if (end_idx > total_items) end_idx = total_items;

                int clicked_idx = ui->scroll_offset + (event->mouse_y - header_rows);
                if (event->mouse_y >= header_rows && clicked_idx >= ui->scroll_offset && clicked_idx < end_idx) {
                    ui->main_selected_idx = clicked_idx;
                    if (ui->main_selected_idx == 0) {
                        ui->current_view = VIEW_DMS_LIST;
                        ui->dm_selected_idx = 0;
                        ui->scroll_offset = 0;
                        state->needs_redraw = true;
                    } else {
                        int guild_idx = ui->main_selected_idx - 1;
                        if (guild_idx >= 0 && guild_idx < state->guild_count) {
                            ui->guild_selected_idx = guild_idx;
                            ui->channel_selected_idx = 0;
                            ui->scroll_offset = 0;
                            ui->current_view = VIEW_GUILD_CHANNELS;
                            state->needs_redraw = true;
                        }
                    }
                }
            }
            break;
        }

        case VIEW_DMS_LIST: {
            if (event->type == KEY_CHAR && (event->ch == 'h' || event->ch == 'H')) {
                ui->hide_header = !ui->hide_header;
                state->needs_redraw = true;
                return true;
            }
            if (event->type == KEY_ESC || (event->type == KEY_CHAR && (event->ch == 'b' || event->ch == 'B'))) {
                ui->current_view = VIEW_MAIN_MENU;
                ui->scroll_offset = 0;
                state->needs_redraw = true;
            } else if (event->type == KEY_UP) {
                if (ui->dm_selected_idx > 0) ui->dm_selected_idx--;
                state->needs_redraw = true;
            } else if (event->type == KEY_DOWN) {
                if (ui->dm_selected_idx < state->dm_count - 1) ui->dm_selected_idx++;
                state->needs_redraw = true;
            } else if (event->type == KEY_ENTER) {
                if (ui->dm_selected_idx >= 0 && ui->dm_selected_idx < state->dm_count) {
                    DiscordDM *dm = &state->dms[ui->dm_selected_idx];
                    char ch_name[256];
                    if (strlen(dm->username) > 0 && strcmp(dm->username, "group") != 0) {
                        snprintf(ch_name, sizeof(ch_name), "@%s", dm->username);
                    } else {
                        snprintf(ch_name, sizeof(ch_name), "%s", dm->display_name);
                    }
                    open_channel(ui, state, token, dm->id, ch_name);
                }
            } else if (event->type == KEY_MOUSE_CLICK) {
                int header_rows = ui->hide_header ? 0 : 2;
                int term_height = terminal_get_height();
                int reserved_lines = ui->hide_header ? 2 : 4;
                int visible_rows = term_height - reserved_lines;
                if (visible_rows < 3) visible_rows = 3;

                int end_idx = ui->scroll_offset + visible_rows;
                if (end_idx > state->dm_count) end_idx = state->dm_count;

                int clicked_idx = ui->scroll_offset + (event->mouse_y - header_rows);
                if (event->mouse_y >= header_rows && clicked_idx >= ui->scroll_offset && clicked_idx < end_idx) {
                    ui->dm_selected_idx = clicked_idx;
                    DiscordDM *dm = &state->dms[clicked_idx];
                    char ch_name[256];
                    if (strlen(dm->username) > 0 && strcmp(dm->username, "group") != 0) {
                        snprintf(ch_name, sizeof(ch_name), "@%s", dm->username);
                    } else {
                        snprintf(ch_name, sizeof(ch_name), "%s", dm->display_name);
                    }
                    open_channel(ui, state, token, dm->id, ch_name);
                }
            }
            break;
        }

        case VIEW_GUILD_CHANNELS: {
            if (event->type == KEY_CHAR && (event->ch == 'h' || event->ch == 'H')) {
                ui->hide_header = !ui->hide_header;
                state->needs_redraw = true;
                return true;
            }
            if (event->type == KEY_ESC || (event->type == KEY_CHAR && (event->ch == 'b' || event->ch == 'B'))) {
                ui->current_view = VIEW_MAIN_MENU;
                ui->scroll_offset = 0;
                state->needs_redraw = true;
            } else if (event->type == KEY_UP) {
                if (ui->channel_selected_idx > 0) ui->channel_selected_idx--;
                state->needs_redraw = true;
            } else if (event->type == KEY_DOWN) {
                DiscordGuild *guild = &state->guilds[ui->guild_selected_idx];
                if (ui->channel_selected_idx < guild->channel_count - 1) ui->channel_selected_idx++;
                state->needs_redraw = true;
            } else if (event->type == KEY_ENTER) {
                DiscordGuild *guild = &state->guilds[ui->guild_selected_idx];
                if (ui->channel_selected_idx >= 0 && ui->channel_selected_idx < guild->channel_count) {
                    DiscordChannel *ch = &guild->channels[ui->channel_selected_idx];
                    char full_ch_name[128];
                    snprintf(full_ch_name, sizeof(full_ch_name), "%s > #%s", guild->name, ch->name);
                    open_channel(ui, state, token, ch->id, full_ch_name);
                }
            } else if (event->type == KEY_MOUSE_CLICK) {
                DiscordGuild *guild = &state->guilds[ui->guild_selected_idx];
                int header_rows = ui->hide_header ? 0 : 2;
                int term_height = terminal_get_height();
                int reserved_lines = ui->hide_header ? 2 : 4;
                int visible_rows = term_height - reserved_lines;
                if (visible_rows < 3) visible_rows = 3;

                int end_idx = ui->scroll_offset + visible_rows;
                if (end_idx > guild->channel_count) end_idx = guild->channel_count;

                int clicked_idx = ui->scroll_offset + (event->mouse_y - header_rows);
                if (event->mouse_y >= header_rows && clicked_idx >= ui->scroll_offset && clicked_idx < end_idx) {
                    ui->channel_selected_idx = clicked_idx;
                    DiscordChannel *ch = &guild->channels[clicked_idx];
                    char full_ch_name[128];
                    snprintf(full_ch_name, sizeof(full_ch_name), "%s > #%s", guild->name, ch->name);
                    open_channel(ui, state, token, ch->id, full_ch_name);
                }
            }
            break;
        }

        case VIEW_CHAT: {
            if (event->type == KEY_ESC) {
                discord_state_clear_active_channel(state);
                ui->input_len = 0;
                ui->input_buffer[0] = '\0';
                ui->scroll_offset = 0;
                if (ui->main_selected_idx == 0) {
                    ui->current_view = VIEW_DMS_LIST;
                } else {
                    ui->current_view = VIEW_GUILD_CHANNELS;
                }
                state->needs_redraw = true;
            } else if (event->type == KEY_BACKSPACE) {
                if (ui->input_len > 0) {
                    ui->input_len--;
                    ui->input_buffer[ui->input_len] = '\0';
                    state->needs_redraw = true;
                }
            } else if (event->type == KEY_ENTER) {
                if (ui->input_len > 0) {
                    char text_to_send[1024];
                    strncpy_s(text_to_send, sizeof(text_to_send), ui->input_buffer, _TRUNCATE);

                    ui->input_len = 0;
                    ui->input_buffer[0] = '\0';

                    discord_api_send_message(token, state->active_channel_id, text_to_send);

                    DiscordMessage local_msg;
                    memset(&local_msg, 0, sizeof(local_msg));
                    strncpy_s(local_msg.content, sizeof(local_msg.content), text_to_send, _TRUNCATE);
                    strncpy_s(local_msg.author_name, sizeof(local_msg.author_name), state->username, _TRUNCATE);
                    strncpy_s(local_msg.author_global_name, sizeof(local_msg.author_global_name), state->display_name, _TRUNCATE);
                    strncpy_s(local_msg.author_id, sizeof(local_msg.author_id), state->user_id, _TRUNCATE);
                    strncpy_s(local_msg.timestamp, sizeof(local_msg.timestamp), "now", _TRUNCATE);
                    discord_state_add_message(state, &local_msg);

                    state->needs_redraw = true;
                }
            } else if (event->type == KEY_CHAR) {
                if (ui->input_len < (int)sizeof(ui->input_buffer) - 2) {
                    ui->input_buffer[ui->input_len++] = (char)event->ch;
                    ui->input_buffer[ui->input_len] = '\0';
                    state->needs_redraw = true;
                }
            }
            break;
        }
    }

    return true;
}
