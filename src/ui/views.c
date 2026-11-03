#include "views.h"
#include "../discord/api.h"
#include "../discord/state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
}

static void render_header(const char *title, const char *hint) {
    printf("%s%s %-36s %s%s%s\n", COLOR_BOLD, COLOR_CYAN, title, COLOR_DIM, hint, COLOR_RESET);
    printf("%s------------------------------------------------------------%s\n", COLOR_DIM, COLOR_RESET);
}

static void render_row(bool is_selected, const char *label, int unread_count, int width) {
    char count_str[16] = "";
    if (unread_count > 0) {
        snprintf(count_str, sizeof(count_str), "%d", unread_count);
    }

    if (is_selected) {
        printf("%s> %-*.*s | %-4s%s\n", COLOR_INVERT, width, width, label, count_str, COLOR_RESET);
    } else {
        printf("  %-*.*s | %s%-4s%s\n", width, width, label,
               (unread_count > 0 ? COLOR_YELLOW : COLOR_DIM), count_str, COLOR_RESET);
    }
}

static void render_main_menu(UIState *ui, DiscordState *state) {
    char header_title[64];
    if (strlen(state->username) > 0) {
        snprintf(header_title, sizeof(header_title), "Discord CLI (@%s)", state->username);
    } else {
        snprintf(header_title, sizeof(header_title), "Discord CLI");
    }
    render_header(header_title, "[↑/↓] Navigate  [Enter] Open  [q] Quit");

    int total_items = 1 + state->guild_count;
    if (ui->main_selected_idx >= total_items) {
        ui->main_selected_idx = total_items > 0 ? total_items - 1 : 0;
    }

    // Row 0: Direct Messages
    render_row(ui->main_selected_idx == 0, "Direct Messages", state->dm_total_unread, 32);

    // Rows 1..N: Servers
    for (int i = 0; i < state->guild_count; i++) {
        render_row(ui->main_selected_idx == (i + 1), state->guilds[i].name, state->guilds[i].unread_count, 32);
    }
}

static void render_dms_list(UIState *ui, DiscordState *state) {
    render_header("Direct Messages", "[↑/↓] Navigate  [Enter] Chat  [Esc] Back");

    if (state->dm_count == 0) {
        printf("  %sNo direct messages found.%s\n", COLOR_DIM, COLOR_RESET);
        return;
    }

    if (ui->dm_selected_idx >= state->dm_count) {
        ui->dm_selected_idx = state->dm_count - 1;
    }

    for (int i = 0; i < state->dm_count; i++) {
        char label[128];
        if (strcmp(state->dms[i].username, "group") == 0) {
            snprintf(label, sizeof(label), "%s", state->dms[i].display_name);
        } else {
            snprintf(label, sizeof(label), "%s (@%s)", state->dms[i].display_name, state->dms[i].username);
        }
        render_row(ui->dm_selected_idx == i, label, state->dms[i].unread_count, 36);
    }
}

static void render_guild_channels(UIState *ui, DiscordState *state) {
    if (ui->guild_selected_idx < 0 || ui->guild_selected_idx >= state->guild_count) {
        ui->current_view = VIEW_MAIN_MENU;
        return;
    }

    DiscordGuild *guild = &state->guilds[ui->guild_selected_idx];
    char title[128];
    snprintf(title, sizeof(title), "Server: %s", guild->name);
    render_header(title, "[↑/↓] Navigate  [Enter] Select  [Esc] Back");

    if (guild->channel_count == 0) {
        printf("  %sNo text channels available.%s\n", COLOR_DIM, COLOR_RESET);
        return;
    }

    if (ui->channel_selected_idx >= guild->channel_count) {
        ui->channel_selected_idx = guild->channel_count - 1;
    }

    for (int i = 0; i < guild->channel_count; i++) {
        char label[128];
        snprintf(label, sizeof(label), "#%s", guild->channels[i].name);
        render_row(ui->channel_selected_idx == i, label, guild->channels[i].unread_count, 32);
    }
}

static void render_chat(UIState *ui, DiscordState *state) {
    char title[128];
    snprintf(title, sizeof(title), "# %s", state->active_channel_name);
    render_header(title, "[Esc] Back to menu");

    // Display messages
    if (state->current_message_count == 0) {
        printf("  %sNo recent messages or loading...%s\n\n", COLOR_DIM, COLOR_RESET);
    } else {
        for (int i = 0; i < state->current_message_count; i++) {
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
    printf("\n%s------------------------------------------------------------%s\n", COLOR_DIM, COLOR_RESET);
    printf("Message: %s", ui->input_buffer);
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

    // Fetch initial history
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
                    state->needs_redraw = true;
                } else {
                    int guild_idx = ui->main_selected_idx - 1;
                    if (guild_idx >= 0 && guild_idx < state->guild_count) {
                        ui->guild_selected_idx = guild_idx;
                        ui->channel_selected_idx = 0;
                        ui->current_view = VIEW_GUILD_CHANNELS;
                        state->needs_redraw = true;
                    }
                }
            }
            break;
        }

        case VIEW_DMS_LIST: {
            if (event->type == KEY_ESC || (event->type == KEY_CHAR && (event->ch == 'b' || event->ch == 'B'))) {
                ui->current_view = VIEW_MAIN_MENU;
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
                    char ch_name[96];
                    snprintf(ch_name, sizeof(ch_name), "@%s", dm->username);
                    open_channel(ui, state, token, dm->id, ch_name);
                }
            }
            break;
        }

        case VIEW_GUILD_CHANNELS: {
            if (event->type == KEY_ESC || (event->type == KEY_CHAR && (event->ch == 'b' || event->ch == 'B'))) {
                ui->current_view = VIEW_MAIN_MENU;
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
                    char full_ch_name[96];
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
                // Return to previous view
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

                    // Clear input immediately
                    ui->input_len = 0;
                    ui->input_buffer[0] = '\0';

                    // Send to discord REST API
                    discord_api_send_message(token, state->active_channel_id, text_to_send);

                    // Add to local message list immediately for responsiveness
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

