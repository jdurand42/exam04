#include <stdlib.h>
#include <stdio.h>
#include <libc.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// SIDE IN == 1 && SIDE OU == 2
#define SIDE_IN 1
#define SIDE_OUT 0

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define END 0
#define PIPE 1
#define BREAK 2

typedef struct s_list 
{
	char **args;
	int type;
	int length;
	int pipes[2];
	struct s_list *prev;
	struct s_list *next;
} 		t_list;

int ft_strlen(char *s)
{
	int i = 0;
	while (s[i])
		i++;
	return (i);
}

int print_error(char *s)
{
	if (s)
		write(STDERR, s, ft_strlen(s));
	return (EXIT_FAILURE);
}

int list_rewind(t_list **list)
{
	while (*list && (*list)->prev)
		*list = (*list)->prev;
	return (EXIT_SUCCESS);
}

int error_fatal()
{
	print_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

void *error_fatal_ptr()
{
	error_fatal();
	exit(EXIT_FAILURE);
	return (NULL);
}

char *ft_strdup(char *s)
{
	int len = ft_strlen(s);
	int i = 0;
	char *b;

	if (!(b = (char*)malloc((len + 1) * sizeof(char))))
		return (error_fatal_ptr());
	while (i < len)
	{
		b[i] = s[i];
		i++;
	}
	b[i] = 0;
	return (b);
}

void print_env(char **env)
{
	int i = 0;
	while (env[i] != 0)
		printf("%s\n", env[i++]);
}


int ft_lst_clear(t_list **list)
{
	t_list *b;
	int i = 0;
	list_rewind(list);
	
	while (*list)
	{
		i = 0;
		b = (*list)->next;
		while (i < (*list)->length)
		{
			free((*list)->args[i]);
			i++;
		}
		free((*list)->args);
		free(*list);
		*list = b;
	}
	return (EXIT_SUCCESS);
}
		
void print_list(t_list *list)
{
	int i = 0;
	int j = 0;
	while (list)
	{
		j = 0;
		printf("%d: length: %d, type: %d, pipes1: %d, pipes2: %d\n", i, list->length, list->type, list->pipes[0], list->pipes[1]);
		i++;	
		if (list->length)
		{
			while (j < list->length)
			{
				printf("%s\n", list->args[j]);
				j++;
			}
		}
		list = list->next;
	}
} 

int add_arg(t_list *list, char *arg)
{
	char **b;
	int i = 0;

	if (!(b = (char**)malloc((list->length + 2) * sizeof(char*))))
		return (error_fatal());
	while (i < list->length)
	{
		b[i] = list->args[i];
		i++;
	}
	if (list->length)
		free(list->args);
	list->args = b;
	b[i] = ft_strdup(arg);
	b[i + 1] = 0;
	list->length += 1;
	return (EXIT_SUCCESS);
}

int push_list(t_list **list, char *arg)
{	
	t_list *b;

	if (!(b = (t_list*)malloc(sizeof(t_list))))
		return (error_fatal());
	b->type = END;
	b->length = 0;
	b->next = NULL;
	b->prev = NULL;
	b->args = NULL;
		
	if (*list)
	{
		(*list)->next = b;
		b->prev = *list;
	}
	*list = b;
	return (add_arg(*list, arg));
}

int parse_arg(t_list **list, char *arg)
{
	int is_break = (strcmp(";", arg) == 0);
	
	if (is_break && !*list)
		return (EXIT_SUCCESS); // if cmds empty and char is ;
	else if (!is_break && (!*list || (*list)->type > END)) // if it's another command (end will be superior than END
		return (push_list(list, arg));
	else if (strcmp("|", arg) == 0) // if is a pipe
		(*list)->type = PIPE;
	else if (is_break) // if is ; and list not empty
		(*list)->type = BREAK;
	else // if it's a string other then pipe or end
	{
		return (add_arg(*list, arg));
	}
	return (EXIT_SUCCESS);
}

int exec_a_cmd(t_list *cmd, char **env)
{
	int ret = EXIT_FAILURE;
	int status;
	int pipe_open = 0;
	int i = 0;
	pid_t pid;

	// if pipe: open pipe
	// else fork

	if (cmd->type == PIPE || (cmd->prev && cmd->prev->type == PIPE))
	{	
		// if this cmd is a pipe or last command is
		// we pipe open
		pipe_open = 1;
		// we create the pipe
		if (pipe(cmd->pipes) < 0)
			return (error_fatal());
	}

	pid = fork();
	//pid = 0;
	if (pid < 0) // then error on forking
		return (error_fatal());
	else if (pid == 0)
	{
		// weuse dup2 to pipe stdout to sideIn (pipe[0])
		// if this command is a pipe
		if (cmd->type == PIPE)
		{
			if (dup2(cmd->pipes[SIDE_IN], STDOUT) < 0)
				return (error_fatal());
		}
		// if prev command was a pipe, we use dup to
		// pipe stdin to SIDE_OUT (pipe[1])
		if (cmd->prev && cmd->prev->type == PIPE)
		{
			if (dup2(cmd->prev->pipes[SIDE_OUT], STDIN) < 0)
				return (error_fatal());
		}	
		//print_env(env);
		if ((ret = execve(cmd->args[0], cmd->args, env)) < 0)
		{
			//printf("errno: %d\n", errno);
			print_error("error: cannot execute ");
			print_error(cmd->args[0]);
			print_error("\n");
		}
		exit(ret);
	}
	else
	{
		waitpid(pid, &status, 0);
		if (pipe_open)
		{
			// if pipe open, we close side_in of current commande
			close(cmd->pipes[SIDE_IN]);
			if (!cmd->next || cmd->type == BREAK) // if last or next cmd is a new one
			{
				// we close SIDE_OUT
				close(cmd->pipes[SIDE_OUT]);
			}
		}
		// even if not pipe open
		if (cmd->prev && cmd->prev->type == PIPE)
		{
			// if prev cmd was a pipe
			// we close side out of prev command
			close(cmd->prev->pipes[SIDE_OUT]);
		}
		if (WIFEXITED(status)) // pour recup le code d'erreur de la commande
			ret = WEXITSTATUS(status);
	}
	return (ret);
}

int exec_cmds(t_list **cmds, char **env)
{
	int ret = EXIT_FAILURE;
	t_list *b = NULL;
	while (*cmds)
	{
		b = *cmds; // why cur? probably useless
		// implementer cd
		// executer commandes
		if (strcmp("cd", b->args[0]) == 0)
		{
			if (b->length < 2)
				return (print_error("error: cd: bad argument\n"));
			else if (chdir(b->args[1])) // return 0 if success
			{
				print_error("error: cd: cannot change path to ");
				print_error(b->args[1]);
				print_error("\n");
				return (EXIT_FAILURE);
			}
		}
		else
			ret = exec_a_cmd(b, env);
		if ((*cmds)->next)
			*cmds = (*cmds)->next;
		else
			break; // need to keep last element for free
	}		
	return (ret); 
}


int main(int ac, char **av, char **env)
{
	int ret = EXIT_SUCCESS;
	t_list *cmds = NULL;
	int i = 1;

	while (i < ac)
	{
		parse_arg(&cmds, av[i]);
		i++;
	}
	// print_env(env);
	if (cmds)
	{
		list_rewind(&cmds);
		//print_list(cmds);
		//printf("----------\n");
		ret = exec_cmds(&cmds, env);
	}
	
	ft_lst_clear(&cmds);
	return (ret);
}
