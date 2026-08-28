#include <stdlib.h>
#include "libtci.h"
#include "env.h"

extern char **environ;

static int envp_count(char **envp)
{
    int i;

    i = 0;
    while (envp && envp[i])
        i++;
    return (i);
}

/* Builds an owned copy of the process's real environment -- from this
 * point on the shell tracks its OWN environment (export/unset only ever
 * touch this copy), never the process's original `environ` directly. */
char **envp_init(void)
{
    int     count;
    char    **copy;
    int     i;

    count = envp_count(environ);
    copy = tci_calloc(count + 1, sizeof(char *));
    i = 0;
    while (i < count) {
        copy[i] = tci_strdup(environ[i]);
        i++;
    }
    return (copy);
}

static int name_matches(char const *entry, char const *name)
{
    size_t  len;

    len = tci_strlen(name);
    return (tci_strncmp(entry, name, len) == 0 && entry[len] == '=');
}

char *envp_get(char **envp, char const *name)
{
    int i;

    i = 0;
    while (envp && envp[i]) {
        if (name_matches(envp[i], name))
            return (envp[i] + tci_strlen(name) + 1);
        i++;
    }
    return (NULL);
}

/* NAME=VALUE, replacing any existing NAME or appending a new entry.
 * `*envp` is replaced wholesale on append (the array itself is fixed-size
 * once allocated) -- callers always use the returned/updated *envp, never
 * a stale copy of the old pointer. */
void envp_set(char ***envp, char const *name, char const *value)
{
    int     i;
    int     count;
    size_t  size;
    char    **bigger;
    char    *entry;

    size = tci_strlen(name) + tci_strlen(value) + 2;
    entry = tci_calloc(size, 1);
    tci_strcpy(entry, name);
    tci_strlcat(entry, "=", size);
    tci_strlcat(entry, value, size);
    i = 0;
    while ((*envp)[i]) {
        if (name_matches((*envp)[i], name)) {
            free((*envp)[i]);
            (*envp)[i] = entry;
            return;
        }
        i++;
    }
    count = envp_count(*envp);
    bigger = tci_calloc(count + 2, sizeof(char *));
    i = 0;
    while (i < count) {
        bigger[i] = (*envp)[i];
        i++;
    }
    bigger[count] = entry;
    bigger[count + 1] = NULL;
    free(*envp);
    *envp = bigger;
}

void envp_unset(char ***envp, char const *name)
{
    int i;
    int j;

    i = 0;
    while ((*envp)[i]) {
        if (name_matches((*envp)[i], name)) {
            free((*envp)[i]);
            j = i;
            while ((*envp)[j]) {
                (*envp)[j] = (*envp)[j + 1];
                j++;
            }
            return;
        }
        i++;
    }
}

void envp_free(char **envp)
{
    int i;

    i = 0;
    while (envp && envp[i]) {
        free(envp[i]);
        i++;
    }
    free(envp);
}
