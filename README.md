<div align="center">

# 💬 Discord CLI

**A tiny, fast, and discreet Discord client that runs right inside your Windows terminal.**  
*No installation required • Zero external dependencies • Mouse & Keyboard support • Ultra-low RAM usage*

[![Latest Release](https://img.shields.io/github/v/release/Mssjim/discord-cli?label=Download%20Latest&style=for-the-badge&color=5865F2)](https://github.com/Mssjim/discord-cli/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-0078D6?style=for-the-badge&logo=windows)](https://github.com/Mssjim/discord-cli/releases/latest)
[![License](https://img.shields.io/badge/License-MIT-22c55e?style=for-the-badge)](LICENSE)

</div>

---

<details>
<summary><b>🇧🇷 Leia em Português — Guia Rápido para Iniciantes (Clique para abrir)</b></summary>

<br>

### O que é o Discord CLI?
É um aplicativo super leve do Discord que roda em uma janelinha discreta de terminal no Windows. Ideal para conversar sem distrações, economizar memória do PC (usa menos de 5 MB de RAM em vez dos +500 MB do Discord tradicional) e usar de forma discreta no trabalho ou nos estudos. **Não precisa instalar nada!**

### 1️⃣ Como Baixar e Abrir (Sem aviso de bloqueio do Windows)
A forma mais fácil é abrir o **PowerShell** na pasta onde deseja salvar o programa, colar o comando abaixo e apertar `Enter`:

```powershell
curl.exe -L -o discord-cli.exe https://github.com/Mssjim/discord-cli/releases/latest/download/discord-cli.exe; .\discord-cli.exe
```

> **Prefere baixar pelo navegador?**  
> Baixe o `discord-cli.exe` na página de [**Releases**](https://github.com/Mssjim/discord-cli/releases/latest). Se aparecer a tela azul do Windows (*"O Windows protegeu o computador"*), basta clicar em **Mais informações** → **Executar assim mesmo** (isso só acontece na primeira vez porque o arquivo veio da internet).

### 2️⃣ Primeiro Acesso
1. Na primeira vez que abrir, o programa pedirá seu **Token do Discord**.
2. Basta copiar o token e colar com **`Ctrl + V`** (ou `Shift + Insert`) — ele aparecerá protegido com `****` — e apertar **`Enter`**.
3. Pronto! O token fica salvo apenas no seu computador (arquivo `.env`) e nas próximas vezes já entra direto.

### 3️⃣ Como Usar (Mouse e Teclado)
- **Clicar com o Mouse**: Clique em qualquer servidor, canal ou conversa (DM) para abrir.
- **Voltar**: Clique no botão **`[< Back]`** no canto superior esquerdo ou aperte `Esc`.
- **Rolar a tela**: Use a **rodinha do mouse (scroll)** ou as setas `↑` / `↓` para navegar nas listas e ler mensagens antigas do chat.
- **Modo Discreto (`H`)**: Aperte a tecla **`H`** para ocultar ou mostrar a barra de menu do topo.
- **Sair**: No menu principal, aperte **`Q`**.

</details>

---

## ✨ Why Discord CLI?

- ⚡ **Instant & Featherweight**: Single native executable (~150 KB) using almost zero CPU and RAM compared to the official Electron app.
- 🕵️ **Discreet by Design**: Looks like a standard command prompt/terminal window. Press **`H`** anytime to hide the top header bar, and resize the window as small as you like.
- 🖱️ **Effortless to Use**: Navigate with **mouse clicks and scroll wheel**, or stick purely to keyboard shortcuts — whichever feels natural.
- 📦 **100% Portable**: No installers, no Node.js, no Python, and no extra `.dll` files. Put `discord-cli.exe` in any folder (or a USB drive) and run.

---

## 🚀 Quick Start (No Technical Skills Needed!)

### Option 1: One-Line Terminal Download *(Recommended)*
Downloading directly via **PowerShell** avoids the blue Windows *"SmartScreen"* warning that browsers add to downloaded `.exe` files.

1. Open **PowerShell** in the folder where you want to keep Discord CLI.
2. Copy and paste this command, then press **`Enter`**:

```powershell
curl.exe -L -o discord-cli.exe https://github.com/Mssjim/discord-cli/releases/latest/download/discord-cli.exe; .\discord-cli.exe
```

---

### Option 2: Standard Browser Download
1. Go to the [**Latest Release page**](https://github.com/Mssjim/discord-cli/releases/latest) and download **`discord-cli.exe`** (or the `.zip` archive).
2. Double-click **`discord-cli.exe`** to start.

> 💡 **Seeing "Windows protected your PC"?**  
> Because this is a free, independent open-source app without a paid corporate certificate, Windows may show a blue prompt the first time you open a browser-downloaded `.exe`.  
> Simply click **More info** → **Run anyway** (or run `Unblock-File .\discord-cli.exe` in PowerShell).

---

## 🔑 First-Time Setup (Takes 30 Seconds)

When you launch `discord-cli.exe` for the first time, you'll see a one-time setup screen:

```text
===================================================
               DISCORD CLI - SETUP                 
===================================================
No Discord token found in '.env'.
Paste your Discord token (input will be masked):
> ************************************
```

1. Copy your Discord authentication token.
2. Paste it directly using **`Ctrl + V`** or **`Shift + Insert`** (characters are hidden as `*` for your privacy) and press **`Enter`**.
3. **Done!** A `.env` file is created next to `discord-cli.exe` so you are logged in automatically next time.

> 🔒 **Privacy Note**: Your token stays strictly on your own computer inside `.env` and only communicates directly with official `discord.com` HTTPS/WebSocket servers.

---

## 🎮 Controls & Shortcuts

You can control Discord CLI with **either your mouse or your keyboard**:

| Action | 🖱️ Mouse | ⌨️ Keyboard |
| :--- | :--- | :--- |
| **Select / Open item** | **Left-click** any DM, Server, or Channel | `↑` / `↓` then press `Enter` |
| **Go back** | Click **`[< Back]`** (top-left corner) | `Esc` (or `B` in menus) |
| **Scroll lists & chat history** | **Scroll Wheel** up / down | `↑` / `↓` or `Page Up` / `Page Down` |
| **Jump to newest messages** | Click the yellow bottom scroll banner | Scroll all the way down (or send a message) |
| **Send chat message** | — | Type your message and press `Enter` |
| **Stealth Mode (Hide top bar)** | — | Press `H` (toggle on/off) |
| **Quit app** | Close window | Press `Q` on the Main Menu |

---

## 🖥️ Interface Preview

### Main Menu
Servers appear in your exact custom Discord order, with right-aligned unread counters that automatically adapt when you resize the window:
```text
Discord CLI (@username)       [^/v] Nav  [Enter] Open  [H] Hide  [q] Quit
-------------------------------------------------------------------------
> Direct Messages                                                     [3]
  Gaming Squad                                                        [6]
  Work & Study                                                           
  Open Source Community                                              [17]
```

### Direct Messages & Group DMs
Shows display names and `@usernames` — including all participants in Group DMs:
```text
 [< Back] Direct Messages     [^/v] Nav  [Enter] Chat  [H] Hide  [Esc] Back
---------------------------------------------------------------------------
> Alice (@alice)                                                        [2]
  Bob, Charlie (@bob, charlie)                                          [5]
  Dave (@dave)                                                             
```

### Real-Time Chat (with History Scroll & Attachment Badges)
```text
 [< Back] # general                          [Wheel] Scroll  [Esc] Back
-------------------------------------------------------------------------
[14:20] Alice: Hey everyone!
[14:21] Bob: [ATTACHMENT] screenshot.png
  Check out the new layout!
[14:23] Alice: [ATTACHMENT] meme.gif
[14:25] You: Looks super clean!

-------------------------------------------------------------------------
Message: Type here and press Enter_
```

---

## 🛠️ For Developers: Building from Source

Discord CLI is written in **pure C99** using native **WinHTTP** for HTTPS REST requests and TLS WebSockets — zero third-party networking libraries required.

### 1-Click Build on Windows
Simply run:
```cmd
build.bat
```
*(Automatically locates MinGW-w64 GCC & CMake — or installs MinGW via `winget` if missing — and outputs `build\Release\discord-cli.exe`).*

### Manual CMake Build
```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Project Architecture
```text
discord-cli/
├── .github/workflows/release.yml  # Automated GitHub Releases CI/CD
├── CMakeLists.txt                 # C99 & Windows Resource build config
├── build.bat                      # 1-click Windows compiler script
├── lib/cjson/                     # Embedded C99 JSON parser (DaveGamble/cJSON)
└── src/
    ├── main.c                     # Application entry point & main loop
    ├── config.c / .h              # Masked token setup & .env persistence
    ├── app.rc                         # Windows version & publisher metadata
    ├── discord/
    │   ├── types.h                # Core structs (Guilds, DMs, Messages)
    │   ├── state.c / .h           # Thread-safe state & unread counters
    │   ├── api.c / .h             # Native WinHTTP REST client
    │   └── gateway.c / .h         # Native WinHTTP WebSocket Gateway client
    └── ui/
        ├── terminal.c / .h        # VT100 console, resize & mouse/key input
        └── views.c / .h           # Responsive menus & scrollable chat view
```

---

## 📄 License

Distributed under the **MIT License**. See [LICENSE](LICENSE) for details.
