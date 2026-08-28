#include <unistd.h>
#include <stdlib.h>
#include "shell.h"
#include "env.h"

int is_builtin(char const *name)
{
    if (!name)
        return (0);
    return (tci_strcmp(name, "cd") == 0
        || tci_strcmp(name, "pwd") == 0
        || tci_strcmp(name, "exit") == 0
        || tci_strcmp(name, "export") == 0
        || tci_strcmp(name, "unset") == 0
        || tci_strcmp(name, "env") == 0);
}

static int builtin_cd(t_shell *sh, char **argv)
{
    char const *target;

    target = argv[1];
    if (!target)
        target = envp_get(sh->envp, "HOME");
    if (!target) {
        tci_printf("cd: no HOME set\n");
        return (1);
    }
    if (chdir(target) != 0) {
        tci_printf("cd: %s: No such file or directory\n", target);
        return (1);
    }
    return (0);
}

static int builtin_pwd(void)
{
    char    buf[4096];

    if (!getcwd(buf, sizeof(buf))) {
        tci_printf("pwd: error retrieving current directory\n");
        return (1);
    }
    tci_printf("%s\n", buf);
    return (0);
}

/* export NAME=VALUE  or  export NAME (kept/added with an empty value if it
 * doesn't already exist -- this shell has no separate "shell variable"
 * concept, export is the only way to set an environment variable). */
static int builtin_export(t_shell *sh, char **argv)
{
    int     i;
    char    *eq;
    char    name[256];
    size_t  nlen;

    i = 1;
    while (argv[i]) {
        eq = tci_strchr(argv[i], '=');
        if (eq) {
            nlen = (size_t)(eq - argv[i]);
            if (nlen >= sizeof(name))
                nlen = sizeof(name) - 1;
            tci_strlcpy(name, argv[i], nlen + 1);
            envp_set(&sh->envp, name, eq + 1);
        }
        else if (!envp_get(sh->envp, argv[i]))
            envp_set(&sh->envp, argv[i], "");
        i++;
    }
    return (0);
}

static int builtin_unset(t_shell *sh, char **argv)
{
    int i;

    i = 1;
    while (argv[i]) {
        envp_unset(&sh->envp, argv[i]);
        i++;
    }
    return (0);
}

static int builtin_env(t_shell *sh)
{
    int i;

    i = 0;
    while (sh->envp[i]) {
        tci_printf("%s\n", sh->envp[i]);
        i++;
    }
    return (0);
}

int run_builtin(t_shell *sh, char **argv)
{
    if (tci_strcmp(argv[0], "cd") == 0)
        return (builtin_cd(sh, argv));
    if (tci_strcmp(argv[0], "pwd") == 0)
        return (builtin_pwd());
    if (tci_strcmp(argv[0], "export") == 0)
        return (builtin_export(sh, argv));
    if (tci_strcmp(argv[0], "unset") == 0)
        return (builtin_unset(sh, argv));
    if (tci_strcmp(argv[0], "env") == 0)
        return (builtin_env(sh));
    sh->running = 0;
    if (argv[1])
        return (tci_atoi(argv[1]));
    return (sh->last_status);
}
