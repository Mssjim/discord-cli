#ifndef DISCORD_STATE_H
#define DISCORD_STATE_H

#include "types.h"

void discord_state_init(DiscordState *state);
void discord_state_destroy(DiscordState *state);

// Parses READY payload (guilds, user_settings guild_positions, private_channels, read_state)
void discord_state_parse_ready(DiscordState *state, const char *json_str, const char *token);

// Handles incoming GUILD_CREATE event (when guilds stream in after READY)
void discord_state_handle_guild_create(DiscordState *state, const char *json_str);

// Handles incoming MESSAGE_CREATE event
void discord_state_handle_message_create(DiscordState *state, const char *json_str);

// Handles MESSAGE_ACK event
void discord_state_handle_message_ack(DiscordState *state, const char *json_str);

// Set or clear the active chat channel
void discord_state_set_active_channel(DiscordState *state, const char *channel_id, const char *channel_name);
void discord_state_clear_active_channel(DiscordState *state);

// Manage cached messages for active channel
void discord_state_set_messages(DiscordState *state, const DiscordMessage *msgs, int count);
void discord_state_add_message(DiscordState *state, const DiscordMessage *msg);

#endif // DISCORD_STATE_H
