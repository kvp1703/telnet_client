typedef void (* app_cli_cb_t ) (const char * command);
void telnet_client_init(app_cli_cb_t app_cli_cb);
