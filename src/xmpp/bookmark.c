/*
 * bookmark.c
 *
 * Copyright (C) 2012 - 2014 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link the code of portions of this program with the OpenSSL library under
 * certain conditions as described in each individual source file, and
 * distribute linked combinations including the two.
 *
 * You must obey the GNU General Public License in all respects for all of the
 * code used other than OpenSSL. If you modify file(s) with this exception, you
 * may extend this exception to your version of the file(s), but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version. If you delete this exception statement from all
 * source files in the program, then also delete it here.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <strophe.h>

#include "common.h"
#include "log.h"
#include "muc.h"
#include "server_events.h"
#include "xmpp/connection.h"
#include "xmpp/stanza.h"
#include "xmpp/xmpp.h"
#include "xmpp/bookmark.h"
#include "ui/ui.h"

#define BOOKMARK_TIMEOUT 5000
#define DEFAULT_STORAGE STORAGE_PRIVATE

static Autocomplete bookmark_ac;
static GList *bookmark_list;

static int _bookmark_handle_private_storage_result(xmpp_conn_t * const conn,
    xmpp_stanza_t * const stanza, void * const userdata);
static void _send_private_storage_bookmarks_update(void);
static int _delete_private_storage_request_handler(xmpp_conn_t * const conn,
    void * const userdata);

static int _bookmark_handle_pubsub_result(xmpp_conn_t * const conn,
    xmpp_stanza_t * const stanza, void * const userdata);
static void _send_pubsub_bookmarks_update(void);
static int _delete_pubsub_request_handler(xmpp_conn_t * const conn,
    void * const userdata);

static void _bookmark_item_destroy(gpointer item);
static int _match_bookmark_by_jid(gconstpointer a, gconstpointer b);

void
bookmark_request(void)
{
    xmpp_conn_t *conn = connection_get_conn();
    xmpp_ctx_t *ctx = connection_get_ctx();

    autocomplete_free(bookmark_ac);
    bookmark_ac = autocomplete_new();
    if (bookmark_list != NULL) {
        g_list_free_full(bookmark_list, _bookmark_item_destroy);
        bookmark_list = NULL;
    }

    // query for private storage bookmarks
    char *private_id = strdup("bookmark_private_storage_req");
    xmpp_timed_handler_add(conn, _delete_private_storage_request_handler, BOOKMARK_TIMEOUT, private_id);
    xmpp_id_handler_add(conn, _bookmark_handle_private_storage_result, private_id, private_id);

    xmpp_stanza_t *private_iq = stanza_create_bookmarks_private_storage_request(ctx);
    xmpp_stanza_set_id(private_iq, private_id);
    xmpp_send(conn, private_iq);
    xmpp_stanza_release(private_iq);

    // TODO query for pubsub bookmarks
    char *pubsub_id = strdup("bookmark_pubsub_req");
    xmpp_timed_handler_add(conn, _delete_pubsub_request_handler, BOOKMARK_TIMEOUT, pubsub_id);
    xmpp_id_handler_add(conn, _bookmark_handle_pubsub_result, pubsub_id, pubsub_id);

    xmpp_stanza_t *pubsub_iq = stanza_create_bookmarks_pubsub_request(ctx);
    xmpp_stanza_set_id(pubsub_iq, pubsub_id);
    xmpp_send(conn, pubsub_iq);
    xmpp_stanza_release(pubsub_iq);
}

gboolean
bookmark_add(const char *jid, const char *nick, const char *password, const char *autojoin_str)
{
    if (autocomplete_contains(bookmark_ac, jid)) {
        return FALSE;
    } else {
        Bookmark *item = malloc(sizeof(*item));
        item->jid = strdup(jid);

        // TODO check for account storage preference
        item->storage = DEFAULT_STORAGE;

        if (nick != NULL) {
            item->nick = strdup(nick);
        } else {
            item->nick = NULL;
        }
        if (password != NULL) {
            item->password = strdup(password);
        } else {
            item->password = NULL;
        }

        if (g_strcmp0(autojoin_str, "on") == 0) {
            item->autojoin = TRUE;
        } else {
            item->autojoin = FALSE;
        }

        bookmark_list = g_list_append(bookmark_list, item);
        autocomplete_add(bookmark_ac, jid);

        if (item->storage == STORAGE_PRIVATE) {
            _send_private_storage_bookmarks_update();
        } else {
            _send_pubsub_bookmarks_update();
        }

        return TRUE;
    }
}

gboolean
bookmark_update(const char *jid, const char *nick, const char *password, const char *autojoin_str)
{
    Bookmark *item = malloc(sizeof(*item));
    item->jid = strdup(jid);
    item->nick = NULL;
    item->password = NULL;
    item->autojoin = FALSE;

    GList *found = g_list_find_custom(bookmark_list, item, _match_bookmark_by_jid);
    _bookmark_item_destroy(item);
    if (found == NULL) {
        return FALSE;
    } else {
        Bookmark *bm = found->data;
        if (nick != NULL) {
            free(bm->nick);
            bm->nick = strdup(nick);
        }
        if (password != NULL) {
            free(bm->password);
            bm->password = strdup(password);
        }
        if (autojoin_str != NULL) {
            if (g_strcmp0(autojoin_str, "on") == 0) {
                bm->autojoin = TRUE;
            } else if (g_strcmp0(autojoin_str, "off") == 0) {
                bm->autojoin = FALSE;
            }
        }

        if (bm->storage == STORAGE_PRIVATE) {
            _send_private_storage_bookmarks_update();
        } else {
            _send_pubsub_bookmarks_update();
        }

        return TRUE;
    }
}

gboolean
bookmark_join(const char *jid)
{
    Bookmark *item = malloc(sizeof(*item));
    item->jid = strdup(jid);
    item->nick = NULL;
    item->password = NULL;
    item->autojoin = FALSE;

    GList *found = g_list_find_custom(bookmark_list, item, _match_bookmark_by_jid);
    _bookmark_item_destroy(item);
    if (found == NULL) {
        return FALSE;
    } else {
        char *account_name = jabber_get_account_name();
        ProfAccount *account = accounts_get_account(account_name);
        Bookmark *item = found->data;
        if (!muc_active(item->jid)) {
            char *nick = item->nick;
            if (nick == NULL) {
                nick = account->muc_nick;
            }
            presence_join_room(item->jid, nick, item->password);
            muc_join(item->jid, nick, item->password, FALSE);
            account_free(account);
        } else if (muc_roster_complete(item->jid)) {
            ui_room_join(item->jid, TRUE);
        }
        return TRUE;
    }
}

gboolean
bookmark_remove(const char *jid)
{
    Bookmark *item = malloc(sizeof(*item));
    item->jid = strdup(jid);
    item->nick = NULL;
    item->password = NULL;
    item->autojoin = FALSE;

    GList *found = g_list_find_custom(bookmark_list, item, _match_bookmark_by_jid);
    _bookmark_item_destroy(item);
    gboolean remove = found != NULL;

    if (remove) {
        storage_t storage = ((Bookmark *)found->data)->storage;
        bookmark_list = g_list_remove_link(bookmark_list, found);
        _bookmark_item_destroy(found->data);
        g_list_free(found);
        autocomplete_remove(bookmark_ac, jid);

        if (storage == STORAGE_PRIVATE) {
            _send_private_storage_bookmarks_update();
        } else {
            _send_pubsub_bookmarks_update();
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

const GList *
bookmark_get_list(void)
{
    return bookmark_list;
}

char *
bookmark_find(char *search_str)
{
    return autocomplete_complete(bookmark_ac, search_str, TRUE);
}

void
bookmark_autocomplete_reset(void)
{
    if (bookmark_ac != NULL) {
        autocomplete_reset(bookmark_ac);
    }
}

static int
_bookmark_handle_private_storage_result(xmpp_conn_t * const conn,
    xmpp_stanza_t * const stanza, void * const userdata)
{
    xmpp_ctx_t *ctx = connection_get_ctx();
    char *id = (char *)userdata;
    xmpp_stanza_t *ptr;
    xmpp_stanza_t *nick;
    xmpp_stanza_t *password_st;
    char *name;
    char *jid;
    char *autojoin;
    char *password;
    gboolean autojoin_val;
    Jid *my_jid;
    Bookmark *item;

    xmpp_timed_handler_delete(conn, _delete_private_storage_request_handler);
    g_free(id);

    name = xmpp_stanza_get_name(stanza);
    if (!name || strcmp(name, STANZA_NAME_IQ) != 0) {
        return 0;
    }

    ptr = xmpp_stanza_get_child_by_name(stanza, STANZA_NAME_QUERY);
    if (!ptr) {
        return 0;
    }
    ptr = xmpp_stanza_get_child_by_name(ptr, STANZA_NAME_STORAGE);
    if (!ptr) {
        return 0;
    }

    if (bookmark_ac == NULL) {
        bookmark_ac = autocomplete_new();
    }
    my_jid = jid_create(jabber_get_fulljid());

    ptr = xmpp_stanza_get_children(ptr);
    while (ptr) {
        name = xmpp_stanza_get_name(ptr);
        if (!name || strcmp(name, STANZA_NAME_CONFERENCE) != 0) {
            ptr = xmpp_stanza_get_next(ptr);
            continue;
        }
        jid = xmpp_stanza_get_attribute(ptr, STANZA_ATTR_JID);
        if (!jid) {
            ptr = xmpp_stanza_get_next(ptr);
            continue;
        }

        log_debug("Handle bookmark for %s", jid);

        name = NULL;
        nick = xmpp_stanza_get_child_by_name(ptr, "nick");
        if (nick) {
            char *tmp;
            tmp = xmpp_stanza_get_text(nick);
            if (tmp) {
                name = strdup(tmp);
                xmpp_free(ctx, tmp);
            }
        }

        password = NULL;
        password_st = xmpp_stanza_get_child_by_name(ptr, "password");
        if (password_st) {
            char *tmp;
            tmp = xmpp_stanza_get_text(password_st);
            if (tmp) {
                password = strdup(tmp);
                xmpp_free(ctx, tmp);
            }
        }

        autojoin = xmpp_stanza_get_attribute(ptr, "autojoin");
        if (autojoin && (strcmp(autojoin, "1") == 0 || strcmp(autojoin, "true") == 0)) {
            autojoin_val = TRUE;
        } else {
            autojoin_val = FALSE;
        }

        autocomplete_add(bookmark_ac, jid);
        item = malloc(sizeof(*item));
        item->jid = strdup(jid);
        item->storage = STORAGE_PRIVATE;
        item->nick = name;
        item->password = password;
        item->autojoin = autojoin_val;
        bookmark_list = g_list_append(bookmark_list, item);

        if (autojoin_val) {
            Jid *room_jid;

            char *account_name = jabber_get_account_name();
            ProfAccount *account = accounts_get_account(account_name);
            if (name == NULL) {
                name = account->muc_nick;
            }

            log_debug("Autojoin %s with nick=%s", jid, name);
            room_jid = jid_create_from_bare_and_resource(jid, name);
            if (!muc_active(room_jid->barejid)) {
                presence_join_room(jid, name, password);
                muc_join(jid, name, password, TRUE);
            }
            jid_destroy(room_jid);
            account_free(account);
        }

        ptr = xmpp_stanza_get_next(ptr);
    }

    jid_destroy(my_jid);

    return 0;
}

static int
_bookmark_handle_pubsub_result(xmpp_conn_t * const conn,
    xmpp_stanza_t * const stanza, void * const userdata)
{
    return 0;
}


static int
_delete_private_storage_request_handler(xmpp_conn_t * const conn,
    void * const userdata)
{
    char *id = (char *)userdata;
    assert(id != NULL);
    log_debug("Timeout for handler with id=%s", id);

    xmpp_id_handler_delete(conn, _bookmark_handle_private_storage_result, id);
    g_free(id);

    return 0;
}

static int
_delete_pubsub_request_handler(xmpp_conn_t * const conn,
    void * const userdata)
{
    char *id = (char *)userdata;
    assert(id != NULL);
    log_debug("Timeout for handler with id=%s", id);

    xmpp_id_handler_delete(conn, _bookmark_handle_pubsub_result, id);
    g_free(id);

    return 0;
}

static void
_bookmark_item_destroy(gpointer item)
{
    Bookmark *p = (Bookmark *)item;

    if (p == NULL) {
        return;
    }

    free(p->jid);
    free(p->nick);
    free(p->password);
    free(p);
}

static int
_match_bookmark_by_jid(gconstpointer a, gconstpointer b)
{
    Bookmark *bookmark_a = (Bookmark *) a;
    Bookmark *bookmark_b = (Bookmark *) b;

    return strcmp(bookmark_a->jid, bookmark_b->jid);
}

static void
_send_private_storage_bookmarks_update(void)
{
    xmpp_conn_t *conn = connection_get_conn();
    xmpp_ctx_t *ctx = connection_get_ctx();

    xmpp_stanza_t *iq = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(iq, STANZA_NAME_IQ);
    char *id = create_unique_id("bookmark_private_storage_update");
    xmpp_stanza_set_id(iq, id);
    free(id);
    xmpp_stanza_set_type(iq, STANZA_TYPE_SET);

    xmpp_stanza_t *query = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(query, STANZA_NAME_QUERY);
    xmpp_stanza_set_ns(query, "jabber:iq:private");

    xmpp_stanza_t *storage = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(storage, STANZA_NAME_STORAGE);
    xmpp_stanza_set_ns(storage, "storage:bookmarks");

    GList *curr = bookmark_list;
    while (curr != NULL) {
        Bookmark *bookmark = curr->data;
        if (bookmark->storage == STORAGE_PRIVATE) {
            xmpp_stanza_t *conference = xmpp_stanza_new(ctx);
            xmpp_stanza_set_name(conference, STANZA_NAME_CONFERENCE);
            xmpp_stanza_set_attribute(conference, STANZA_ATTR_JID, bookmark->jid);

            Jid *jidp = jid_create(bookmark->jid);
            if (jidp->localpart != NULL) {
                xmpp_stanza_set_attribute(conference, STANZA_ATTR_NAME, jidp->localpart);
            }
            jid_destroy(jidp);

            if (bookmark->autojoin) {
                xmpp_stanza_set_attribute(conference, STANZA_ATTR_AUTOJOIN, "true");
            } else {
                xmpp_stanza_set_attribute(conference, STANZA_ATTR_AUTOJOIN, "false");
            }

            if (bookmark->nick != NULL) {
                xmpp_stanza_t *nick_st = xmpp_stanza_new(ctx);
                xmpp_stanza_set_name(nick_st, STANZA_NAME_NICK);
                xmpp_stanza_t *nick_text = xmpp_stanza_new(ctx);
                xmpp_stanza_set_text(nick_text, bookmark->nick);
                xmpp_stanza_add_child(nick_st, nick_text);
                xmpp_stanza_add_child(conference, nick_st);

                xmpp_stanza_release(nick_text);
                xmpp_stanza_release(nick_st);
            }

            if (bookmark->password != NULL) {
                xmpp_stanza_t *password_st = xmpp_stanza_new(ctx);
                xmpp_stanza_set_name(password_st, STANZA_NAME_PASSWORD);
                xmpp_stanza_t *password_text = xmpp_stanza_new(ctx);
                xmpp_stanza_set_text(password_text, bookmark->password);
                xmpp_stanza_add_child(password_st, password_text);
                xmpp_stanza_add_child(conference, password_st);

                xmpp_stanza_release(password_text);
                xmpp_stanza_release(password_st);
            }

            xmpp_stanza_add_child(storage, conference);
            xmpp_stanza_release(conference);
        }

        curr = curr->next;
    }

    xmpp_stanza_add_child(query, storage);
    xmpp_stanza_add_child(iq, query);
    xmpp_stanza_release(storage);
    xmpp_stanza_release(query);

    xmpp_send(conn, iq);
    xmpp_stanza_release(iq);
}

static void
_send_pubsub_bookmarks_update(void)
{
    xmpp_conn_t *conn = connection_get_conn();
    xmpp_ctx_t *ctx = connection_get_ctx();

    xmpp_stanza_t *iq = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(iq, STANZA_NAME_IQ);
    char *id = create_unique_id("bookmark_private_storage_update");
    xmpp_stanza_set_id(iq, id);
    xmpp_stanza_set_type(iq, STANZA_TYPE_SET);

    xmpp_stanza_t *pubsub = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(pubsub, STANZA_NAME_PUBSUB);
    xmpp_stanza_set_ns(pubsub, STANZA_NS_PUBSUB);

    xmpp_stanza_t *publish = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(publish, STANZA_NAME_PUBLISH);
    xmpp_stanza_set_attribute(publish, STANZA_ATTR_NODE, "storage:bookmarks");

    GList *curr = bookmark_list;
    while (curr != NULL) {
        Bookmark *bookmark = curr->data;
        if (bookmark->storage == STORAGE_PUBSUB) {
            xmpp_stanza_t *item = xmpp_stanza_new(ctx);
            xmpp_stanza_set_name(item, STANZA_NAME_ITEM);

            xmpp_stanza_t *conference = xmpp_stanza_new(ctx);
            xmpp_stanza_set_name(conference, STANZA_NAME_CONFERENCE);
            xmpp_stanza_set_attribute(conference, STANZA_ATTR_JID, bookmark->jid);

            Jid *jidp = jid_create(bookmark->jid);
            xmpp_stanza_set_attribute(conference, STANZA_ATTR_NAME, jidp->localpart);
            jid_destroy(jidp);

            if (bookmark->autojoin) {
                xmpp_stanza_set_attribute(conference, STANZA_ATTR_AUTOJOIN, "true");
            } else {
                xmpp_stanza_set_attribute(conference, STANZA_ATTR_AUTOJOIN, "false");
            }

            if (bookmark->nick != NULL) {
                xmpp_stanza_t *nick_st = xmpp_stanza_new(ctx);
                xmpp_stanza_set_name(nick_st, STANZA_NAME_NICK);
                xmpp_stanza_t *nick_text = xmpp_stanza_new(ctx);
                xmpp_stanza_set_text(nick_text, bookmark->nick);
                xmpp_stanza_add_child(nick_st, nick_text);
                xmpp_stanza_add_child(conference, nick_st);

                xmpp_stanza_release(nick_text);
                xmpp_stanza_release(nick_st);
            }

            if (bookmark->password != NULL) {
                xmpp_stanza_t *password_st = xmpp_stanza_new(ctx);
                xmpp_stanza_set_name(password_st, STANZA_NAME_PASSWORD);
                xmpp_stanza_t *password_text = xmpp_stanza_new(ctx);
                xmpp_stanza_set_text(password_text, bookmark->password);
                xmpp_stanza_add_child(password_st, password_text);
                xmpp_stanza_add_child(conference, password_st);

                xmpp_stanza_release(password_text);
                xmpp_stanza_release(password_st);
            }

            xmpp_stanza_add_child(item, conference);
            xmpp_stanza_release(conference);
            xmpp_stanza_add_child(publish, item);
            xmpp_stanza_release(item);
        }

        curr = curr->next;
    }

    xmpp_stanza_add_child(pubsub, publish);
    xmpp_stanza_release(publish);

    xmpp_stanza_t *publish_options = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(publish_options, STANZA_NAME_PUBLISH_OPTIONS);

    xmpp_stanza_t *x = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(x, STANZA_NAME_X);
    xmpp_stanza_set_ns(x, STANZA_NS_DATA);
    xmpp_stanza_set_attribute(x, STANZA_ATTR_TYPE, "submit");

    xmpp_stanza_t *form_type = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(form_type, STANZA_NAME_FIELD);
    xmpp_stanza_set_attribute(form_type, STANZA_ATTR_VAR, "FORM_TYPE");
    xmpp_stanza_set_attribute(form_type, STANZA_ATTR_TYPE, "hidden");
    xmpp_stanza_t *form_type_value = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(form_type_value, STANZA_NAME_VALUE);
    xmpp_stanza_t *form_type_value_text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(form_type_value_text, "http://jabber.org/protocol/pubsub#publish-options");
    xmpp_stanza_add_child(form_type_value, form_type_value_text);
    xmpp_stanza_add_child(form_type, form_type_value);
    xmpp_stanza_add_child(x, form_type);
    xmpp_stanza_release(form_type);

    xmpp_stanza_t *persist_items = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(persist_items, STANZA_NAME_FIELD);
    xmpp_stanza_set_attribute(persist_items, STANZA_ATTR_VAR, "pubsub#persist_items");
    xmpp_stanza_t *persist_items_value = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(persist_items_value, STANZA_NAME_VALUE);
    xmpp_stanza_t *persist_items_value_text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(persist_items_value_text, "true");
    xmpp_stanza_add_child(persist_items_value, persist_items_value_text);
    xmpp_stanza_add_child(persist_items, persist_items_value);
    xmpp_stanza_add_child(x, persist_items);
    xmpp_stanza_release(persist_items);

    xmpp_stanza_t *access_model = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(access_model, STANZA_NAME_FIELD);
    xmpp_stanza_set_attribute(access_model, STANZA_ATTR_VAR, "pubsub#access_model");
    xmpp_stanza_t *access_model_value = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(access_model_value, STANZA_NAME_VALUE);
    xmpp_stanza_t *access_model_value_text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(access_model_value_text, "whitelist");
    xmpp_stanza_add_child(access_model_value, access_model_value_text);
    xmpp_stanza_add_child(access_model, access_model_value);
    xmpp_stanza_add_child(x, access_model);
    xmpp_stanza_release(access_model);

    xmpp_stanza_add_child(publish_options, x);
    xmpp_stanza_release(x);

    xmpp_stanza_add_child(pubsub, publish_options);
    xmpp_stanza_release(publish_options);

    xmpp_stanza_add_child(iq, pubsub);
    xmpp_stanza_release(pubsub);

    xmpp_send(conn, iq);
    xmpp_stanza_release(iq);
}
