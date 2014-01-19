#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <ftw.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "config/preferences.h"

#include "ui/ui.h"
#include "ui/mock_ui.h"

#include "command/commands.h"

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

static int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

void create_config_dir(void **state)
{
    setenv("XDG_CONFIG_HOME", "./tests/files/xdg_config_home", 1);
    gchar *xdg_config = xdg_get_config_home();

    GString *profanity_dir = g_string_new(xdg_config);
    g_string_append(profanity_dir, "/profanity");

    if (!mkdir_recursive(profanity_dir->str)) {
        assert_true(FALSE);
    }
    g_string_free(profanity_dir, TRUE);

    g_free(xdg_config);
}

void delete_config_dir(void **state)
{
   rmrf("./tests/files");
}

void cmd_statuses_shows_usage_when_bad_subcmd(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    help->usage = "some usage";
    gchar *args[] = { "badcmd", NULL };

    expect_cons_show("Usage: some usage");

    gboolean result = cmd_statuses(args, *help);
    assert_true(result);

    free(help);
}

void cmd_statuses_shows_usage_when_bad_console_setting(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    help->usage = "some usage";
    gchar *args[] = { "console", "badsetting", NULL };

    expect_cons_show("Usage: some usage");

    gboolean result = cmd_statuses(args, *help);
    assert_true(result);

    free(help);
}

void cmd_statuses_shows_usage_when_bad_chat_setting(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    help->usage = "some usage";
    gchar *args[] = { "chat", "badsetting", NULL };

    expect_cons_show("Usage: some usage");

    gboolean result = cmd_statuses(args, *help);
    assert_true(result);

    free(help);
}

void cmd_statuses_shows_usage_when_bad_muc_setting(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    help->usage = "some usage";
    gchar *args[] = { "muc", "badsetting", NULL };

    expect_cons_show("Usage: some usage");

    gboolean result = cmd_statuses(args, *help);
    assert_true(result);

    free(help);
}

void cmd_statuses_console_sets_all(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "console", "all", NULL };

    expect_cons_show("All presence updates will appear in the console.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CONSOLE);
    assert_non_null(setting);
    assert_string_equal("all", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_console_sets_online(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "console", "online", NULL };

    expect_cons_show("Only online/offline presence updates will appear in the console.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CONSOLE);
    assert_non_null(setting);
    assert_string_equal("online", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_console_sets_none(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "console", "none", NULL };

    expect_cons_show("Presence updates will not appear in the console.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CONSOLE);
    assert_non_null(setting);
    assert_string_equal("none", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_chat_sets_all(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "chat", "all", NULL };

    expect_cons_show("All presence updates will appear in chat windows.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CHAT);
    assert_non_null(setting);
    assert_string_equal("all", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_chat_sets_online(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "chat", "online", NULL };

    expect_cons_show("Only online/offline presence updates will appear in chat windows.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CHAT);
    assert_non_null(setting);
    assert_string_equal("online", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_chat_sets_none(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "chat", "none", NULL };

    expect_cons_show("Presence updates will not appear in chat windows.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    char *setting = prefs_get_string(PREF_STATUSES_CHAT);
    assert_non_null(setting);
    assert_string_equal("none", setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_muc_sets_on(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "muc", "on", NULL };

    expect_cons_show("Chat room presence updates enabled.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    gboolean setting = prefs_get_boolean(PREF_STATUSES_MUC);
    assert_non_null(setting);
    assert_true(setting);
    assert_true(result);

    prefs_close();
    free(help);
}

void cmd_statuses_muc_sets_off(void **state)
{
    mock_cons_show();
    CommandHelp *help = malloc(sizeof(CommandHelp));
    gchar *args[] = { "muc", "off", NULL };

    expect_cons_show("Chat room presence updates disabled.");

    prefs_load();
    gboolean result = cmd_statuses(args, *help);
    prefs_close();
    prefs_load();

    gboolean setting = prefs_get_boolean(PREF_STATUSES_MUC);
    assert_false(setting);
    assert_true(result);

    prefs_close();
    free(help);
}