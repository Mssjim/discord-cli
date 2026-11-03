#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "discord/types.h"
#include "discord/state.h"
#include "discord/gateway.h"
#include "ui/terminal.h"
#include "ui/views.h"

int main(void) {
    terminal_init();

    char token[MAX_TOKEN_LEN];
    if (!config_get_token(token, sizeof(token))) {
        fprintf(stderr, "Failed to retrieve Discord token. Exiting.\n");
        terminal_cleanup();
        return 1;
    }

    DiscordState state;
    discord_state_init(&state);

    UIState ui;
    ui_init(&ui);

    // Initial loading screen
    state.needs_redraw = true;
    ui_render(&ui, &state);

    // Start WebSocket Gateway
    if (!discord_gateway_start(token, &state)) {
        fprintf(stderr, "Failed to connect to Discord Gateway.\n");
        discord_state_destroy(&state);
        terminal_cleanup();
        return 1;
    }

    // Main application event loop
    bool running = true;
    while (running) {
        if (state.needs_redraw) {
            ui_render(&ui, &state);
        }

        KeyEvent ev;
        if (terminal_read_key(&ev, 50)) {
            running = ui_handle_input(&ui, &state, token, &ev);
            if (state.needs_redraw) {
                ui_render(&ui, &state);
            }
        }
    }

    // Graceful cleanup
    terminal_clear();
    printf("Exiting Discord CLI...\n");
    discord_gateway_stop();
    discord_state_destroy(&state);
    terminal_cleanup();

    return 0;
}

