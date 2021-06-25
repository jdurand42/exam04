#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SIDE_IN 1
#define SIDE_OUT 0

#define END 0
#define PIPE 1
#define BREAK 2

typedef struct s_list
{
	char **args;
	int type;
	int length;
	int pipes[2];
	struct s_list *next;
	struct s_list *prev;
}		t_list;

int ft_strlen(char *s)
{
	int i = 0;
	while (s[i] != 0)
		i++;
	return (i);
}

void print_error(char *s)
{
	write(STDERR, s, ft_strlen(s));
}

int error_fatal()
{
	print_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

char *ft_strdup(char *s)
{
	int i = 0;
	int len = ft_strlen(s);
	char *b;

	if (!(b = (char*)malloc((len + 1) * sizeof(char))))
	{
		error_fatal();
		return (NULL);
	}
	while (i < len)
	{
		b[i] = s[i];
		i++;
	}
	b[i] = 0;
	return (b);
}

void list_rewind(t_list **list)
{
	if (!*list)
		return ;
	while ((*list)->prev)
		*list = (*list)->prev;
}

void lst_clear(t_list **list)
{
	int i = 0;
	t_list *b = NULL;

	if (!*list)
		return ;
	list_rewind(list);
	while (*list)
	{
		b = (*list)->next;
		if ((*list)->length)
		{
			i = 0;
			while (i < (*list)->length)
			{
				free((*list)->args[i]);
				i++;
			}
			free((*list)->args);
		}
		free(*list);
		*list = b;
	}
}

void add_arg(t_list *list, char *arg)
{
	int i = 0;
	char **b = NULL;

	if (!(b = (char**)malloc((list->length + 2) * sizeof(char*))))
		error_fatal();
	while (i < list->length)
	{
		//printf("%s\n", list->args[i]);
		b[i] = list->args[i];
		//printf("%s\n", b[i]);
		//printf("%d\n", ft_strlen(list->args[i]));
		i++;
	}
	b[i] = ft_strdup(arg);
	i++;
	b[i] = 0;
	list->length += 1;
	if (list->args)
		free(list->args);
	list->args = b;
}

void push_list(t_list **list, char *arg)
{
	t_list *b = NULL;
	
	if (!(b = (t_list*)malloc(sizeof(t_list))))
		error_fatal();
	
	b->type = END;
	b->args = NULL;
	b->length = 0;
	b->next = NULL;
	b->prev = NULL;
	
	if (*list)
	{
		b->prev = *list;
		(*list)->next = b;
	}
	*list = b;

	add_arg(*list, arg);
}

void parse_arg(t_list **list, char *arg)
{
	int IS_BREAK = (strcmp(";", arg) == 0);
	
	if (IS_BREAK && !*list)
		return ;
	else if (!IS_BREAK && (!*list || (*list)->type > END)) 
		push_list(list, arg);
	else if (strcmp("|", arg) == 0)
		(*list)->type = PIPE;
	else if (IS_BREAK)
		(*list)->type = BREAK;
	else
		add_arg(*list, arg);
}

void print_list(t_list *list)
{
	int i = 0;
	int j = 0;
	
	while (list)
	{
		printf("%d: length: %d, type: %d\n", i, list->length, list->type);
		if (list->length)
		{
			j = 0;
			while (j < list->length)
			{
				printf("%s\n", list->args[j]);
				j++;
			}
		}
		i++;
		list = list->next;
	}
}

int exec_a_cmd(t_list *list, char **env)
{
	int pipe_open = 0;
	int ret = EXIT_FAILURE;
	int status;
	pid_t pid;
	
	if (list->type == PIPE || (list->prev && list->prev->type == PIPE))
	{
		pipe_open = 1;
		if (pipe(list->pipes) < 0)
			return (error_fatal());
	}
	pid = fork();
	if (pid < 0)
		return (error_fatal());
	if (pid == 0)
	{
		if (list->type == PIPE && (dup2(list->pipes[SIDE_IN], STDOUT) < 0))
			return (error_fatal());
		if (list->prev && list->prev->type == PIPE &&
		(dup2(list->prev->pipes[SIDE_OUT], STDIN) < 0))
			return (error_fatal());
		if ((ret = execve(list->args[0], list->args, env)) < 0)
		{
			print_error("error: cannot execute ");
			print_error(list->args[0]);
			print_error("\n");
		}
		exit(ret);
	}
	else
	{
		waitpid(pid, &status, 0);
		if (pipe_open == 1)
		{
			close(list->pipes[SIDE_IN]);
			if (!list->next || list->type == BREAK)
				close(list->pipes[SIDE_OUT]);
		}
		if (list->prev && list->prev->type == PIPE)
			close(list->prev->pipes[SIDE_OUT]);
		
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);	
	}
	return (ret);
}	

int exec_cmds(t_list **list, char **env)
{
	int ret = EXIT_FAILURE;
	while (*list)
	{
		if (strcmp("cd", (*list)->args[0]) == 0)
		{
			if ((*list)->length < 2)
			{
				print_error("error: cd: bad arguments\n");
				return (EXIT_FAILURE);
			}
			else if (chdir((*list)->args[1]) < 0)
			{
				print_error("error: cd: cannot change directory to ");
				print_error((*list)->args[1]);
				print_error("\n");
				return (EXIT_FAILURE);
			}
		}
		else
		{
			ret = exec_a_cmd(*list, env);
		}
		if ((*list)->next)
			*list = (*list)->next;
		else
			break ;
	}
	return (ret);
}

int main(int ac, char **av, char **env)
{
	int ret = EXIT_SUCCESS;
	int i = 1;
	t_list *cmds = NULL;

	while (i < ac)
	{
		parse_arg(&cmds, av[i]);
		i++;
	}
	list_rewind(&cmds);
//	print_list(cmds);
	if (cmds)
	{
		ret = exec_cmds(&cmds, env);
	}
	
	lst_clear(&cmds);
	return (ret);
}	
