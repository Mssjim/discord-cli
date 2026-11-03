# Discord CLI Client (Native C)

A lightweight, high-performance, and discreet Discord command-line interface (CLI) client for CMD, PowerShell, and Windows Terminal, written in **pure C (C99)**. Designed specifically for stealth, productivity, and distraction-free use in workplace environments.

---

## Key Features

- **Ultra-Lightweight**: Native binary compiled to only ~61 KB with near-zero memory footprint.
- **Zero External DLL Dependencies on Windows**: Uses native **WinHTTP** for HTTPS and TLS-secured WebSockets out of the box (no libcurl, OpenSSL, or external package managers required).
- **Discreet & Minimalist UI**: Clean tabular text layout without heavy ASCII art or distracting graphics—perfect for discreet workplace usage.
- **Smooth Arrow Navigation**: Seamlessly navigate lists using `↑` and `↓`, open conversations with `Enter`, and return with `Esc` or `b`.
- **Real-Time Notifications**: Displays unread badge/mention counts for Direct Messages and Servers.
- **Original Server Order**: Preserves the exact user-defined server order (`guild_positions` from user settings).
- **Interactive Chat**: Open channels or DMs to read recent chat history, receive real-time updates, and type/send messages directly from the terminal.
- **Automatic Token Setup**: Securely prompts for your token on first run with masked input (`*`) and persists it to `.env` (automatically protected in `.gitignore`).

---

## Visual Layout

### Main Menu
```
Discord CLI (@username)               [↑/↓] Navigate  [Enter] Open  [q] Quit
------------------------------------------------------------
> Direct Messages                  | 3
  Server 1                         | 6
  Server 2                         | 2
  Server 3                         | 17
  Server 4                         | 3
```

### Direct Messages
```
Direct Messages                      [↑/↓] Navigate  [Enter] Chat  [Esc] Back
------------------------------------------------------------
> User 1 (@user1)                  | 2
  User 2 (@user2)                  | 6
  User 3 (@user3)                  | 0
```

### Server Channels
```
Server: Server 1                     [↑/↓] Navigate  [Enter] Select  [Esc] Back
------------------------------------------------------------
> #general                         | 4
  #announcements                   | 2
  #dev                             | 0
```

### Real-Time Chat
```
# #general - Server 1                [Esc] Back to menu
------------------------------------------------------------
[14:20] Alice: Hey everyone!
[14:22] Bob: Hey Alice, how is the project going?
[14:25] You: Everything is on track.

------------------------------------------------------------
Message: Type your message here and press Enter to send_
```

---

## How to Build

### Option 1: Quick Build Script (Windows)
Double-click or run from your command prompt:
```cmd
build.bat
```
If CMake is not installed, the script automatically installs it with `winget`.
The optimized executable binary will be generated at:
```
build\Release\discord-cli.exe
```

### Option 2: CMake
```cmd
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

---

## How to Run

1. Run the executable:
   ```cmd
   .\build\Release\discord-cli.exe
   ```
2. On the first launch, the client will prompt for your Discord token (input is masked):
   ```
   ===================================================
                  DISCORD CLI - SETUP                 
   ===================================================
   No Discord token found in '.env'.
   Paste your Discord token (input will be masked):
   > ************************************
   ```
3. The token will be saved automatically to `.env` for future sessions.

### Keyboard Shortcuts

| Key | Action |
| :--- | :--- |
| `↑` / `↓` | Move selection up or down |
| `Enter` | Open selected item (or send message when in chat view) |
| `Esc` or `b` | Go back to the previous screen |
| `q` | Quit application (from the main menu) |
| `Backspace` | Delete character in the chat input bar |

---

## Project Structure

```
discord-cli/
├── CMakeLists.txt           # C99 build configuration
├── build.bat                # 1-click Windows build script
├── .env.example             # Example configuration file
├── lib/
│   └── cjson/               # Lightweight C99 JSON parser (DaveGamble/cJSON)
└── src/
    ├── main.c               # Entry point and event coordinator
    ├── config.c / .h        # Safe .env management and token loader
    ├── discord/
    │   ├── types.h          # Data structures (User, Guild, Channel, Message)
    │   ├── state.c / .h     # Memory state synchronization and unread counters
    │   ├── api.c / .h       # HTTPS REST calls (messages, history, ACK)
    │   └── gateway.c / .h   # Discord WSS Gateway connection (Heartbeats & Dispatch)
    └── ui/
        ├── terminal.c / .h  # ANSI VT100 console setup and key reader
        └── views.c / .h     # Minimalist table layout and chat rendering
```
