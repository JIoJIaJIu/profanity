/*
 * window.h
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

#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "config.h"

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "contact.h"
#include "muc.h"
#include "ui/buffer.h"
#include "xmpp/xmpp.h"

#define NO_ME           1
#define NO_DATE         2
#define NO_EOL          4
#define NO_COLOUR_FROM  8
#define NO_COLOUR_DATE  16

#define PAD_SIZE 1000

typedef enum {
    WIN_CONSOLE,
    WIN_CHAT,
    WIN_MUC,
    WIN_MUC_CONFIG,
    WIN_PRIVATE,
    WIN_XML
} win_type_t;

typedef struct prof_win_t {
    win_type_t type;

    WINDOW *win;
    ProfBuff buffer;
    char *from;
    int y_pos;
    int paged;
    int unread;
    union {
        // WIN_CONSOLE
        struct {
            WINDOW *subwin;
            int sub_y_pos;
        } cons;

        // WIN_CHAT
        struct {
            gboolean is_otr;
            gboolean is_trusted;
            char *resource;
            gboolean history_shown;
        } chat;

        // WIN_MUC
        struct {
            WINDOW *subwin;
            int sub_y_pos;
        } muc;

        // WIN_MUC_CONFIG
        struct {
            DataForm *form;
        } conf;

        // WIN_PRIVATE
        struct {
        } priv;

        // WIN_XML
        struct {
        } xml;
    } wins;
} ProfWin;

ProfWin* win_create_console(void);
ProfWin* win_create_chat(const char * const barejid);
ProfWin* win_create_muc(const char * const roomjid);
ProfWin* win_create_muc_config(const char * const title, DataForm *form);
ProfWin* win_create_private(const char * const fulljid);
ProfWin* win_create_xmlconsole(void);

void win_free(ProfWin *window);
void win_update_virtual(ProfWin *window);
void win_move_to_end(ProfWin *window);
void win_show_contact(ProfWin *window, PContact contact);
void win_show_occupant(ProfWin *window, Occupant *occupant);
void win_show_status_string(ProfWin *window, const char * const from,
    const char * const show, const char * const status,
    GDateTime *last_activity, const char * const pre,
    const char * const default_show);
void win_print_incoming_message(ProfWin *window, GTimeVal *tv_stamp,
    const char * const from, const char * const message);
void win_show_info(ProfWin *window, PContact contact);
void win_show_occupant_info(ProfWin *window, const char * const room, Occupant *occupant);
void win_save_vprint(ProfWin *window, const char show_char, GTimeVal *tstamp, int flags, theme_item_t theme_item, const char * const from, const char * const message, ...);
void win_save_print(ProfWin *window, const char show_char, GTimeVal *tstamp, int flags, theme_item_t theme_item, const char * const from, const char * const message);
void win_save_println(ProfWin *window, const char * const message);
void win_save_newline(ProfWin *window);
void win_redraw(ProfWin *window);
void win_hide_subwin(ProfWin *window);
void win_show_subwin(ProfWin *window);
int win_roster_cols(void);
int win_occpuants_cols(void);
void win_printline_nowrap(WINDOW *win, char *msg);
gboolean win_is_otr(ProfWin *window);
gboolean win_is_trusted(ProfWin *window);

#endif
