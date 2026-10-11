#include "TinyTile.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void spawn_command(const char *command)
{
    if (!command || !*command)
        return;

    if (fork() != 0)
        return;

    setsid();
    signal(SIGCHLD, SIG_IGN);

    execl("/bin/sh", "sh", "-c", command, (char *)NULL);
    _exit(EXIT_FAILURE);
}

void spawn_terminal(const union Arg *arg)
{
    (void)arg;
    spawn_command(config.terminal);
}

void spawn_menu(const union Arg *arg)
{
    (void)arg;
    spawn_command(config.menu);
}

void spawn_browser(const union Arg *arg)
{
    (void)arg;
    spawn_command(config.browser);
}

void quit(const union Arg *arg)
{
    (void)arg;
    wl_display_terminate(server.display);
}