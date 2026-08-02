/* NOTES:
	cmd:
	q        quit
	p        print node
	b        go back
	h        print history
	l        jump to last watched
	[0-9]o   open directory
	[0-9]r   reproduce video with that number

	Example: 
	reproduce video with index 4
	4r
*/

#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#define MAXNAME_LEN 255      /* path name must be no longer than this */
#define MAX_CHILDS  256      /* nodes max children nodes */
#define MAX_INPUT   4        /* max input buffer */

struct node {
	int type;
	char path[MAXNAME_LEN + 1];
	struct node *parent;
	struct node *children[MAX_CHILDS];
	int n_children;
};

struct command {
	long addr;
	char cmd;
	int err;
};

enum { OK = 0, ERR_BAD_CMD = -1 };

const char *cmd_list = "qpbhlor";



void usage(void);
void walk_dir(struct node *n); 
void join_path(char *dst, char *basename, char *filename, int d_type);
struct node *new_node(int d_type, char *filename, struct node *parent);
void free_tree(struct node *n);

void parse_command(char *input, struct command *cmd);
void set_err(struct command *cmd, int err);

void print_screen(struct node *n);
void print_help(void);
void print_err(struct command *cmd);


int main(int argc, char *argv[]) 
{
	int ch;
	char *dir = NULL;
	char input[MAX_INPUT];

	while ((ch = getopt(argc, argv, "d:")) != -1) {
		switch (ch) {
		case 'd':
			dir = optarg;;
			break;
		case '?':
		default:
			usage();
		}

	}
	argc -= optind;
	argv += optind;

	if (dir == NULL) {
		usage();
		return 1;
	}

	/* check dir is a directory */
	struct stat sb;
	stat(dir, &sb);
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "Erorr: %s is not a directory\n", dir);
		return 1;
	}

	/* remove trailing '/' */
	size_t len = strlen(dir);
	if (dir[len - 1] == '/') 
		dir[len - 1] = '\0';

	struct node *root = new_node(DT_DIR, dir, NULL);
	walk_dir(root);

	struct node *cur_node = root;
	print_screen(cur_node);

	memset(input, '\0', MAX_INPUT);
	struct command cmd = {-1, -1, ERR_BAD_CMD};

	int running = 1;
	while (running == 1) {
		printf("megatron> ");

		memset(input, '\0', MAX_INPUT);
		fgets(input, MAX_INPUT, stdin);
		input[MAX_INPUT - 1] = '\0';

		parse_command(input, &cmd);

		if (cmd.err != 0) {
			print_err(&cmd);
			continue;
		}

		switch(cmd.cmd) {
		case 'p':
			print_screen(cur_node);
			break;
		case 'q':
			running = 0;
			break;
		default:
			break;
		}
	}

	free_tree(root);
	return 0;
}


void usage(void) 
{
	puts("usage: megatron [-d dir]");
	return;
}


void walk_dir(struct node *n) 
{
	DIR* dirp;
	struct dirent *r;

	dirp = opendir(n->path);
	if (dirp == NULL) {
		fprintf(stderr, "Error: %s directory cannot be open\n", n->path);
		return;
	}

	while ((r = readdir(dirp)) != NULL) {
		if ((strcmp(r->d_name, ".") == 0) || (strcmp(r->d_name, "..") == 0)) continue;
		if ((r->d_type != DT_DIR) && (r->d_type != DT_REG)) continue;

		/* create node. add to parent */
		struct node *child = new_node(r->d_type, r->d_name, n);
		n->children[n->n_children++] = child;

		if (child->type == DT_DIR) {
			walk_dir(child);
		} 
	}

	closedir(dirp);
}


void join_path(char *dst, char *basename, char *filename, int d_type)
{
	memset(dst, '\0', MAXNAME_LEN + 1);
	char *end = (d_type == DT_DIR ? "/" : "");
	snprintf(dst, MAXNAME_LEN, "%s%s%s", basename, filename, end); 
	dst[MAXNAME_LEN] = '\0';
}

struct node *new_node(int d_type, char *filename, struct node *parent) 
{
	struct node *n = (struct node *) malloc(sizeof(struct node));

	n->type = d_type;
	
	char *basename = (parent == NULL ? "" : parent->path);
	join_path(n->path, basename, filename, d_type);

	n->parent = parent;

	for (int i=0; i < MAX_CHILDS; i++) { n->children[i] = NULL; }

	n->n_children = (d_type == DT_REG ? -1 : 0);

	return n;
}

void free_tree(struct node *n)
{
	for (int i=0; i < n->n_children; i++) {
		if (n->children[i]->type == DT_DIR) {
			free_tree(n->children[i]);
		}
		free(n->children[i]);
	}

	if (n->parent == NULL) 
		free(n);
}

void print_screen(struct node *n)
{
	printf(" === %s === \n", n->path);
	for (int i=0; i < n->n_children; i++) {
		char *p = strdup(n->children[i]->path);
		printf("%4d  %s\n", i, basename(p));
		free(p);
	}
	printf("\n");
}

void print_help(void)
{
	// TODO! //
	puts("megatron");
	puts("\t your buddy");
}


void parse_command(char *input, struct command *cmd)
{
	/* strip newline, truncate at space */
	char *nl;
	if ((nl = strchr(input, '\n')) != NULL) { *nl = '\0'; } 

	char *p_cmd = NULL;
	long addr = strtol(input, &p_cmd, 10);

	if ((strchr(cmd_list, p_cmd[0])) == NULL) {
		set_err(cmd, ERR_BAD_CMD);
		return;
	} 
		
	cmd->addr = addr;
	cmd->cmd = p_cmd[0];
	cmd->err = OK;
}


void set_err(struct command *cmd, int err)
{
	if (err == ERR_BAD_CMD) {
		cmd->addr = -1;
		cmd->cmd = 'e';
		cmd->err = ERR_BAD_CMD;
	}
}

void print_err(struct command *cmd)
{
	if (cmd->err == ERR_BAD_CMD) {
		puts("bad command");
	}
}
