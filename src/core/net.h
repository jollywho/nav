
int net_init();
int net_max(fd_set *set);
void net_select(fd_set *set);

static void handle_new();
static void handle_socks();
