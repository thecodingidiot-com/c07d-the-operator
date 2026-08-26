#ifndef ENV_H
# define ENV_H

char    **envp_init(void);
char    *envp_get(char **envp, char const *name);
void    envp_set(char ***envp, char const *name, char const *value);
void    envp_unset(char ***envp, char const *name);
void    envp_free(char **envp);

#endif
