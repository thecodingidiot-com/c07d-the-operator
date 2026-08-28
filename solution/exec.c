#define _POSIX_C_SOURCE 200809L
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include "shell.h"
#include "expand.h"

extern char **environ;

/* --- checkpoint C (c07c): redirections + pipes ---------------------------- */

# define MAX_REDIRS 32

typedef struct s_resolved
{
    int target_fd;   /* STDIN_FILENO or STDOUT_FILENO */
    int source_fd;   /* the already-open fd to dup2 onto target_fd */
}   t_resolved;

static int heredoc_line_matches(char const *line, char const *delim)
{
    size_t  len;
    char    trimmed[4096];

    len = tci_strlen(line);
    if (len >= sizeof(trimmed))
        return (0);
    tci_strlcpy(trimmed, line, sizeof(trimmed));
    if (len > 0 && trimmed[len - 1] == '\n')
        trimmed[len - 1] = '\0';
    return (tci_strcmp(trimmed, delim) == 0);
}

/* Reads from the shell's OWN stdin until a line exactly matches `delim`,
 * collecting the text into a fresh pipe. This MUST run before any fork()
 * for this command: tci_getline() keeps a per-fd read-ahead buffer, and
 * that buffer is duplicated (not shared) across a fork -- collecting the
 * heredoc body in a child left the parent's own copy of the buffer still
 * holding the same bytes, so the main loop re-read and re-executed the
 * heredoc body as separate commands the first time this was tried. */
static int resolve_heredoc(char const *delim)
{
    int     fd[2];
    char    *line;

    if (pipe(fd) < 0)
        return (-1);
    while (1) {
        line = tci_getline(STDIN_FILENO);
        if (!line || heredoc_line_matches(line, delim)) {
            free(line);
            break;
        }
        write(fd[1], line, tci_strlen(line));
        free(line);
    }
    close(fd[1]);
    return (fd[0]);
}

static int target_fd(t_tok_type type)
{
    if (type == TOK_REDIR_IN || type == TOK_HEREDOC)
        return (STDIN_FILENO);
    return (STDOUT_FILENO);
}

/* All the actual I/O (opening files, reading heredoc bodies) happens here,
 * in whichever process calls this -- always before any fork() for this
 * command, so a forked child only ever needs to dup2() already-open fds
 * it inherited, never perform blocking reads of its own. */
static int resolve_redirs(t_redir *redirs, t_resolved *out, int *count)
{
    int fd;

    *count = 0;
    while (redirs) {
        if (redirs->type == TOK_HEREDOC)
            fd = resolve_heredoc(redirs->target);
        else if (redirs->type == TOK_REDIR_IN)
            fd = open(redirs->target, O_RDONLY);
        else if (redirs->type == TOK_REDIR_APPEND)
            fd = open(redirs->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            fd = open(redirs->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "%s: cannot open\n", redirs->target);
            return (-1);
        }
        out[*count].target_fd = target_fd(redirs->type);
        out[*count].source_fd = fd;
        (*count)++;
        redirs = redirs->next;
    }
    return (0);
}

static void apply_resolved(t_resolved *resolved, int count)
{
    int i;

    i = 0;
    while (i < count) {
        dup2(resolved[i].source_fd, resolved[i].target_fd);
        i++;
    }
}

static void close_resolved(t_resolved *resolved, int count)
{
    int i;

    i = 0;
    while (i < count) {
        close(resolved[i].source_fd);
        i++;
    }
}

/* External command: redirects are resolved in the PARENT (see above), then
 * the already-open fds are inherited by fork() and applied with plain
 * dup2() calls in the child -- no I/O happens after the fork.
 * `environ = sh->envp` in the child (never in the parent -- a fork's
 * writes to its own `environ` never reach the parent) is what makes
 * `export`/`unset` visible to the programs this shell runs: execvp()
 * PATH-searches and builds the new program's environment from `environ`,
 * not from any argument we could pass it directly. */
static int exec_external(t_shell *sh, char **argv, t_redir *redirs)
{
    t_resolved  resolved[MAX_REDIRS];
    int         count;
    pid_t       pid;
    int         status;

    if (resolve_redirs(redirs, resolved, &count) < 0)
        return (1);
    pid = fork();
    if (pid < 0) {
        close_resolved(resolved, count);
        return (1);
    }
    if (pid == 0) {
        apply_resolved(resolved, count);
        environ = sh->envp;
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(127);
    }
    close_resolved(resolved, count);
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return (WEXITSTATUS(status));
    if (WIFSIGNALED(status))
        return (128 + WTERMSIG(status));
    return (1);
}

/* A builtin never forks, so its redirections would otherwise mutate the
 * shell's own stdin/stdout permanently -- save both, apply the resolved
 * redirect, run the builtin, then restore. */
static int exec_builtin_cmd(t_shell *sh, t_redir *redirs, char **argv)
{
    t_resolved  resolved[MAX_REDIRS];
    int         count;
    int         saved_in;
    int         saved_out;
    int         status;

    if (resolve_redirs(redirs, resolved, &count) < 0)
        return (1);
    saved_in = dup(STDIN_FILENO);
    saved_out = dup(STDOUT_FILENO);
    apply_resolved(resolved, count);
    close_resolved(resolved, count);
    status = run_builtin(sh, argv);
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_in);
    close(saved_out);
    return (status);
}

/* Checkpoint D (c07d): variable expansion ($VAR, $?) then wildcard
 * expansion (glob.h) run once per command, in that order -- matching a
 * real shell's expansion order -- before dispatching to a builtin or an
 * external program. */
static int exec_cmd(t_shell *sh, t_node *node)
{
    char    **expanded;
    char    **globbed;
    int     status;

    expanded = expand_argv(sh, node->argv);
    globbed = expand_wildcards(expanded);
    free_expanded(expanded);
    if (is_builtin(globbed[0]))
        status = exec_builtin_cmd(sh, node->redirs, globbed);
    else
        status = exec_external(sh, globbed, node->redirs);
    free_expanded(globbed);
    return (status);
}

/* left | right, N-stage pipelines compose because `left`/`right` may
 * themselves be NODE_PIPE -- each side is executed by a full recursive
 * exec_node() call inside its own forked child. */
static int exec_pipe(t_shell *sh, t_node *node)
{
    int     fd[2];
    pid_t   left_pid;
    pid_t   right_pid;
    int     status;

    if (pipe(fd) < 0)
        return (1);
    left_pid = fork();
    if (left_pid == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        _exit(exec_node(sh, node->left));
    }
    right_pid = fork();
    if (right_pid == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        _exit(exec_node(sh, node->right));
    }
    close(fd[0]);
    close(fd[1]);
    waitpid(left_pid, NULL, 0);
    waitpid(right_pid, &status, 0);
    if (WIFEXITED(status))
        return (WEXITSTATUS(status));
    if (WIFSIGNALED(status))
        return (128 + WTERMSIG(status));
    return (1);
}

/* left && right: right only runs if left succeeded (status 0).
 * left || right: right only runs if left failed (status != 0). */
static int exec_and_or(t_shell *sh, t_node *node)
{
    int status;

    status = exec_node(sh, node->left);
    if (node->type == NODE_AND && status != 0)
        return (status);
    if (node->type == NODE_OR && status == 0)
        return (status);
    return (exec_node(sh, node->right));
}

/* ( inner ): runs the inner tree in a child process and exits with its
 * status -- a subshell's own builtins (cd, export, ...) never affect the
 * parent shell, same as a piped builtin already didn't. */
static int exec_subshell(t_shell *sh, t_node *node)
{
    pid_t   pid;
    int     status;

    pid = fork();
    if (pid < 0)
        return (1);
    if (pid == 0)
        _exit(exec_node(sh, node->left));
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return (WEXITSTATUS(status));
    if (WIFSIGNALED(status))
        return (128 + WTERMSIG(status));
    return (1);
}

/* Checkpoint D (c07d): every node type is real. */
int exec_node(t_shell *sh, t_node *node)
{
    if (node->type == NODE_CMD)
        return (exec_cmd(sh, node));
    if (node->type == NODE_PIPE)
        return (exec_pipe(sh, node));
    if (node->type == NODE_AND || node->type == NODE_OR)
        return (exec_and_or(sh, node));
    return (exec_subshell(sh, node));
}
