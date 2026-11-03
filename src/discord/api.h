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

#endif // DISCORD_API_H

