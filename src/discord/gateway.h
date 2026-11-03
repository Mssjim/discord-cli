#ifndef DISCORD_GATEWAY_H
#define DISCORD_GATEWAY_H

#include "types.h"
#include <stdbool.h>

/**
 * Connects to the Discord Gateway, handles authentication, heartbeats,
 * and processes incoming dispatch events (READY, MESSAGE_CREATE, etc.) in a background thread.
 * 
 * @param token Discord user or bot token
 * @param state Shared state pointer
 * @return true if gateway was successfully started, false otherwise
 */
bool discord_gateway_start(const char *token, DiscordState *state);

/**
 * Stops the gateway thread and closes connections.
 */
void discord_gateway_stop(void);

#endif // DISCORD_GATEWAY_H

