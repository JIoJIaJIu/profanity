void create_config_dir(void **state);
void delete_config_dir(void **state);

void cmd_statuses_shows_usage_when_bad_subcmd(void **state);
void cmd_statuses_shows_usage_when_bad_console_setting(void **state);
void cmd_statuses_shows_usage_when_bad_chat_setting(void **state);
void cmd_statuses_shows_usage_when_bad_muc_setting(void **state);
void cmd_statuses_console_sets_all(void **state);
void cmd_statuses_console_sets_online(void **state);
void cmd_statuses_console_sets_none(void **state);
void cmd_statuses_chat_sets_all(void **state);
void cmd_statuses_chat_sets_online(void **state);
void cmd_statuses_chat_sets_none(void **state);
void cmd_statuses_muc_sets_on(void **state);
void cmd_statuses_muc_sets_off(void **state);