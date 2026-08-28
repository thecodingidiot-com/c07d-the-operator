#include <stdlib.h>
#include <stdio.h>
#include <glob.h>
#include "libtci.h"
#include "expand.h"
#include "env.h"

typedef struct s_buf
{
    char    *data;
    size_t  len;
    size_t  cap;
}   t_buf;

static void buf_init(t_buf *b)
{
    b->cap = 32;
    b->len = 0;
    b->data = tci_calloc(b->cap, 1);
}

static void buf_push(t_buf *b, char c)
{
    char    *bigger;

    if (b->len + 2 >= b->cap) {
        bigger = tci_calloc(b->cap * 2, 1);
        tci_memcpy(bigger, b->data, b->len);
        free(b->data);
        b->data = bigger;
        b->cap *= 2;
    }
    b->data[b->len] = c;
    b->len++;
    b->data[b->len] = '\0';
}

static void buf_push_str(t_buf *b, char const *s)
{
    while (s && *s) {
        buf_push(b, *s);
        s++;
    }
}

static int is_ident_start(char c)
{
    return (tci_isalpha((unsigned char)c) || c == '_');
}

static int is_ident_char(char c)
{
    return (tci_isalnum((unsigned char)c) || c == '_');
}

/* $VAR / $? substitution -- everything else in `word` is copied verbatim. */
static char *expand_word(t_shell *sh, char const *word)
{
    t_buf   buf;
    int     i;
    int     start;
    char    name[256];
    int     nlen;
    char    status_str[16];

    buf_init(&buf);
    i = 0;
    while (word[i]) {
        if (word[i] == '$' && word[i + 1] == '?') {
            snprintf(status_str, sizeof(status_str), "%d", sh->last_status);
            buf_push_str(&buf, status_str);
            i += 2;
        }
        else if (word[i] == '$' && is_ident_start(word[i + 1])) {
            i++;
            start = i;
            while (is_ident_char(word[i]))
                i++;
            nlen = i - start;
            if (nlen >= (int)sizeof(name))
                nlen = sizeof(name) - 1;
            tci_strlcpy(name, word + start, nlen + 1);
            buf_push_str(&buf, envp_get(sh->envp, name));
        }
        else {
            buf_push(&buf, word[i]);
            i++;
        }
    }
    return (buf.data);
}

char **expand_argv(t_shell *sh, char **argv)
{
    int     n;
    int     i;
    char    **out;

    n = 0;
    while (argv[n])
        n++;
    out = tci_calloc(n + 1, sizeof(char *));
    i = 0;
    while (i < n) {
        out[i] = expand_word(sh, argv[i]);
        i++;
    }
    return (out);
}

typedef struct s_wbuf
{
    char    **data;
    size_t  len;
    size_t  cap;
}   t_wbuf;

static void wbuf_init(t_wbuf *b)
{
    b->cap = 8;
    b->len = 0;
    b->data = tci_calloc(b->cap, sizeof(char *));
}

static void wbuf_push(t_wbuf *b, char *s)
{
    char    **bigger;
    size_t  i;

    if (b->len + 2 >= b->cap) {
        bigger = tci_calloc(b->cap * 2, sizeof(char *));
        i = 0;
        while (i < b->len) {
            bigger[i] = b->data[i];
            i++;
        }
        free(b->data);
        b->data = bigger;
        b->cap *= 2;
    }
    b->data[b->len] = s;
    b->len++;
    b->data[b->len] = NULL;
}

static int has_glob_char(char const *s)
{
    while (*s) {
        if (*s == '*' || *s == '?' || *s == '[')
            return (1);
        s++;
    }
    return (0);
}

/* No match keeps the pattern literal (bash's default, non-nullglob
 * behaviour) rather than dropping the argument. */
char **expand_wildcards(char **argv)
{
    t_wbuf  out;
    glob_t  g;
    int     i;
    size_t  j;

    wbuf_init(&out);
    i = 0;
    while (argv[i]) {
        if (has_glob_char(argv[i]) && glob(argv[i], 0, NULL, &g) == 0
            && g.gl_pathc > 0) {
            j = 0;
            while (j < g.gl_pathc) {
                wbuf_push(&out, tci_strdup(g.gl_pathv[j]));
                j++;
            }
            globfree(&g);
        }
        else
            wbuf_push(&out, tci_strdup(argv[i]));
        i++;
    }
    return (out.data);
}

void free_expanded(char **argv)
{
    int i;

    i = 0;
    while (argv[i]) {
        free(argv[i]);
        i++;
    }
    free(argv);
}
