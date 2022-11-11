#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define SIDE_OUT 0
#define SIDE_IN 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define TYPE_END 3
#define TYPE_PIPE 4
#define TYPE_BREAK 5

typedef struct s_list
{
    char            **args;
    int             lenght;
    int             type;
    int             pipe[2];
    struct s_list   *prev;
    struct s_list   *next;
} t_list;

void    free_all(t_list *cmds)
{
    t_list	*tmp;

	while (cmds)
	{
		tmp = cmds->prev;
		free(cmds->args);
		free(cmds);
		cmds = tmp;
	}
}

int exit_handler(char *s1, char *s2)
{
    write(STDERR, s1, strlen(s1));
    write(STDERR, s2, strlen(s2));
    write(STDERR, "\n", 1);
    return EXIT_FAILURE;
}

void exit_fatal()
{
    exit_handler("error: ", "fatal");
    exit(EXIT_FAILURE);
}

void    add_arg(t_list *cmd, char *arg)
{
    int     i;
    char    **tmp;

    i = -1;
    if (!(tmp = (char **)malloc(sizeof(char *)*(cmd->lenght+2))))
        exit_fatal();
    while (++i < cmd->lenght)
        tmp[i] = cmd->args[i];
    if (cmd->lenght > 0)
        free(cmd->args);
    cmd->args = tmp;
    cmd->args[i++] = arg;
    cmd->args[i] = 0;
    cmd->lenght++;
}

void    add_cmd(t_list **cmds, char *arg)
{
    t_list *new;

    if (!(new = (t_list *)malloc(sizeof(t_list))))
        exit_fatal();
    new->args = 0;
    new->lenght = 0;
    new->next = 0;
    new->prev = 0;
    new->type = TYPE_END;
    if (*cmds)
    {
        (*cmds)->next = new;
        new->prev = (*cmds); 
    }
    (*cmds) = new;
    add_arg(*cmds, arg);
}

void    parse(t_list **cmds, char *arg)
{
    int is_break;

    is_break = (strcmp(";", arg) == 0);
    if (is_break && !(*cmds))
        return ;
    else if (!is_break && (!(*cmds) || (*cmds)->type > TYPE_END))
        add_cmd(cmds, arg);
    else if (strcmp("|", arg) == 0)
        (*cmds)->type = TYPE_PIPE;
    else if (is_break)
        (*cmds)->type = TYPE_BREAK;
    else
        add_arg(*cmds, arg);
}

int    excuter(t_list *cmd, char **env)
{
    printf("executer\n");
    int     ret;
    pid_t   pid;
    int     status;
    int     pipe_open;

    ret = EXIT_SUCCESS;
    pipe_open = 0;
    if (cmd->type == TYPE_PIPE || (cmd->prev && cmd->prev->type == TYPE_PIPE)) 
    {
        pipe_open = 1;
        if (pipe(cmd->pipe))
            exit_fatal();
    }
    pid = fork();
    if (pid < 0)
        exit_fatal();
    else if (pid == 0)
    {
        if (cmd->type == TYPE_PIPE && (dup2(cmd->pipe[SIDE_IN], STDOUT) < 0))
            exit_fatal();
        if (cmd->prev && cmd->prev->type == TYPE_PIPE && (dup2(cmd->prev->pipe[SIDE_OUT], STDIN) < 0))
            exit_fatal();
        if ((ret=execve(cmd->args[0], cmd->args, env) < 0))
            exit_handler("error: cannot execute ", cmd->args[0]);
        exit(ret);
        
    }
    else
    {
        waitpid(pid, &status, 0);
        if (pipe_open)
        {
            close(cmd->pipe[SIDE_IN]);
            if (!cmd->next || cmd->type == TYPE_BREAK)
                close(cmd->pipe[SIDE_OUT]);
        }
        if (cmd->prev && cmd->prev->type == TYPE_PIPE)
            close(cmd->prev->pipe[SIDE_OUT]);
        if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
    }
    return ret;
}

int    controller(t_list *cmds, char **env)
{
    int ret;

    while (cmds->prev)
        cmds = cmds->prev;
    ret = EXIT_SUCCESS;
    while (cmds)
    {
        if (strcmp(cmds->args[0], "cd") == 0)
        {
            if (cmds->lenght < 2)
                ret = exit_handler("error: cd: ", "bad arguments");
            else if ((ret=chdir(cmds->args[1])))
                ret = exit_handler("error: cd: cannot change directory to ", cmds->args[1]);
        }
        else
            ret = excuter(cmds, env);
        if (ret == EXIT_FAILURE)
            break;
        cmds = cmds->next;
    }
    return ret;
}

int main(int argc, char **argv, char **env)
{
    int     ret;
    int     i;
    t_list  *cmds;

    ret = EXIT_SUCCESS;
    i = 1;
    while (i < argc)
        parse(&cmds, argv[i++]);
    if (cmds)
        ret = controller(cmds, env);
    free_all(cmds);
    return ret;
}
