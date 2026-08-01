#include <dirent.h>
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

void usage(void);

void walk_dir(struct node *n); 
void join_path(char *dst, char *basename, char *filename, int d_type);

struct node *new_node(int d_type, char *filename, struct node *parent);

int main(int argc, char *argv[]) 
{
	int ch;
	char* dir = NULL;

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

	struct stat *st = NULL;
	stat(dir, st);

	printf("%d\n", st->st_mode);

	

//	struct node *root = new_node(DT_DIR, dir, NULL);
	// walk_dir(&root);
//	puts(root->path);
//	free(root);

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

	/* do something as _base_ case */

	while ((r = readdir(dirp)) != NULL) {
		if ((strcmp(r->d_name, ".") == 0) || (strcmp(r->d_name, "..") == 0)) continue;
		if ((r->d_type != DT_DIR) && (r->d_type != DT_REG)) continue;

		/* create the node */

		if (r->d_type == DT_DIR) {
			// TODO: walk _into_ the child
//			walk_dir(name);
		} 
	}

	closedir(dirp);
}


void join_path(char *dst, char *basename, char *filename, int d_type)
{
	char *end = (d_type == DT_DIR ? "/" : "");
	snprintf(dst, MAXNAME_LEN, "%s%s%s", basename, filename, end); 
	dst[MAXNAME_LEN] = '\0';
}


struct node *new_node(int d_type, char *filename, struct node *parent) 
{
	struct node *n = (struct node *) malloc(sizeof(struct node));

	n->type = d_type;

	memset(n->path, '\0', MAXNAME_LEN + 1);
	if (parent == NULL) {
		strncpy(n->path, filename, MAXNAME_LEN);
		n->path[MAXNAME_LEN] = '\0';
	} else {
		join_path(n->path, parent->path, filename, d_type);
	}

	n->parent = parent;

	for (int i=0; i < MAX_CHILDS; i++) { n->children[i] = NULL; }

	n->n_children = (d_type == DT_REG ? -1 : 0);

	return n;
}
