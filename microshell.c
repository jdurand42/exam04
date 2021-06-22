#include <stdlib.h>
#include <stdio.h>
#include <libc.h>
#include <unistd.h>
#include <string.h>

#define SIDE_IN 0
#define SIDE_OUT 1

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
	while (*list && (*list)->previous)
		*list = (*list)->previous;
	return (EXIT_SUCCES);
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

int ft_lst_clear(t_list **list)
{
	t_list *b;
	int i = 0;
	list_rewind(list);
	
	while (*list)
	{
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
	b->length += 1;
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
		return (push_list(&list, arg));
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
	print_list(cmds);
	ft_lst_clear(&cmds);
	return (ret);
}
