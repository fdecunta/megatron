#include <sys/ioctl.h>
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

enum cursor_direction { UP, DOWN };
enum { HEADER_ROW = 1, LIST_ROW = 2 };

struct node {
	int type;
	char path[MAXNAME_LEN + 1];
	char filename[MAXNAME_LEN + 1];
	struct node *parent;
	struct node *children[MAX_CHILDS];
	int n_children;
};

struct termios old_settings, new_settings;

int screenrows, screencols;

struct state {
	int row;
	int index;
	int last_row;
	int index_stack[16];
	int depth;
	struct node *node;
};


void 	usage(void);
int 	walk_dir(struct node *n); 
int 	join_path(char *dst, char *basename, char *filename, int d_type);
struct node * 	new_node(int d_type, char *filename, struct node *parent);
void 	free_tree(struct node *n);

static int 	cmpnodes(const void *a, const void *b);
void 	 sort_childrens(struct node *n);

int 	get_window_size(void);
int 	init_screen(void);
int 	end_screen(void);
int 	clear_screen(void);
int 	refresh_screen(void);
int 	tui(struct node *n);
int 	get_cursor_position(int *rows, int *cols);
void 	set_cursor_at(int row, int col);
void 	open_node(void);
void	close_node(void);

void show_cursor(void); 
void hide_cursor(void); 

struct state stt; 

int
main(int argc, char *argv[]) 
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
	tui(root);
	

	free_tree(root);
	return 0;
}


void 
usage(void) 
{
	puts("usage: megatron [-d dir]");
	return;
}


/* --- Nodes functions --- */

int
walk_dir(struct node *n) 
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

int
join_path(char *dst, char *dirname, char *filename, int d_type)
{
	memset(dst, '\0', MAXNAME_LEN + 1);
	char *end = (d_type == DT_DIR ? "/" : "");
	if (snprintf(dst, MAXNAME_LEN, "%s%s%s", dirname, filename, end) < 0) {
		fprintf(stderr, "Error in join_path(); snprintf\n"); 
		return -1;
	}
	dst[MAXNAME_LEN] = '\0';
	return 0;
}

struct node *
new_node(int d_type, char *filename, struct node *parent) 
{

	struct node *n = (struct node *) malloc(sizeof(struct node));
	if (n == NULL) {
		perror("new_node malloc");
		return NULL;
	}

	n->type = d_type;
	
	char *dirname = (parent == NULL ? "" : parent->path);
	if (join_path(n->path, dirname, filename, d_type) < 0)
		return NULL;

	strncpy(n->filename, filename, MAXNAME_LEN);
	size_t len = strlen(filename);
	n->filename[len] = '\0';

	n->parent = parent;
	for (int i=0; i < MAX_CHILDS; i++) { n->children[i] = NULL; }
	n->n_children = (d_type == DT_REG ? -1 : 0);

	return n;
}

void
free_tree(struct node *n)
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

static int 
cmpnodes(const void *a, const void *b)
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

void
sort_childrens(struct node *n)
{
	size_t nmemb = (size_t)n->n_children;
	qsort(n->children, nmemb, sizeof(struct node *), cmpnodes);
}


/* --- tui functions --- */

int
init_screen(void) 
{
	if (tcgetattr(STDIN_FILENO, &old_settings) != -1) {
		new_settings = old_settings;
		/* turn off ICANON and  echo */
		new_settings.c_lflag &= (tcflag_t)~(ICANON | ECHO);

		/* *TODO*: need to remove output processecing? */
		new_settings.c_oflag &= (tcflag_t)~(OPOST);

		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings) == -1) {
			perror("tcsetattr in init_screen()");
			return -1;
		}

		hide_cursor();
	} else {
		perror("enable_raw_mode(); tcgetattr()");
		return -1;
	}
	return 0;
}

int
end_screen(void)
{
	/* TODO: line 180 from screen.c in top from OpenBSD:
	   they use TCSADRAIN. Don't know if should use that */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_settings) == -1) {
		perror("tcsetattr in end_screen()");
		return -1;
	}
	show_cursor();
	return 0;
}

int
clear_screen(void)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);    /* clear all screen */
	return 0;
}

int
get_window_size(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
		screencols = ws.ws_col;
		screenrows = ws.ws_row;
	}
	return 0;
}

int 
get_cursor_position(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

void set_cursor_at(int row, int col)
{	
	size_t len;
	char buf[16];
	len = (size_t)snprintf(buf, 16, "\x1b[%d;%dH", row, col);
	write(STDOUT_FILENO, buf, len);
}

void mv_cursor(int d)
{
	if (d == UP && stt.row != 1 && stt.index != 0) {
		stt.row -= 1;
		stt.index -= 1;
	} 

	if (d == DOWN && (stt.row + 1 < screenrows) && (stt.index + 1 < stt.node->n_children)) {
		stt.row += 1;
		stt.index += 1;
	}
	set_cursor_at(stt.row, 0);
}

void 
show_cursor(void) {
	write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void
hide_cursor(void) {
	write(STDOUT_FILENO, "\x1b[?25l", 6);
}

void
open_node(void) 
{
	if (stt.node->children[stt.index]->type != DT_DIR)
		return;

	stt.index_stack[stt.depth++] = stt.index;
	stt.node = stt.node->children[stt.index];

	stt.index = 0;
	stt.row = LIST_ROW;
	stt.last_row = 0;
}

void
close_node(void)
{
	if (stt.node->parent == NULL)    /* only root has NULL parent */
		return;

	stt.node = stt.node->parent;
	stt.index = stt.index_stack[--stt.depth];
	stt.row = stt.index + LIST_ROW;
}

void
print_bold(const char *s)
{
	size_t len;
	char buf[MAXNAME_LEN + 1];
	memset(buf, '\0', MAXNAME_LEN + 1);

	snprintf(buf, MAXNAME_LEN, "\x1b[1m%s\x1b[0m", s);
	len = (size_t)strlen(buf);

	write(STDOUT_FILENO, buf, len);
}

void
print_fill(const char *s)
{
	size_t len;
	char buf[MAXNAME_LEN + 1];
	memset(buf, '\0', MAXNAME_LEN + 1);

	snprintf(buf, MAXNAME_LEN, "\x1b[7m%s\x1b[0m", s);
	len = (size_t)strlen(buf);

	write(STDOUT_FILENO, buf, len);
}

void
print_normal(const char *s)
{
	size_t len;
	len = (size_t)strlen(s);
	write(STDOUT_FILENO, s, len);
}

void
print_node(void)
{
	int i, printrow;

	set_cursor_at(1, 1);
	clear_screen();

	printf(" === %s ===\r\n", stt.node->path);

	printrow = LIST_ROW;    
	set_cursor_at(printrow, 1);
	for (i = 0; i < stt.node->n_children; i++) {

		if (i == stt.index) {
			print_fill(stt.node->children[i]->filename);
		} else if (stt.node->children[i]->type == DT_DIR) {
			print_bold(stt.node->children[i]->filename);
		} else {
			print_normal(stt.node->children[i]->filename);
		}
		set_cursor_at(++printrow, 1);
	}

	stt.last_row = i + 1;
	set_cursor_at(stt.row, 1);  
}

int
tui(struct node *root)
{
	char ch = 0;

	/* init state */
	stt.row = LIST_ROW;		/* start at 2. 1 is the header with node path */
	stt.index = 0;
	stt.last_row = 0;
	memset(stt.index_stack, -1, sizeof(int) * 16);
	stt.depth = 0;
	stt.node = root;

	/* init screen */
	if (get_window_size() == -1) return -1;
	if (screenrows < 20 || screencols < 50) {
		fprintf(stderr, "Error: terminal is too small. Can't use megatron in a shit like this!\n");
		return -1;
	}

	if (clear_screen() == -1) return -1;
	if (init_screen() == -1) return -1;

	while (ch != 'q') {
		print_node();

		read(STDIN_FILENO, &ch, 1);
		switch (ch) {
			case 'j':
				mv_cursor(DOWN);
				break;
			case 'k':
				mv_cursor(UP);
				break;
			case 'l':
				open_node();
				break;
			case 'h':
				close_node();
				break;
		}

	}

	set_cursor_at(stt.last_row + LIST_ROW, 1);  
	if (end_screen() == -1) return -1;

	return 0;
}
