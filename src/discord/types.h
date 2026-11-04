#ifndef DISCORD_TYPES_H
#define DISCORD_TYPES_H

#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>
  typedef CRITICAL_SECTION discord_mutex_t;
  #define DISCORD_MUTEX_INIT(m) InitializeCriticalSection(m)
  #define DISCORD_MUTEX_LOCK(m) EnterCriticalSection(m)
  #define DISCORD_MUTEX_UNLOCK(m) LeaveCriticalSection(m)
  #define DISCORD_MUTEX_DESTROY(m) DeleteCriticalSection(m)
#else
  #include <pthread.h>
  typedef pthread_mutex_t discord_mutex_t;
  #define DISCORD_MUTEX_INIT(m) pthread_mutex_init(m, NULL)
  #define DISCORD_MUTEX_LOCK(m) pthread_mutex_lock(m)
  #define DISCORD_MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
  #define DISCORD_MUTEX_DESTROY(m) pthread_mutex_destroy(m)
#endif

#define MAX_CHANNELS_PER_GUILD 128
#define MAX_DMS 256
#define MAX_GUILDS 128
#define MAX_CACHED_MESSAGES 100

typedef struct {
    char id[36];
    char author_id[36];
    char author_name[64];
    char author_global_name[64];
    char content[2048];
    char timestamp[32]; // e.g. "14:32"
} DiscordMessage;

typedef struct {
    char id[36];
    char name[96];
    int type; // 0 = GUILD_TEXT, 2 = GUILD_VOICE, etc.
    int position;
    int unread_count;
    char last_message_id[36];
} DiscordChannel;

typedef struct {
    char id[36]; // channel_id
    char recipient_id[36];
    char username[256];
    char display_name[256];
    int unread_count;
    char last_message_id[36];
} DiscordDM;

typedef struct {
    char id[36];
    char name[96];
    int unread_count;
    int channel_count;
    DiscordChannel channels[MAX_CHANNELS_PER_GUILD];
} DiscordGuild;

typedef struct {
    // Current user
    char user_id[36];
    char username[64];
    char display_name[64];

    // DMs
    int dm_count;
    int dm_total_unread;
    DiscordDM dms[MAX_DMS];

    // Guilds
    int guild_count;
    DiscordGuild guilds[MAX_GUILDS];

    // Current viewed channel / chat
    char active_channel_id[36];
    char active_channel_name[96];
    int current_message_count;
    DiscordMessage current_messages[MAX_CACHED_MESSAGES];

    // Sync & Status
    bool is_ready;
    bool needs_redraw;
    discord_mutex_t lock;
} DiscordState;

#endif // DISCORD_TYPES_H
