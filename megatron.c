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
#include <time.h>
#include <unistd.h>

#include "config.h" 	/* load DEFAUTL_DIR and HISTORY_FILE */

#define MAXNAME_LEN 255 	/* path name must be no longer than this */
#define MAX_CHILDS  256 	/* nodes max children nodes */
#define MAX_DEPTH   16 		/* max depth in filetree */ 

enum cursor_direction { UP, DOWN };
enum { HEADER_ROW = 1, LIST_ROW = 3};

struct node {
	int type;
	char path[MAXNAME_LEN + 1];
	char filename[MAXNAME_LEN + 1];
	struct node *parent;
	struct node *children[MAX_CHILDS];
	int n_children;
};

struct state {
	int highlight_row;
	int index;
	int index_top_slice;
	struct node *node;
};

struct stack {
	int depth;
	struct state state[MAX_DEPTH];
};

const char *video_ext[] = {
	"mkv",
	"mp4",
	"avi",
	"mpg",
	"mpeg",
	"mov",
	"wmv",
	NULL
};  


void		close_node(void);
static int 	cmpnodes(const void *a, const void *b);
int		history_assert_file(void);
int		history_print(void);
int		history_write(const char *path);
int 		is_video(const char *filename);
int 		join_path(char *dst, char *basename, char *filename, int d_type);
int 		node_build_tree(struct node *n); 
struct node * 	node_create(int d_type, char *filename, struct node *parent);
void 		node_free_tree(struct node *n);
void 		node_sort_childrens(struct node *n);
void 		open_node(void);
void		play(void);
void 		print_header(const char *s);
int 		screen_clear(void);
int 		screen_end(void);
int 		screen_get_winsize(void);
int 		screen_init(void);
void 		screen_cursor_mv(int d);
void 		screen_set_cursor(int row, int col);
int 		tui(struct node *n);
void 		usage(void);

struct termios old_settings, new_settings;
int screenrows, screencols;
struct state stt; 
struct stack state_stack;


int
main(int argc, char *argv[]) 
{
	errno = 0;
	int ch;
	char *dir = NULL;

	if (history_assert_file() != 0)
		return 1;

	while ((ch = getopt(argc, argv, "H")) != -1) {
		switch (ch) {
		case 'H':
			/* todo: add err handling */
			history_print();
			return 0;
		default:
			usage();
			return 0;
		}
	}
	argc -= optind;
	argv += optind;

	dir = strdup((argc == 0 ? DEFAULT_DIR : *argv));

	/* check dir is a directory */
	struct stat sb;
	if (stat(dir, &sb) == -1) {
		if (errno == ENOENT) 
			fprintf(stderr, "Can't find %s\n", dir);
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

	struct node *root = node_create(DT_DIR, dir, NULL);
	if (root == NULL) {
		fprintf(stderr, "Error: root node is NULL\n");
		return -1;
	}

	if (node_build_tree(root) != 0) {
		fprintf(stderr, "Error: cannot walk on root\n");
		node_free_tree(root);
		return -1;
	}

	tui(root);

	free(dir);
	node_free_tree(root);
	return 0;
}

void 
usage(void) 
{
	puts("usage: megatron [-H] [dir]");
	puts("  -H  print history");
}

/* --- Nodes functions --- */

int
node_build_tree(struct node *n) 
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
		if (r->d_type != DT_DIR &&
			r->d_type == DT_REG && 
			is_video(r->d_name) == 0) continue;

		if (n->n_children >= MAX_CHILDS) {
			fprintf(stderr, "Warning: %s truncated at %d entries", 
				n->path, MAX_CHILDS); 
			break;
		}
			
		struct node *child = node_create(r->d_type, r->d_name, n);
		if (child == NULL) {
			fprintf(stderr, "Error node_build_tree: NULL node_create\n");
			return -1;
		}
		n->children[n->n_children++] = child;

		if (child->type == DT_DIR && node_build_tree(child) != 0) 
			return -1;
	}

	node_sort_childrens(n);
	
	if (closedir(dirp) != 0) {
		perror("node_build_tree");
		return -1;
	}

	return 0;
}

int
is_video(const char *filename)
{
	char *ext = strrchr(filename, '.');
	if (ext == NULL) 
		return 0;

	for (int j = 0; video_ext[j] != NULL; j++) {
		if (!strcmp(ext + 1, video_ext[j]))
			return 1;
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
node_create(int d_type, char *filename, struct node *parent) 
{

	struct node *n = (struct node *) malloc(sizeof(struct node));
	if (n == NULL) {
		perror("node_create malloc");
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
node_free_tree(struct node *n)
{
	for (int i=0; i < n->n_children; i++) {
		if (n->children[i]->type == DT_DIR) {
			node_free_tree(n->children[i]);
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
node_sort_childrens(struct node *n)
{
	size_t nmemb = (size_t)n->n_children;
	qsort(n->children, nmemb, sizeof(struct node *), cmpnodes);
}

/* --- tui functions --- */

int
screen_init(void) 
{
	if (tcgetattr(STDIN_FILENO, &old_settings) != -1) {
		new_settings = old_settings;
		/* turn off ICANON and  echo */
		new_settings.c_lflag &= (tcflag_t)~(ICANON | ECHO);

		new_settings.c_oflag &= (tcflag_t)~(OPOST);

		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings) == -1) {
			perror("tcsetattr in screen_init()");
			return -1;
		}

	} else {
		perror("enable_raw_mode(); tcgetattr()");
		return -1;
	}
	return 0;
}

int
screen_end(void)
{
	/* TODO: line 180 from screen.c in _top_ from OpenBSD:
	   they use TCSADRAIN. Don't know if should use that */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_settings) == -1) {
		perror("tcsetattr in screen_end()");
		return -1;
	}
	return 0;
}

int
screen_clear(void)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);    /* clear all screen */
	return 0;
}

int
screen_get_winsize(void)
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

void 
screen_set_cursor(int row, int col)
{	
	size_t len;
	char buf[16];
	len = (size_t)snprintf(buf, 16, "\x1b[%d;%dH", row, col);
	write(STDOUT_FILENO, buf, len);
}

void 
mv_cursor(int d)
{
	if (d == UP && stt.index != 0) {
		if (stt.highlight_row == LIST_ROW) {
			stt.index_top_slice -= 1;
		} else {
			stt.highlight_row -= 1;
		}
		stt.index -= 1;
	} 

	if (d == DOWN && stt.index + 1 < stt.node->n_children) {
		if (stt.highlight_row == screenrows) {
			stt.index_top_slice += 1;
		} else {
			stt.highlight_row += 1;
		}
		stt.index += 1;
	}
	screen_set_cursor(stt.highlight_row, 0);
}

void
open_node(void) 
{
	if (stt.node->n_children == 0 || stt.node->children[stt.index]->type != DT_DIR)
		return;

	if (state_stack.depth >= MAX_DEPTH) {
		print_header("Max depth reacherd!");
		return;
	}

	/* put state in stack */
	state_stack.state[state_stack.depth++] = stt;

	/* start a clean state */
	stt.node = stt.node->children[stt.index];
	stt.index = stt.index_top_slice = 0;
	stt.highlight_row = LIST_ROW;
}

void
close_node(void)
{
	if (stt.node->parent != NULL)    
		stt = state_stack.state[--state_stack.depth];
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
print_underline(const char *s)
{
	size_t len;
	char buf[MAXNAME_LEN + 1];
	memset(buf, '\0', MAXNAME_LEN + 1);

	snprintf(buf, MAXNAME_LEN, "\x1b[4m%s\x1b[0m", s);
	len = (size_t)strlen(buf);

	write(STDOUT_FILENO, buf, len);
}

void
print_underlinebold(const char *s)
{
	size_t len;
	char buf[MAXNAME_LEN + 1];
	memset(buf, '\0', MAXNAME_LEN + 1);

	snprintf(buf, MAXNAME_LEN, "\x1b[1m\x1b[4m%s\x1b[0m", s);
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
print_header(const char *s)
{
	size_t len;
	char *header;

	if ((header = strdup(s)) == NULL) {
		perror("strdup");
		return;
	}

	len = strlen(header);
	if ((int)len >= screencols) {
		int i;
		for (i = 4; i > 1; i--)
			header[screencols - i] = '.';
		header[screencols - 1] = '\0';

		len = strlen(header);
	}
	
	screen_set_cursor(HEADER_ROW, 1);
	write(STDOUT_FILENO, header, len);

	free(header);
	return;
}

void
print_node(void)
{
	screen_set_cursor(1, 1);
	screen_clear();

	print_header(stt.node->path);

	/* prepare to print list of files */
	int printed_row = LIST_ROW;    
	screen_set_cursor(LIST_ROW, 1);

	if (stt.node->n_children == 0) 
		print_normal("Empty: no videos nor dirs");

	/* 
	 * This loop prints the list of files and directories.
	 *
	 * It iterates over the children nodes from the current nude
	 * and prints each one using a different style.
	 * 
	 * normal 		regular file
	 * bold 		directories
	 * underscore 		selected file 
	 * bold and underscore 	selected dir
	 * 
	 * The iteration has two rules. 
	 * 1. Stop when there are no more children nodes.
	 * 2. Stop when screen has no empty rows. 
	 * 
	 * For long lists, scrolling is implemented by printing a slice
	 * of the children nodes array. Note that 'i' is initialized as 
	 * 'index_top_slice'.
	 *
	 * For long lists, the loop stops when all the printable rows
	 * have been filled. 
	 * This happens when the number of printed items (i - index_top_slice)
	 * is equal to the number of printable rows (screenrows - LIST_ROW)
	 */
	
	for (int i = stt.index_top_slice; i < stt.node->n_children && i - stt.index_top_slice <= screenrows - LIST_ROW; i++) {
		char *s = strdup(stt.node->children[i]->filename);
		if (s == NULL) {
			perror("strdup");
			return;
		}

		if ((int)strlen(s) > screencols) {
			int j;
			for (j = 4; j > 1; j--)
				s[screencols - j] = '.';
			s[screencols - 1] = '\0';
		}

		if (i == stt.index && stt.node->children[i]->type == DT_DIR) {
			print_underlinebold(s);
		} else if (i == stt.index) {
			print_underline(s);
		} else if (stt.node->children[i]->type == DT_DIR) {
			print_bold(s);
		} else {
			print_normal(s);
		}

		screen_set_cursor(++printed_row, 1);
		free(s);
	}
	screen_set_cursor(stt.highlight_row, 1);  
}

void
play(void)
{
	struct node *sel = stt.node->children[stt.index];
	if (sel->type != DT_REG)
		return;
	
	history_write(sel->path);
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork failed");
		return;
	} 
	else if (pid == 0) {
		execlp("vlc", "vlc", "--play-and-exit", sel->path, (char *)NULL);
	} else {
		;
	}
}

int
tui(struct node *root)
{
	char ch = 0;

	state_stack.depth = 0;

	stt.highlight_row = LIST_ROW;
	stt.node = root;

	/* init screen */
	if (screen_get_winsize() == -1) return -1;
	if (screenrows < 20 || screencols < 50) {
		fprintf(stderr, "Error: terminal is too small. Can't use megatron in a shit like this!\n");
		return -1;
	}

	if (screen_clear() == -1) return -1;
	if (screen_init() == -1) return -1;

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
			case 'p':
				play();
				break;
		}
	}
	screen_set_cursor(LIST_ROW + stt.node->n_children - stt.index_top_slice, 1);
	write(STDOUT_FILENO, "\x1b[0K", 4);    /* clear line */
	if (screen_end() == -1) return -1;

	return 0;
}

/* --- history functions --- */

int
history_assert_file(void)
{
	struct stat sb;
	if (stat(HISTORY_FILE, &sb) == -1 || !S_ISREG(sb.st_mode)) {
		perror("Read history");
		fprintf(stderr, "File does not exist: %s\n", HISTORY_FILE);
		return -1;
	}
	return 0;
}

int
history_print(void)
{
	FILE *fp = fopen(HISTORY_FILE, "r");
	if (fp == NULL) {
		perror("print_history");
		return 1;
	}

	int ch;
	while ((ch = fgetc(fp)) != EOF) 
		putchar(ch);
	
	fclose(fp);
	return 0;
}

int
history_write(const char *path)
{
	FILE *fp = fopen(HISTORY_FILE, "a");
	if (fp == NULL) {
		perror("print_history");
		return 1;
	}
	
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char timestr[64];
	strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(fp, "%s\t%s\n", timestr, path);

	fclose(fp);
	return 0;

}
