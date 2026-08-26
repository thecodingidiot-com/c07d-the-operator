#ifndef EXPAND_H
# define EXPAND_H

# include "shell.h"

char    **expand_argv(t_shell *sh, char **argv);
char    **expand_wildcards(char **argv);
void    free_expanded(char **argv);

#endif
