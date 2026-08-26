#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "shell.h"
#include "lexer.h"
#include "parser.h"
#include "env.h"

static void trim_newline(char *line)
{
    size_t  len;

    len = tci_strlen(line);
    if (len > 0 && line[len - 1] == '\n')
        line[len - 1] = '\0';
}

static int is_blank(char const *line)
{
    while (*line)
    {
        if (!tci_isspace((unsigned char)*line))
            return (0);
        line++;
    }
    return (1);
}

static void run_line(t_shell *sh, char const *line)
{
    t_token *tokens;
    t_node  *root;

    if (is_blank(line))
        return;
    tokens = lex(line);
    if (!tokens)
    {
        fprintf(stderr, "c07shell: syntax error: unterminated quote\n");
        sh->last_status = 2;
        return;
    }
    root = parse(tokens);
    if (!root)
    {
        fprintf(stderr, "c07shell: syntax error\n");
        free_tokens(tokens);
        sh->last_status = 2;
        return;
    }
    sh->last_status = exec_node(sh, root);
    free_node(root);
    free_tokens(tokens);
}

/* BUG (fixed here, c07d): checkpoint A had no sigaction call anywhere, so
 * Ctrl+C used the default SIGINT disposition and killed this whole shell
 * process instead of just a running foreground command. SA_RESTART means
 * the interrupted read() inside tci_getline just resumes automatically --
 * the handler's own job is only to print a fresh line, not to make the
 * main loop treat the interruption as EOF. Each command's own child resets
 * SIGINT back to the default disposition before exec (see exec.c) so the
 * program actually running in the foreground still dies normally. */
static void sigint_handler(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
}

static void setup_signals(void)
{
    struct sigaction    sa;

    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGQUIT, SIG_IGN);
}

int main(void)
{
    t_shell sh;
    char    *line;
    int     interactive;

    sh.envp = envp_init();
    sh.last_status = 0;
    sh.running = 1;
    interactive = isatty(STDIN_FILENO);
    setup_signals();
    while (sh.running)
    {
        if (interactive)
            tci_printf("$ ");
        line = tci_getline(STDIN_FILENO);
        if (!line)
        {
            if (interactive)
                tci_printf("exit\n");
            break;
        }
        trim_newline(line);
        run_line(&sh, line);
        free(line);
    }
    envp_free(sh.envp);
    return (sh.last_status);
}
