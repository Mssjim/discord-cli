#ifndef DISCORD_API_H
#define DISCORD_API_H

#include "types.h"
#include <stdbool.h>

/**
 * Fetches recent messages from a channel or DM.
 * Messages will be ordered chronologically (oldest to newest).
 */
bool discord_api_fetch_messages(const char *token, const char *channel_id, int limit, DiscordMessage *out_msgs, int *out_count);

/**
 * Sends a text message to a channel or DM.
 */
bool discord_api_send_message(const char *token, const char *channel_id, const char *content);

/**
 * Acknowledges a message to clear unread status on Discord server.
 */
bool discord_api_ack_message(const char *token, const char *channel_id, const char *message_id);

/**
 * Fetches user guilds from REST API (/users/@me/guilds)
 */
bool discord_api_fetch_guilds(const char *token, DiscordGuild *out_guilds, int max_guilds, int *out_count);

#endif // DISCORD_API_H
