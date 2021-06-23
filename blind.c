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

typedef struct	s_list
{
	char **args;
	int type;
	int pipes[2];
	int length;
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

int print_error(char *s)
{
	write(STDERR, s, ft_strlen(s));
	return (EXIT_FAILURE);
}

int error_fatal()
{
	print_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

char *ft_strdup(char *s)
{
	int len = ft_strlen(s);
	int i = 0;
	char *b;

	if (!(b = (char*)malloc((ft_strlen(s) + 1) * sizeof(char))))
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

void lst_rewind(t_list **list)
{
	if (!*list)
		return;
	while ((*list)->prev)
		*list = (*list)->prev;
}

void lst_clear(t_list **list)
{
	t_list *b;
	lst_rewind(list);
	int i = 0;

	if (!*list)
		return ;
	while (*list)
	{
		b = (*list)->next;
		i = 0;
		if ((*list)->length > 0)
		{
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

void add_arg(t_list *list, char * arg)
{
	int i = 0;
	char **b;

	if (!(b = (char**)malloc((list->length + 2) * sizeof(char*))))
		error_fatal();
	while (i < list->length)
	{
		b[i] = list->args[i];
		i++;
	}
	b[i++] = ft_strdup(arg);
	b[i] = 0;
	free(list->args);
	list->args = b;
	list->length += 1;
}

void push_list(t_list **list, char *arg)
{
	t_list *b;

	if (!(b = (t_list*)malloc(sizeof(t_list))))
		error_fatal();

	b->args = NULL;
	b->length = 0;
	b->type = END;
	b->next = NULL;
	b->prev = NULL;

	if (!*list)
		*list = b;
	else
	{
		(*list)->next = b;
		b->prev = *list;	
	}
	*list = b;
	add_arg(*list, arg);
} 

void parse_arg(t_list **cmds, char *arg)
{
	int is_break = (strcmp(";", arg) == 0);
	// cas ou premier arg est ; -> on ignore
	if (is_break && !*cmds)
		return ;
	// cas ou arg n'est pas ; et (c'est la premiere commande ou la commande précédente est fini (END et non PIPE ou BREAK)
	// on crée un nouveau element dans la liste
	// permet d'initialiser la liste, le premier argument sera ajouter a list->args dans add_arg dans push_list
	// marche car le sujet ne testera pas un "|" précdé ou suivi par rien
	else if (!is_break && (!*cmds || (*cmds)->type > END))
		push_list(cmds, arg);
	// cas ou arg est PIPE
	// on met le type de l'élément de la liste a PIPE
	else if (strcmp("|", arg) == 0)
		(*cmds)->type = PIPE;
	// cas ou arg est BREAK
	// on met le type de l'élément de la liste a BREAK
	else if (is_break)
		(*cmds)->type = BREAK;
	// Si l'arg n'est ni BREAK, PIPE et que la liste n'est pas vide
	else
		add_arg(*cmds, arg);
}

void print_list(t_list *list)
{
	int i = 0;
	int j = 0;
	while (list)
	{
		j = 0;
		printf("%d: length: %d, type: %d\n", i, list->length, list->type);
		while (j < list->length)
		{
			printf("%s ", list->args[j]);
			j++;
		}
		if (j)
			printf("\n");
		list = list->next;
	}
}

int exec_a_cmd(t_list *list, char **env)
{
	int ret = EXIT_FAILURE;
	int status;
	int pipe_open = 0;
	pid_t pid;

	if (list->type == PIPE || (list->prev && list->prev->type == PIPE))
	{
		//marker for knowing if pipe was created to close
		pipe_open = 1;
		// create pipe
		if (pipe(list->pipes) < 0)
			return (error_fatal());
	}
	pid = fork();
	if (pid < 0)
		return (error_fatal());
	if (pid == 0)
	{
		// pipe stuff
		// maping pipes
		// si pipes sur cette commande, on map le side_in de son pipe avec STDOUT -> IN OUT
		if (list->type == PIPE && dup2(list->pipes[SIDE_IN], STDOUT) < 0)
			return (error_fatal());
		// si pipes sur la précédente commande, on map le SIDE_OUT de la PRECEDENTE commande avec STDIN -> OUT IN
		if (list->prev && list->prev->type == PIPE && dup2(list->prev->pipes[SIDE_OUT], STDIN) < 0)
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
		// si pipe_open
		// on ferme ke SIDE_IN de la commande en cour
		if (pipe_open)
		{
			close(list->pipes[SIDE_IN]);
			// si il n'y a pas de prochaine commande ou que cette commance est un BREAK (;)
			// on ferme le SIDE_OUT de CETTE commande
			if (!list->next || list->type == BREAK)
				close(list->pipes[SIDE_OUT]);

		}
		// dans tout les cas
		// on ferme le side out de la commande PRECEDENTE si pipe sur la commande PRECEDENTE
		if (list->prev && list->prev->type == PIPE)
		{
			close(list->prev->pipes[SIDE_OUT]);
		}
		// to get exit code of the fork in parent
		// no choice but to learn it
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
		// on implement cd
		// pas de pieges
		if ((*list)->length && (strcmp("cd", (*list)->args[0]) == 0))
		{
			if ((*list)->length < 2)
			{
				print_error("error: cd: bad argument\n");
				return (EXIT_FAILURE);
			}
			else if (chdir((*list)->args[1]) < 0)
			{
				print_error("error: cd: cannot change path to ");
				print_error((*list)->args[1]);
				print_error("\n");
				return (EXIT_FAILURE);
			}
					
		}
		else
		{
			// si pas cd on execute la commande
			ret = exec_a_cmd(*list, env);
		}
		// pour garder le dernier element de la liste pour pouvoir free
		if (!(*list)->next)
			break;
		*list = (*list)->next;
	}
	return (ret);
}

int main(int ac, char **av, char **env)
{
	int ret = EXIT_SUCCESS;
	int i = 1;

	t_list *cmds = NULL;

	if (ac < 2)
		return (ret);
	while (i < ac)
	{
		parse_arg(&cmds, av[i]);
		i++;
	}
	lst_rewind(&cmds);
	if (cmds)
		ret = exec_cmds(&cmds, env);
	lst_clear(&cmds);
	return (ret);
	//while (1);
}
	
