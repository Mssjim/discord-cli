#include "state.h"
#include "api.h"
#include "../../lib/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void discord_state_init(DiscordState *state) {
    if (!state) return;
    memset(state, 0, sizeof(DiscordState));
    DISCORD_MUTEX_INIT(&state->lock);
}

void discord_state_destroy(DiscordState *state) {
    if (!state) return;
    DISCORD_MUTEX_DESTROY(&state->lock);
}

static void safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy_s(dest, dest_size, src, _TRUNCATE);
}

static int compare_dms(const void *a, const void *b) {
    const DiscordDM *dmA = (const DiscordDM *)a;
    const DiscordDM *dmB = (const DiscordDM *)b;
    size_t lenA = strlen(dmA->last_message_id);
    size_t lenB = strlen(dmB->last_message_id);
    if (lenA != lenB) {
        return (lenB > lenA) ? 1 : -1;
    }
    return strcmp(dmB->last_message_id, dmA->last_message_id);
}

void discord_state_parse_ready(DiscordState *state, const char *json_str, const char *token) {
    if (!state || !json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "[Gateway] Warning: Failed to parse READY JSON payload\n");
        return;
    }

    cJSON *d = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (!d) {
        cJSON_Delete(root);
        return;
    }

    DISCORD_MUTEX_LOCK(&state->lock);

    // 1. Current user
    cJSON *user = cJSON_GetObjectItemCaseSensitive(d, "user");
    if (user) {
        cJSON *uid = cJSON_GetObjectItemCaseSensitive(user, "id");
        cJSON *uname = cJSON_GetObjectItemCaseSensitive(user, "username");
        cJSON *gname = cJSON_GetObjectItemCaseSensitive(user, "global_name");

        if (cJSON_IsString(uid)) safe_strcpy(state->user_id, sizeof(state->user_id), uid->valuestring);
        if (cJSON_IsString(uname)) safe_strcpy(state->username, sizeof(state->username), uname->valuestring);
        if (cJSON_IsString(gname) && gname->valuestring) {
            safe_strcpy(state->display_name, sizeof(state->display_name), gname->valuestring);
        } else {
            safe_strcpy(state->display_name, sizeof(state->display_name), state->username);
        }
    }

    // 2. Read States (Badge and mention counts)
    cJSON *read_state = cJSON_GetObjectItemCaseSensitive(d, "read_state");
    cJSON *rs_entries = NULL;
    if (cJSON_IsObject(read_state)) {
        rs_entries = cJSON_GetObjectItemCaseSensitive(read_state, "entries");
    } else if (cJSON_IsArray(read_state)) {
        rs_entries = read_state;
    }

    typedef struct {
        char channel_id[36];
        int count;
    } TempReadCount;
    TempReadCount temp_rc[1024];
    int temp_rc_count = 0;

    if (rs_entries && cJSON_IsArray(rs_entries)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, rs_entries) {
            if (temp_rc_count >= 1024) break;
            cJSON *cid = cJSON_GetObjectItemCaseSensitive(entry, "id");
            cJSON *badge = cJSON_GetObjectItemCaseSensitive(entry, "badge_count");
            cJSON *mention = cJSON_GetObjectItemCaseSensitive(entry, "mention_count");

            if (cJSON_IsString(cid)) {
                safe_strcpy(temp_rc[temp_rc_count].channel_id, 36, cid->valuestring);
                int count = 0;
                if (cJSON_IsNumber(badge) && badge->valueint > 0) count = badge->valueint;
                else if (cJSON_IsNumber(mention) && mention->valueint > 0) count = mention->valueint;
                temp_rc[temp_rc_count].count = count;
                temp_rc_count++;
            }
        }
    }

    // 3. Private Channels (DMs)
    state->dm_count = 0;
    state->dm_total_unread = 0;
    cJSON *private_channels = cJSON_GetObjectItemCaseSensitive(d, "private_channels");
    if (cJSON_IsArray(private_channels)) {
        cJSON *chan = NULL;
        cJSON_ArrayForEach(chan, private_channels) {
            if (state->dm_count >= MAX_DMS) break;

            cJSON *cid = cJSON_GetObjectItemCaseSensitive(chan, "id");
            cJSON *last_msg = cJSON_GetObjectItemCaseSensitive(chan, "last_message_id");
            cJSON *recipients = cJSON_GetObjectItemCaseSensitive(chan, "recipients");

            if (!cJSON_IsString(cid)) continue;

            DiscordDM *dm = &state->dms[state->dm_count];
            memset(dm, 0, sizeof(DiscordDM));
            safe_strcpy(dm->id, sizeof(dm->id), cid->valuestring);

            if (cJSON_IsString(last_msg)) {
                safe_strcpy(dm->last_message_id, sizeof(dm->last_message_id), last_msg->valuestring);
            }

            // Find unread count
            for (int r = 0; r < temp_rc_count; r++) {
                if (strcmp(temp_rc[r].channel_id, dm->id) == 0) {
                    dm->unread_count = temp_rc[r].count;
                    break;
                }
            }
            state->dm_total_unread += dm->unread_count;

            // Recipient info (support single DM and multi-user Group DMs)
            int num_recipients = (cJSON_IsArray(recipients)) ? cJSON_GetArraySize(recipients) : 0;
            cJSON *cname = cJSON_GetObjectItemCaseSensitive(chan, "name");
            bool has_custom_name = (cJSON_IsString(cname) && cname->valuestring && strlen(cname->valuestring) > 0);

            if (num_recipients > 0) {
                if (has_custom_name) {
                    safe_strcpy(dm->display_name, sizeof(dm->display_name), cname->valuestring);
                } else {
                    dm->display_name[0] = '\0';
                }

                dm->username[0] = '\0';
                for (int r = 0; r < num_recipients; r++) {
                    cJSON *rec = cJSON_GetArrayItem(recipients, r);
                    if (!rec) continue;
                    cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
                    cJSON *rname = cJSON_GetObjectItemCaseSensitive(rec, "username");
                    cJSON *rgname = cJSON_GetObjectItemCaseSensitive(rec, "global_name");

                    if (r == 0 && cJSON_IsString(rid)) {
                        safe_strcpy(dm->recipient_id, sizeof(dm->recipient_id), rid->valuestring);
                    }

                    const char *u = cJSON_IsString(rname) ? rname->valuestring : "";
                    const char *g = (cJSON_IsString(rgname) && rgname->valuestring && strlen(rgname->valuestring) > 0) ? rgname->valuestring : u;

                    if (!has_custom_name) {
                        if (r > 0 && strlen(dm->display_name) > 0) {
                            strncat(dm->display_name, ", ", sizeof(dm->display_name) - strlen(dm->display_name) - 1);
                        }
                        strncat(dm->display_name, g, sizeof(dm->display_name) - strlen(dm->display_name) - 1);
                    }

                    if (strlen(u) > 0) {
                        if (r > 0 && strlen(dm->username) > 0) {
                            strncat(dm->username, ", ", sizeof(dm->username) - strlen(dm->username) - 1);
                        }
                        strncat(dm->username, u, sizeof(dm->username) - strlen(dm->username) - 1);
                    }
                }
            } else if (has_custom_name) {
                safe_strcpy(dm->display_name, sizeof(dm->display_name), cname->valuestring);
                safe_strcpy(dm->username, sizeof(dm->username), "group");
            } else {
                safe_strcpy(dm->display_name, sizeof(dm->display_name), "Group DM");
                safe_strcpy(dm->username, sizeof(dm->username), "group");
            }

            state->dm_count++;
        }

        // Check if any DMs are missing recipient details (lazy DMs in Discord v9 Gateway)
        bool dms_incomplete = false;
        for (int i = 0; i < state->dm_count; i++) {
            if (strcmp(state->dms[i].username, "group") == 0 && strcmp(state->dms[i].display_name, "Group DM") == 0) {
                dms_incomplete = true;
                break;
            }
        }

        if (dms_incomplete && token && strlen(token) > 0) {
            DiscordDM *rest_dms = (DiscordDM *)malloc(sizeof(DiscordDM) * MAX_DMS);
            int rest_dms_count = 0;
            if (rest_dms && discord_api_fetch_dms(token, rest_dms, MAX_DMS, &rest_dms_count)) {
                for (int i = 0; i < state->dm_count; i++) {
                    for (int j = 0; j < rest_dms_count; j++) {
                        if (strcmp(state->dms[i].id, rest_dms[j].id) == 0) {
                            if (strcmp(rest_dms[j].username, "group") != 0 || strlen(rest_dms[j].recipient_id) > 0) {
                                safe_strcpy(state->dms[i].recipient_id, sizeof(state->dms[i].recipient_id), rest_dms[j].recipient_id);
                                safe_strcpy(state->dms[i].username, sizeof(state->dms[i].username), rest_dms[j].username);
                                safe_strcpy(state->dms[i].display_name, sizeof(state->dms[i].display_name), rest_dms[j].display_name);
                            }
                            break;
                        }
                    }
                }
            }
            if (rest_dms) free(rest_dms);
        }

        if (state->dm_count > 1) {
            qsort(state->dms, state->dm_count, sizeof(DiscordDM), compare_dms);
        }
    }

    // 4. Raw Guilds from READY
    DiscordGuild *raw_guilds = (DiscordGuild *)malloc(sizeof(DiscordGuild) * MAX_GUILDS);
    int raw_guild_count = 0;

    if (raw_guilds) {
        cJSON *guilds = cJSON_GetObjectItemCaseSensitive(d, "guilds");
        if (cJSON_IsArray(guilds)) {
            cJSON *g = NULL;
            cJSON_ArrayForEach(g, guilds) {
                if (raw_guild_count >= MAX_GUILDS) break;

                cJSON *gid = cJSON_GetObjectItemCaseSensitive(g, "id");
                cJSON *gname = cJSON_GetObjectItemCaseSensitive(g, "name");
                cJSON *channels = cJSON_GetObjectItemCaseSensitive(g, "channels");

                if (!cJSON_IsString(gid)) continue;

                DiscordGuild *guild = &raw_guilds[raw_guild_count];
                memset(guild, 0, sizeof(DiscordGuild));
                safe_strcpy(guild->id, sizeof(guild->id), gid->valuestring);
                if (cJSON_IsString(gname)) safe_strcpy(guild->name, sizeof(guild->name), gname->valuestring);

                guild->channel_count = 0;
                guild->unread_count = 0;

                if (cJSON_IsArray(channels)) {
                    cJSON *ch = NULL;
                    cJSON_ArrayForEach(ch, channels) {
                        if (guild->channel_count >= MAX_CHANNELS_PER_GUILD) break;

                        cJSON *cid = cJSON_GetObjectItemCaseSensitive(ch, "id");
                        cJSON *cname = cJSON_GetObjectItemCaseSensitive(ch, "name");
                        cJSON *ctype = cJSON_GetObjectItemCaseSensitive(ch, "type");
                        cJSON *cpos = cJSON_GetObjectItemCaseSensitive(ch, "position");
                        cJSON *clast = cJSON_GetObjectItemCaseSensitive(ch, "last_message_id");

                        if (!cJSON_IsString(cid)) continue;

                        int type_val = cJSON_IsNumber(ctype) ? ctype->valueint : 0;
                        if (type_val != 0 && type_val != 5) continue;

                        DiscordChannel *chan_obj = &guild->channels[guild->channel_count];
                        memset(chan_obj, 0, sizeof(DiscordChannel));
                        safe_strcpy(chan_obj->id, sizeof(chan_obj->id), cid->valuestring);
                        if (cJSON_IsString(cname)) safe_strcpy(chan_obj->name, sizeof(chan_obj->name), cname->valuestring);
                        chan_obj->type = type_val;
                        chan_obj->position = cJSON_IsNumber(cpos) ? cpos->valueint : guild->channel_count;
                        if (cJSON_IsString(clast)) safe_strcpy(chan_obj->last_message_id, sizeof(chan_obj->last_message_id), clast->valuestring);

                        for (int r = 0; r < temp_rc_count; r++) {
                            if (strcmp(temp_rc[r].channel_id, chan_obj->id) == 0) {
                                chan_obj->unread_count = temp_rc[r].count;
                                break;
                            }
                        }
                        guild->unread_count += chan_obj->unread_count;
                        guild->channel_count++;
                    }
                }

                raw_guild_count++;
            }
        }

        // Fallback / Supplement: If READY guilds have missing names (lazy guilds),
        // query REST API /users/@me/guilds
        bool names_missing = false;
        for (int i = 0; i < raw_guild_count; i++) {
            if (strlen(raw_guilds[i].name) == 0) {
                names_missing = true;
                break;
            }
        }

        if (names_missing && token && strlen(token) > 0) {
            DiscordGuild *rest_guilds = (DiscordGuild *)malloc(sizeof(DiscordGuild) * MAX_GUILDS);
            int rest_count = 0;
            if (rest_guilds && discord_api_fetch_guilds(token, rest_guilds, MAX_GUILDS, &rest_count)) {
                for (int i = 0; i < raw_guild_count; i++) {
                    if (strlen(raw_guilds[i].name) == 0) {
                        for (int j = 0; j < rest_count; j++) {
                            if (strcmp(raw_guilds[i].id, rest_guilds[j].id) == 0) {
                                safe_strcpy(raw_guilds[i].name, sizeof(raw_guilds[i].name), rest_guilds[j].name);
                                break;
                            }
                        }
                    }
                }
            }
            if (rest_guilds) free(rest_guilds);
        }

        // 5. User Guild Positions (Original server order)
        cJSON *user_settings = cJSON_GetObjectItemCaseSensitive(d, "user_settings");
        cJSON *guild_positions = NULL;
        if (cJSON_IsObject(user_settings)) {
            guild_positions = cJSON_GetObjectItemCaseSensitive(user_settings, "guild_positions");
        }

        state->guild_count = 0;
        bool used_guild[MAX_GUILDS] = {0};

        if (cJSON_IsArray(guild_positions)) {
            cJSON *pos_id = NULL;
            cJSON_ArrayForEach(pos_id, guild_positions) {
                if (state->guild_count >= MAX_GUILDS) break;
                if (!cJSON_IsString(pos_id)) continue;

                for (int i = 0; i < raw_guild_count; i++) {
                    if (!used_guild[i] && strcmp(raw_guilds[i].id, pos_id->valuestring) == 0) {
                        state->guilds[state->guild_count++] = raw_guilds[i];
                        used_guild[i] = true;
                        break;
                    }
                }
            }
        }

        // Add any remaining guilds that weren't in guild_positions
        for (int i = 0; i < raw_guild_count; i++) {
            if (!used_guild[i] && state->guild_count < MAX_GUILDS) {
                state->guilds[state->guild_count++] = raw_guilds[i];
                used_guild[i] = true;
            }
        }

        free(raw_guilds);
    }

    state->is_ready = true;
    state->needs_redraw = true;

    DISCORD_MUTEX_UNLOCK(&state->lock);
    cJSON_Delete(root);
}

void discord_state_handle_guild_create(DiscordState *state, const char *json_str) {
    if (!state || !json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *d = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (!d) {
        cJSON_Delete(root);
        return;
    }

    cJSON *gid = cJSON_GetObjectItemCaseSensitive(d, "id");
    cJSON *gname = cJSON_GetObjectItemCaseSensitive(d, "name");
    cJSON *channels = cJSON_GetObjectItemCaseSensitive(d, "channels");

    if (!cJSON_IsString(gid)) {
        cJSON_Delete(root);
        return;
    }

    DISCORD_MUTEX_LOCK(&state->lock);

    // Check if guild already exists in state
    DiscordGuild *target_guild = NULL;
    for (int i = 0; i < state->guild_count; i++) {
        if (strcmp(state->guilds[i].id, gid->valuestring) == 0) {
            target_guild = &state->guilds[i];
            break;
        }
    }

    // If not found and space allows, append new guild
    if (!target_guild && state->guild_count < MAX_GUILDS) {
        target_guild = &state->guilds[state->guild_count++];
        memset(target_guild, 0, sizeof(DiscordGuild));
        safe_strcpy(target_guild->id, sizeof(target_guild->id), gid->valuestring);
    }

    if (target_guild) {
        if (cJSON_IsString(gname)) {
            safe_strcpy(target_guild->name, sizeof(target_guild->name), gname->valuestring);
        }

        if (cJSON_IsArray(channels)) {
            target_guild->channel_count = 0;
            cJSON *ch = NULL;
            cJSON_ArrayForEach(ch, channels) {
                if (target_guild->channel_count >= MAX_CHANNELS_PER_GUILD) break;

                cJSON *cid = cJSON_GetObjectItemCaseSensitive(ch, "id");
                cJSON *cname = cJSON_GetObjectItemCaseSensitive(ch, "name");
                cJSON *ctype = cJSON_GetObjectItemCaseSensitive(ch, "type");
                cJSON *cpos = cJSON_GetObjectItemCaseSensitive(ch, "position");
                cJSON *clast = cJSON_GetObjectItemCaseSensitive(ch, "last_message_id");

                if (!cJSON_IsString(cid)) continue;

                int type_val = cJSON_IsNumber(ctype) ? ctype->valueint : 0;
                if (type_val != 0 && type_val != 5) continue;

                DiscordChannel *chan_obj = &target_guild->channels[target_guild->channel_count];
                memset(chan_obj, 0, sizeof(DiscordChannel));
                safe_strcpy(chan_obj->id, sizeof(chan_obj->id), cid->valuestring);
                if (cJSON_IsString(cname)) safe_strcpy(chan_obj->name, sizeof(chan_obj->name), cname->valuestring);
                chan_obj->type = type_val;
                chan_obj->position = cJSON_IsNumber(cpos) ? cpos->valueint : target_guild->channel_count;
                if (cJSON_IsString(clast)) safe_strcpy(chan_obj->last_message_id, sizeof(chan_obj->last_message_id), clast->valuestring);

                target_guild->channel_count++;
            }
        }
        state->needs_redraw = true;
    }

    DISCORD_MUTEX_UNLOCK(&state->lock);
    cJSON_Delete(root);
}

void discord_state_handle_message_create(DiscordState *state, const char *json_str) {
    if (!state || !json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *d = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (!d) {
        cJSON_Delete(root);
        return;
    }

    cJSON *cid = cJSON_GetObjectItemCaseSensitive(d, "channel_id");
    cJSON *author = cJSON_GetObjectItemCaseSensitive(d, "author");
    cJSON *content = cJSON_GetObjectItemCaseSensitive(d, "content");
    cJSON *mid = cJSON_GetObjectItemCaseSensitive(d, "id");
    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(d, "timestamp");

    if (!cJSON_IsString(cid)) {
        cJSON_Delete(root);
        return;
    }

    DISCORD_MUTEX_LOCK(&state->lock);

    // If message is in the currently viewed channel, append to chat history
    if (strlen(state->active_channel_id) > 0 && strcmp(state->active_channel_id, cid->valuestring) == 0) {
        DiscordMessage local_msg;
        memset(&local_msg, 0, sizeof(DiscordMessage));
        if (cJSON_IsString(mid)) safe_strcpy(local_msg.id, sizeof(local_msg.id), mid->valuestring);
        if (cJSON_IsString(content)) safe_strcpy(local_msg.content, sizeof(local_msg.content), content->valuestring);

        cJSON *attachments = cJSON_GetObjectItemCaseSensitive(d, "attachments");
        cJSON *stickers = cJSON_GetObjectItemCaseSensitive(d, "sticker_items");
        cJSON *embeds = cJSON_GetObjectItemCaseSensitive(d, "embeds");

        int num_att = (cJSON_IsArray(attachments)) ? cJSON_GetArraySize(attachments) : 0;
        if (num_att > 0) {
            local_msg.has_attachment = true;
            cJSON *first_att = cJSON_GetArrayItem(attachments, 0);
            cJSON *fname = first_att ? cJSON_GetObjectItemCaseSensitive(first_att, "filename") : NULL;
            if (cJSON_IsString(fname) && fname->valuestring) {
                if (num_att > 1) {
                    snprintf(local_msg.attachment_name, sizeof(local_msg.attachment_name), "%s (+%d)", fname->valuestring, num_att - 1);
                } else {
                    safe_strcpy(local_msg.attachment_name, sizeof(local_msg.attachment_name), fname->valuestring);
                }
            }
        } else if (cJSON_IsArray(stickers) && cJSON_GetArraySize(stickers) > 0) {
            local_msg.has_attachment = true;
            cJSON *first_stk = cJSON_GetArrayItem(stickers, 0);
            cJSON *sname = first_stk ? cJSON_GetObjectItemCaseSensitive(first_stk, "name") : NULL;
            if (cJSON_IsString(sname) && sname->valuestring) {
                snprintf(local_msg.attachment_name, sizeof(local_msg.attachment_name), "sticker: %s", sname->valuestring);
            }
        } else if (cJSON_IsArray(embeds) && cJSON_GetArraySize(embeds) > 0 && strlen(local_msg.content) == 0) {
            local_msg.has_attachment = true;
        }

        if (author) {
            cJSON *aid = cJSON_GetObjectItemCaseSensitive(author, "id");
            cJSON *aname = cJSON_GetObjectItemCaseSensitive(author, "username");
            cJSON *agname = cJSON_GetObjectItemCaseSensitive(author, "global_name");

            if (cJSON_IsString(aid)) safe_strcpy(local_msg.author_id, sizeof(local_msg.author_id), aid->valuestring);
            if (cJSON_IsString(aname)) safe_strcpy(local_msg.author_name, sizeof(local_msg.author_name), aname->valuestring);
            if (cJSON_IsString(agname) && agname->valuestring) {
                safe_strcpy(local_msg.author_global_name, sizeof(local_msg.author_global_name), agname->valuestring);
            } else {
                safe_strcpy(local_msg.author_global_name, sizeof(local_msg.author_global_name), local_msg.author_name);
            }
        }

        if (cJSON_IsString(timestamp) && strlen(timestamp->valuestring) >= 16) {
            const char *t = strchr(timestamp->valuestring, 'T');
            if (t && strlen(t) >= 6) {
                snprintf(local_msg.timestamp, sizeof(local_msg.timestamp), "%.5s", t + 1);
            } else {
                safe_strcpy(local_msg.timestamp, sizeof(local_msg.timestamp), "now");
            }
        } else {
            safe_strcpy(local_msg.timestamp, sizeof(local_msg.timestamp), "now");
        }

        if (state->current_message_count < MAX_CACHED_MESSAGES) {
            state->current_messages[state->current_message_count++] = local_msg;
        } else {
            for (int i = 0; i < MAX_CACHED_MESSAGES - 1; i++) {
                state->current_messages[i] = state->current_messages[i + 1];
            }
            state->current_messages[MAX_CACHED_MESSAGES - 1] = local_msg;
        }
        state->needs_redraw = true;
    } else {
        // Increment unread count for the channel / DM
        bool found_dm = false;
        for (int i = 0; i < state->dm_count; i++) {
            if (strcmp(state->dms[i].id, cid->valuestring) == 0) {
                state->dms[i].unread_count++;
                state->dm_total_unread++;
                if (cJSON_IsString(mid)) {
                    safe_strcpy(state->dms[i].last_message_id, sizeof(state->dms[i].last_message_id), mid->valuestring);
                }
                found_dm = true;
                break;
            }
        }

        if (!found_dm) {
            for (int g = 0; g < state->guild_count; g++) {
                for (int c = 0; c < state->guilds[g].channel_count; c++) {
                    if (strcmp(state->guilds[g].channels[c].id, cid->valuestring) == 0) {
                        state->guilds[g].channels[c].unread_count++;
                        state->guilds[g].unread_count++;
                        break;
                    }
                }
            }
        }
        state->needs_redraw = true;
    }

    DISCORD_MUTEX_UNLOCK(&state->lock);
    cJSON_Delete(root);
}

void discord_state_handle_message_ack(DiscordState *state, const char *json_str) {
    if (!state || !json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *d = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (!d) {
        cJSON_Delete(root);
        return;
    }

    cJSON *cid = cJSON_GetObjectItemCaseSensitive(d, "channel_id");
    if (!cJSON_IsString(cid)) {
        cJSON_Delete(root);
        return;
    }

    DISCORD_MUTEX_LOCK(&state->lock);

    for (int i = 0; i < state->dm_count; i++) {
        if (strcmp(state->dms[i].id, cid->valuestring) == 0) {
            state->dm_total_unread -= state->dms[i].unread_count;
            if (state->dm_total_unread < 0) state->dm_total_unread = 0;
            state->dms[i].unread_count = 0;
            break;
        }
    }

    for (int g = 0; g < state->guild_count; g++) {
        for (int c = 0; c < state->guilds[g].channel_count; c++) {
            if (strcmp(state->guilds[g].channels[c].id, cid->valuestring) == 0) {
                state->guilds[g].unread_count -= state->guilds[g].channels[c].unread_count;
                if (state->guilds[g].unread_count < 0) state->guilds[g].unread_count = 0;
                state->guilds[g].channels[c].unread_count = 0;
                break;
            }
        }
    }

    state->needs_redraw = true;
    DISCORD_MUTEX_UNLOCK(&state->lock);
    cJSON_Delete(root);
}

void discord_state_set_active_channel(DiscordState *state, const char *channel_id, const char *channel_name) {
    if (!state) return;
    DISCORD_MUTEX_LOCK(&state->lock);

    safe_strcpy(state->active_channel_id, sizeof(state->active_channel_id), channel_id);
    safe_strcpy(state->active_channel_name, sizeof(state->active_channel_name), channel_name);
    state->current_message_count = 0;

    for (int i = 0; i < state->dm_count; i++) {
        if (strcmp(state->dms[i].id, channel_id) == 0) {
            state->dm_total_unread -= state->dms[i].unread_count;
            if (state->dm_total_unread < 0) state->dm_total_unread = 0;
            state->dms[i].unread_count = 0;
            break;
        }
    }

    for (int g = 0; g < state->guild_count; g++) {
        for (int c = 0; c < state->guilds[g].channel_count; c++) {
            if (strcmp(state->guilds[g].channels[c].id, channel_id) == 0) {
                state->guilds[g].unread_count -= state->guilds[g].channels[c].unread_count;
                if (state->guilds[g].unread_count < 0) state->guilds[g].unread_count = 0;
                state->guilds[g].channels[c].unread_count = 0;
                break;
            }
        }
    }

    state->needs_redraw = true;
    DISCORD_MUTEX_UNLOCK(&state->lock);
}

void discord_state_clear_active_channel(DiscordState *state) {
    if (!state) return;
    DISCORD_MUTEX_LOCK(&state->lock);
    state->active_channel_id[0] = '\0';
    state->active_channel_name[0] = '\0';
    state->current_message_count = 0;
    state->needs_redraw = true;
    DISCORD_MUTEX_UNLOCK(&state->lock);
}

void discord_state_set_messages(DiscordState *state, const DiscordMessage *msgs, int count) {
    if (!state) return;
    DISCORD_MUTEX_LOCK(&state->lock);
    state->current_message_count = (count > MAX_CACHED_MESSAGES) ? MAX_CACHED_MESSAGES : count;
    for (int i = 0; i < state->current_message_count; i++) {
        state->current_messages[i] = msgs[i];
    }
    state->needs_redraw = true;
    DISCORD_MUTEX_UNLOCK(&state->lock);
}

void discord_state_add_message(DiscordState *state, const DiscordMessage *msg) {
    if (!state || !msg) return;
    DISCORD_MUTEX_LOCK(&state->lock);
    if (state->current_message_count < MAX_CACHED_MESSAGES) {
        state->current_messages[state->current_message_count++] = *msg;
    } else {
        for (int i = 0; i < MAX_CACHED_MESSAGES - 1; i++) {
            state->current_messages[i] = state->current_messages[i + 1];
        }
        state->current_messages[MAX_CACHED_MESSAGES - 1] = *msg;
    }
    state->needs_redraw = true;
    DISCORD_MUTEX_UNLOCK(&state->lock);
}
