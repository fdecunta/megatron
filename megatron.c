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

struct node {
	int type;
	char path[MAXNAME_LEN + 1];
	struct node *parent;
	struct node *children[MAX_CHILDS];
	int n_children;
};

enum { OK = 0, ERR_BAD_CMD = -1 };

void 	usage(void);
void 	walk_dir(struct node *n); 
void 	join_path(char *dst, char *basename, char *filename, int d_type);
struct node * 	new_node(int d_type, char *filename, struct node *parent);
void 	free_tree(struct node *n);

static int cmpnodes(const void *a, const void *b);
void sort_childrens(struct node *n);

void print_node(struct node *n);


int main(int argc, char *argv[]) 
{
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

//	struct node *cur_node = root;

	print_node(root);
	

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

	sort_childrens(n);

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

void print_help(void)
{
	// TODO! //
	puts("megatron");
	puts("\t your buddy");
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
