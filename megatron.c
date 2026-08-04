#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAXNAME_LEN 255      /* path name must be no longer than this */
#define MAX_CHILDS  256      /* nodes max children nodes */

struct node {
	int type;
	char path[MAXNAME_LEN + 1];
	struct node *parent;
	struct node *children[MAX_CHILDS];
	int n_children;
};

void 	usage(void);
int 	walk_dir(struct node *n); 
int 	join_path(char *dst, char *basename, char *filename, int d_type);
struct node * 	new_node(int d_type, char *filename, struct node *parent);
void 	free_tree(struct node *n);

static int 	cmpnodes(const void *a, const void *b);
void 	 sort_childrens(struct node *n);

void 	print_node(struct node *n);

void 	disable_raw_mode(void);
void 	enable_raw_mode(void);
void 	init_tui(struct node *n);

struct termios orig_termios;


int main(int argc, char *argv[]) 
{
	errno = 0;
	int ch;
	char *dir = NULL;

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
	if (stat(dir, &sb) == -1) {
		perror("stat");
		return -1;
	}
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "Error: %s is not a directory\n", dir);
		return 1;
	}

	/* remove trailing '/' */
	size_t len = strlen(dir);
	if (dir[len - 1] == '/') 
		dir[len - 1] = '\0';

	struct node *root = new_node(DT_DIR, dir, NULL);
	if (root == NULL) {
		fprintf(stderr, "Error: root node is NULL\n");
		return -1;
	}

	if (walk_dir(root) != 0) {
		fprintf(stderr, "Error: cannot walk on root\n");
		free_tree(root);
		return -1;
	}

	
	//print_node(root);
	init_tui(root);
	

	free_tree(root);
	return 0;
}


void usage(void) 
{
	puts("usage: megatron [-d dir]");
	return;
}


/* --- Nodes functions --- */

int walk_dir(struct node *n) 
{
	DIR* dirp;
	struct dirent *r;

	dirp = opendir(n->path);
	if (dirp == NULL) {
		fprintf(stderr, "Error: %s directory cannot be open\n", n->path);
		return -1;
	}

	while ((r = readdir(dirp)) != NULL) {
		if (r->d_name[0] == '.') continue;
		if (r->d_type != DT_REG && r->d_type != DT_DIR) continue;

		struct node *child = new_node(r->d_type, r->d_name, n);
		if (child == NULL) {
			fprintf(stderr, "Error walk_dir(): NULL new_node\n");
			return -1;
		}
		n->children[n->n_children++] = child;

		if (child->type == DT_DIR && walk_dir(child) != 0) 
			return -1;
	}

	sort_childrens(n);
	
	if (closedir(dirp) != 0) {
		perror("walk_dir()");
		return -1;
	}

	return 0;
}

int join_path(char *dst, char *basename, char *filename, int d_type)
{
	memset(dst, '\0', MAXNAME_LEN + 1);
	char *end = (d_type == DT_DIR ? "/" : "");
	if (snprintf(dst, MAXNAME_LEN, "%s%s%s", basename, filename, end) < 0) {
		fprintf(stderr, "Error in join_path(); snprintf\n"); 
		return -1;
	}
	dst[MAXNAME_LEN] = '\0';
	return 0;
}

struct node *new_node(int d_type, char *filename, struct node *parent) 
{
	struct node *n = (struct node *) malloc(sizeof(struct node));
	if (n == NULL) {
		perror("new_node malloc");
		return NULL;
	}

	n->type = d_type;
	
	char *basename = (parent == NULL ? "" : parent->path);
	if (join_path(n->path, basename, filename, d_type) < 0)
		return NULL;

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

		if (n->children[i] != NULL)
			free(n->children[i]);
	}

	if (n->parent == NULL) 
		free(n);
}

void print_node(struct node *n)
{
	printf("==> %s <==\n", n->path);
	for (int i=0; i < n->n_children; i++) {
		if (n->children[i]->type == DT_DIR) {
			print_node(n->children[i]);
		} else {
			puts(n->children[i]->path);
		}
	}
}

static int cmpnodes(const void *a, const void *b)
{
	/* The dereferencing is a bit weird here. 
	qsort(3) pass 'const void *' args to this function.
	Here the input is &n->children[i]. That is,
	the address of a pointer to a node.

	Since the pointer is void, need to cast it:
	  (const struct node * const *)a
	To get the actual pointer to the node, need to dereference:
	  *(const struct node * const *)a
	*/

	const struct node *na = *(const struct node * const *)a;
	const struct node *nb = *(const struct node * const *)b;
	return strcmp(na->path, nb->path);
}

void sort_childrens(struct node *n)
{
	size_t nmemb = (size_t)n->n_children;
	qsort(n->children, nmemb, sizeof(struct node *), cmpnodes);
}


/* --- TUI functions --- */
void disable_raw_mode(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode(void) 
{
	struct termios raw;

	// TODO: set errors for these

	tcgetattr(STDIN_FILENO, &orig_termios);
	// TODO: don't use atexit, is weird.
	atexit(disable_raw_mode);

	raw = orig_termios;
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void init_tui(struct node *n)
{
	printf("is here: %s\n", n->path);
//	struct node *cur_node = root;
	enable_raw_mode();

	char c;
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if (iscntrl(c)) {
			printf("%d\n", c);
		} else {
			printf("%d (%c)\n", c, c);
		}
	}

}
