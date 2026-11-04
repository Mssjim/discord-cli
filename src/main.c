#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "config.h"
#include "discord/types.h"
#include "discord/state.h"
#include "discord/gateway.h"
#include "ui/terminal.h"
#include "ui/views.h"

#ifdef _WIN32
static LONG WINAPI CrashExceptionHandler(EXCEPTION_POINTERS *pExceptionInfo) {
    terminal_show_cursor();
    DWORD code = pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionCode : 0;
    void *addr = pExceptionInfo ? pExceptionInfo->ExceptionRecord->ExceptionAddress : NULL;

    fprintf(stderr, "\n\n===================================================\n");
    fprintf(stderr, "                 FATAL ERROR / CRASH               \n");
    fprintf(stderr, "===================================================\n");
    fprintf(stderr, "Exception Code   : 0x%08lX\n", code);
    fprintf(stderr, "Faulting Address : %p\n", addr);

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        fprintf(stderr, "Type             : Access Violation (Segmentation Fault)\n");
    } else if (code == EXCEPTION_STACK_OVERFLOW) {
        fprintf(stderr, "Type             : Stack Overflow\n");
    }

    fprintf(stderr, "\nPress Enter to exit...\n");
    fflush(stderr);

    char dummy[16];
    fgets(dummy, sizeof(dummy), stdin);

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashExceptionHandler);
#endif

    terminal_init();

    char token[MAX_TOKEN_LEN];
    if (!config_get_token(token, sizeof(token))) {
        fprintf(stderr, "Failed to retrieve Discord token. Exiting.\n");
        terminal_cleanup();
        return 1;
    }

    DiscordState *state = (DiscordState *)malloc(sizeof(DiscordState));
    if (!state) {
        fprintf(stderr, "Failed to allocate memory for DiscordState.\n");
        terminal_cleanup();
        return 1;
    }
    discord_state_init(state);

    UIState ui;
    ui_init(&ui);

    // Initial loading screen
    state->needs_redraw = true;
    ui_render(&ui, state);

    // Start WebSocket Gateway
    if (!discord_gateway_start(token, state)) {
        fprintf(stderr, "Failed to connect to Discord Gateway.\n");
        discord_state_destroy(state);
        free(state);
        terminal_cleanup();
        return 1;
    }

    // Main application event loop
    bool running = true;
    while (running) {
        // Automatically check if console window dimensions changed
        if (terminal_check_resized()) {
            state->needs_redraw = true;
        }

        if (state->needs_redraw) {
            ui_render(&ui, state);
        }

        KeyEvent ev;
        if (terminal_read_key(&ev, 50)) {
            running = ui_handle_input(&ui, state, token, &ev);
            if (state->needs_redraw) {
                ui_render(&ui, state);
            }
        }
    }

    // Graceful cleanup
    terminal_clear();
    printf("Exiting Discord CLI...\n");
    discord_gateway_stop();
    discord_state_destroy(state);
    free(state);
    terminal_cleanup();

    return 0;
}
