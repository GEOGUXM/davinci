#include "cvector.h"
#include "dvio.h"
#include "ff_source.h"

#include "parser.h"
#include "system.h"
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// include specific libraries for finding the DV_EXEPATH
#if defined(__APPLE__)
#include <libproc.h>

#elif defined(_WIN32)
//#include <windows.h>

#elif defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef HAVE_XT
#define USE_X11_EVENTS 1
#endif

#ifdef USE_X11_EVENTS
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>

Widget applicationShell = NULL;
XtAppContext applicationContext;
#endif


static int windows = 1;
int usage(char* prog);

// Used for tab completion
extern int num_internal_funcs;
extern struct _vfuncptr vfunclist[];

extern avl_tree_t ufuncs_avl;

////////////////////




/**
 ** Command line args:
 **
 **     -f filename     - read filename for input rather than stdin
 **     -l logfile      - log commands to logfile rather than '.dvlog'
 **     -q              - quick.  Don't load startup file
 **     -e string       - eval.  Evaluate string as commands, then exit
 **     -i              - interactive.  Force an interactive shell

 **/
void fake_data();
void env_vars();
void log_time();
void lhandler(char* line);
void quit(int);
void parse_buffer(char* buf);

void init_history(char* fname);
void process_streams(void);
void event_loop(void);

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>

char** dv_complete_func(const char* text, int start, int end);
char** dv_null_func(const char* text, int start, int end) { return NULL; }

#endif

jmp_buf env;

void user_sighandler(int data)
{
#ifndef __MINGW32__
	signal(SIGUSR1, user_sighandler);
#else
	parse_error("Function not spported under Windows.");
#endif
}

void dv_sighandler(int data)
{
	Scope* scope;
	char* path = getenv("TMPDIR");

	switch (data) {

	case (SIGSEGV):
		rmrf(path);
		signal(SIGSEGV, SIG_DFL);
		break;

#if !(defined(__CYGWIN__) || defined(__MINGW32__))
	case (SIGBUS):
		rmrf(path);
		signal(SIGBUS, SIG_DFL);
		break;
#else
		parse_error("Function not spported under Windows.");
#endif /* __CYGWIN__ */

	case (SIGINT):
		signal(SIGINT, SIG_IGN);
		while ((scope = scope_tos()) != global_scope()) {
			//NOTE(rswinkle): This does nothing!
			dd_unput_argv(scope);

			scope_pop();
		}

		signal(SIGINT, dv_sighandler);
		longjmp(env, 1);
		break;
	}
}

/* char *__progname = "davinci"; */




int main(int ac, char** av)
{
	Var* v;
	FILE* fp;
	char path[256];
	int quick = 0;
	int i, j, k, flag = 0;
	char* logfile = NULL;
	char* p;
	int history = 1;

#if defined(__APPLE__)
	int ret;
	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
	// add 12 for length of ENV Variable Name
	char exe_path_out[PROC_PIDPATHINFO_MAXSIZE + 12];
	pid_t pid;
#elif defined(_WIN32)
	//char pathbuf[MAX_PATH];
	// add 12 for length of ENV Variable Name
	//char exe_path_out[MAX_PATH + 12];
	//HMODULE hModule;
#elif defined(__linux__)
	char pidpath[PATH_MAX];
	char pathbuf[PATH_MAX];
	// add 12 for length of ENV Variable Name
	char exe_path_out[PATH_MAX + 12];
	struct stat info;
	pid_t pid;
#endif




	// TODO(rswinkle): put all this in a general "init" function
	init_input_stack();
	init_scope_stack();
	init_ufunc_tree();

	Scope scope;
	init_scope(&scope);
	scope_push(&scope);
	Scope* s = scope_stack_back();

	//sort internal function list to speed up tab completion matching
	//and present them in alphabetical order when double TABing for multiple matches
	//also lets us do bsearch for function dispatch in ff.c:V_func()
	//
	//note cmp_string only works because name is the first member of _vfuncptr
	qsort(vfunclist, num_internal_funcs, sizeof(struct _vfuncptr), cmp_string);




	signal(SIGINT, dv_sighandler);
	signal(SIGSEGV, dv_sighandler);
#ifndef __MINGW32__
	signal(SIGPIPE, SIG_IGN);
	signal(SIGBUS, dv_sighandler);
	signal(SIGUSR1, user_sighandler);
#endif

	/**
	 ** handle $0 specially.
	 **/
	v         = p_mkval(ID_STRING, *av);
	V_NAME(v) = strdup("$0");
	put_sym(v);

// get the full path in different ways depending on the OS
#if defined(__APPLE__)
	// use the pid to get the path
	pid = getpid();
	ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));

	// if we succeed pathbuf will have the path and ret==1
	if (ret > 0) {
		// set DV_EXEPATH environent variable
		sprintf(exe_path_out, "DV_EXEPATH=%s", pathbuf);
		putenv(exe_path_out);
	}

	// also set the DV_OS enviornment variable
	putenv("DV_OS=mac");

#elif defined(_WIN32)
	// Will contain exe path
	/*
	hModule = GetModuleHandle(NULL);
	if (hModule != NULL) {
	    GetModuleFileName(hModule, pathbuf, (sizeof(pathbuf)));

	    //set the DV_EXEPATH environment variable
	    sprintf(exe_path_out, "DV_EXEPATH=%s", pathbuf);
	    putenv(exe_path_out);
	}
	//also set the DV_OS environment variable
	*/
	putenv("DV_OS=win");

#elif defined(__linux__)
	// use the pid to get the relative link
	pid = getpid();
	sprintf(pidpath, "/proc/%d/exe", pid);

	//char mypathbuf[4097];
	//getcwd(mypathbuf, 4097);
	// printf("\ncwd=\n%s\n\n", mypathbuf);

	// resolve the link with readlink
	size_t path_len = readlink(pidpath, pathbuf, PATH_MAX);
	if (path_len != -1) {
		pathbuf[path_len] = 0; // readlink doesn't NULL terminate
		sprintf(exe_path_out, "DV_EXEPATH=%s", pathbuf);
		putenv(exe_path_out);
	}

	// also set the DV_OS environment variable
	putenv("DV_OS=linux");
#endif

	/**
	 ** Everything that starts with a - is assumed to be an option,
	 ** until we get something that doesn't.
	 **
	 ** The user can force this with --, as well.
	 **
	 ** We now pass all "--options" as ARGV parameters.
	 **/
	for (i = 1; i < ac; i++) {
		k = 0;
		if (!flag && av[i] && av[i][0] == '-' && !(strlen(av[i]) > 2 && av[i][1] == '-')) {
			for (j = 1; j < strlen(av[i]); j++) {
				switch (av[i][j]) {
				case '-': /* last option */ flag   = 1; break;
				case 'w': /* no windows */ windows = 0; break;
				case 'f': { /* redirected input file */
					k++;
					push_input_file(av[i + k]);
					av[i + k]   = NULL;
					interactive = 0;
					break;
				}
				case 'l': { /* redirected log file */
					k++;
					logfile   = av[i + k];
					av[i + k] = NULL;
					break;
				}
				case 'e': { /* execute given command string */
					FILE* fp;
					k++;
					if ((fp = tmpfile()) != NULL) {
						fputs(av[i + k], fp);
						fputc('\n', fp);
						rewind(fp);
						push_input_stream(fp, ":command line:");
						interactive = 0;
					}
					av[i + k] = NULL;
					break;
				}
				case 'v': {
					if (isdigit(av[i][j + 1])) {
						VERBOSE = av[i][j + 1] - '0';
					} else {
						k++;
						if (i + k >= ac) {
							exit(usage(av[0]));
						} else {
							VERBOSE   = atoi(av[i + k]);
							av[i + k] = NULL;
						}
					}
					break;
				}
				case 'q': {
					quick = 1;
					break;
				}
				case 'H': {
					/* force loading of the history, even in quick mode */
					history = 1;
					break;
				}
				case 'V': {
					dump_version();
					exit(1);
				}
				case 'h': {
					usage(av[0]);
					exit(1);
				}
				}
			}
			i += k;
		} else {
			char buf[256];
			flag = 1;
			dd_put_argv(s, p_mkval(ID_STRING, av[i]));
			v = p_mkval(ID_STRING, av[i]);
			sprintf(buf, "$%d", i);
			V_NAME(v) = strdup(buf);
			put_sym(v);
		}
	}
	dv_set_iom_verbosity();

	env_vars();
	fake_data();

	if (interactive) {
		if (logfile == NULL) logfile = (char*)".dvlog";

		lfile = fopen(logfile, "a");
		log_time();
		if (quick == 0 || history == 1) init_history(logfile);
#ifdef HAVE_LIBREADLINE
		/* JAS FIX */

		rl_attempted_completion_function = dv_complete_func;

		//to turn off tab completion (except filename completion which is free)
		//rl_attempted_completion_function = dv_null_func;
#endif
	}
	if (quick == 0) {
		sprintf(path, "%s/.dvrc", getenv("HOME"));
		if ((fp = fopen(path, "r")) != NULL) {
			printf("reading file: %s\n", path);
			push_input_stream(fp, path);
		}
	}

	/**
	 ** set up temporary directory
	 **/
	if ((p = getenv("TMPDIR")) == NULL) {
		sprintf(path, "TMPDIR=%s/dv_%d", P_tmpdir, getpid());
	} else {
		sprintf(path, "TMPDIR=%s/dv_%d", getenv("TMPDIR"), getpid());
	}

#ifndef _WIN32
	mkdir(path + 7, 0777);
#else
	mkdir(path + 7);
#endif
	putenv(path);

	/*
	** Before we get to events, process any pushed files
	*/
	/* moved the process_streams into the event loop so
	     that it happens after Xt is initialized, but before
	     the endless loop starts
	*/
	event_loop();
	quit(0);

	/* event_loop never returns... unless we're not interactive */

	return 0;
}

#if defined(USE_X11_EVENTS) && defined(HAVE_LIBREADLINE)
/* ARGSUSED */
void get_file_input(XtPointer client_data, int* fid, XtInputId* id)
{
	rl_callback_read_char();
}
#endif

#ifdef HAVE_XT

/* FIX: move to gui.c */

static const String defaultAppResources[] = {
    "*TopLevelShell.allowShellResize: true", "*vicar.xZoomIn: 1", "*vicar.xZoomOut: 1",
    "*vicar.yZoomIn: 1", "*vicar.yZoomOut: 1", "*vicar.viewWidth: 640", "*vicar.viewHeight: 480",
    "*vicar.tileWidth: 256", "*vicar.tileHeight: 256", "*vicar.allowShellResize: True",
    "*vicar.shadowThickness: 1",
    /* NOTE: the following is one long string.  Don't add commas. */
    "*vicar.translations: #augment \\n "
    "~Shift<Btn2Down>:            MousePanStart() \\n "
    "~Shift<Btn2Motion>:          MousePan() \\n "
    "~Shift~Ctrl~Meta<Key>osfLeft:    PanOne(left) \\n "
    "~Shift~Ctrl~Meta<Key>osfRight:   PanOne(right) \\n "
    "~Shift~Ctrl~Meta<Key>osfUp:      PanOne(up) \\n "
    "~Shift~Ctrl~Meta<Key>osfDown:    PanOne(down) \\n "
    "Ctrl~Shift~Meta<Key>osfLeft:     PanEdge(left) \\n "
    "Ctrl~Shift~Meta<Key>osfRight:    PanEdge(right) \\n "
    "Ctrl~Shift~Meta<Key>osfUp:       PanEdge(up) \\n "
    "Ctrl~Shift~Meta<Key>osfDown:     PanEdge(down) \\n "
    "Shift~Ctrl~Meta<Key>osfLeft:     PanHalfView(left) \\n "
    "Shift~Ctrl~Meta<Key>osfRight:    PanHalfView(right) \\n "
    "Shift~Ctrl~Meta<Key>osfUp:       PanHalfView(up) \\n "
    "Shift~Ctrl~Meta<Key>osfDown:     PanHalfView(down) \\n "
    "<Key>osfActivate:            Input(\"Return hit\") \\n "
    "<Btn1Down>:              Input(\"Draw\",\"start\") \\n "
    "Button2<Key>space:           Input(\"Draw\",\"mark\") \\n "
    "<Btn1Motion>:            Input(\"Draw\",\"drag\") \\n "
    "<Btn1Up>:                Input(\"Draw\",\"end\") \\n "
    "<Key>osfEscape:          CursorMode(toggle) \\n "
    "~Shift<Key>grave:            CursorMode(toggle) \\n "
    "<Key>asciitilde:         CursorMode(toggle,true) \\n "
    "Shift<Key>grave:         CursorMode(toggle,true) \\n "
    "<Key>plus:               CursorMode(floating) \\n "
    "<Key>minus:              CursorMode(planted) \\n "
    "Shift<Motion>:           MoveCursorMouse() \\n "
    "<Key>c:              MoveCursorMouse() \\n "
    "Shift Ctrl<Key>osfLeft:      MoveCursor(left) \\n "
    "Shift Ctrl<Key>osfRight:     MoveCursor(right) \\n "
    "Shift Ctrl<Key>osfUp:        MoveCursor(up) \\n "
    "Shift Ctrl<Key>osfDown:      MoveCursor(down) \\n "
    "Meta<Key>osfLeft:            MoveCursorScreen(left) \\n "
    "Meta<Key>osfRight:           MoveCursorScreen(right) \\n "
    "Meta<Key>osfUp:          MoveCursorScreen(up) \\n "
    "Meta<Key>osfDown:            MoveCursorScreen(down) \\n "
    "<Visible>:               Input(\"VisibilityNotify\")",
    /* NOTE: end of long string. */
    NULL};

#endif

void event_loop(void)
{
	if (interactive) {
#if !defined(USE_X11_EVENTS) || !defined(HAVE_LIBREADLINE)
		/* JAS FIX */
		process_streams();
		lhandler((char*)readline("dv> "));
#else
		// JAS FIX: this should work even with a null DISPLAY, if -display is set..i think there are
		// better
		// ways to get the display as well..
		if (windows && getenv("DISPLAY") != NULL) {
			// JAS FIX: what is this argv/argv manglation?
			char* argv[1];
			const char* av0  = "null";
			int argc         = 1;
			argv[0]          = (char*)av0;
			applicationShell = XtVaAppInitialize(&applicationContext, "Davinci", NULL, 0, &argc,
			                                     argv, (char**)defaultAppResources, NULL);

		} else {
			/**
			 ** This is a hack to let us use the Xt event model, without
			 ** needing an X server.  It is a bad idea.
			 **/
			XtToolkitInitialize();
			applicationContext = XtCreateApplicationContext();
		}

		XtAppAddInput(applicationContext, fileno(stdin), (void*)XtInputReadMask, get_file_input, NULL);

		process_streams();
		rl_callback_handler_install("dv> ", lhandler);
		XtAppMainLoop(applicationContext);
#endif
	} else {
		/* not interactive, but still have to process the input streams,
		   or -e (and scripts) will never work. */
		process_streams();
	}
}
void lhandler(char* line)
{
	char* buf = NULL;
	char prompt[256];
	extern int pp_line;
	extern int pp_count;

#if !defined(USE_X11_EVENTS) || !defined(HAVE_LIBREADLINE)
	/* JAS FIX */
	while (1) {
#endif

		/**
		 ** Readline has a whole line of input, process it.
		 **/
		if (line == NULL) {
			quit(0);
		}

		if ((buf = (char*)malloc(strlen(line) + 2)) == NULL) {
			parse_error("Unable to alloc %ld bytes.\n", strlen(line) + 2);
			quit(0);
		}
		strcpy(buf, line);
		strcat(buf, "\n");

		if (*line) {
			add_history((char*)line);
			log_line(buf);
		}

		pp_line++;
		pp_count = 0;

		parse_buffer(buf);

		setjmp(env);

		/*
		** Process anything pushed onto the stream stack by the last command.
		*/
		process_streams();

		if (indent) {
			sprintf(prompt, "%2d> ", indent);
		} else if (continuation) {
			continuation = 0;
			sprintf(prompt, "  > ");
		} else if (in_comment) {
			sprintf(prompt, "/*> "); /* nothing */
		} else if (in_string) {
			sprintf(prompt, "\" > ");
		} else {
			sprintf(prompt, "dv> ");
		}

#if defined(USE_X11_EVENTS) && defined(HAVE_LIBREADLINE)
		/* JAS FIX */
		rl_callback_handler_install(prompt, lhandler);
		free(buf);
#else
	line = (char*)readline(prompt);
	free(buf);
}
#endif
	}

void process_streams(void)
{
	char buf[1024];
	extern int pp_line;

	// Process anything that has been pushed onto the input stream stack.
	while (input_stack_size()) {
		while (fgets(buf, 1024, top_input_file()) != NULL) {
			parse_buffer(buf);
			pp_line++;
		}
		pop_input_file();
	}
}

extern Var* curnode;

void parse_buffer(char* buf)
{
	int i, j = 0;
	extern char* yytext;
	Var* v = NULL;
	void* parent_buffer;
	void* buffer;
	Var* node;
	extern char* pp_str;

	extern void* get_current_buffer();
	extern void* yy_scan_string();
	extern void yy_delete_buffer(void*);
	extern void yy_switch_to_buffer(void*);

	parent_buffer = (void*)get_current_buffer();
	buffer        = (void*)yy_scan_string(buf);
	pp_str        = buf;

	curnode = NULL;

	while ((i = yylex()) != 0) {
		/*
		** if this is a function definition, do no further parsing yet.
		*/
		j = yyparse(i, (Var*)yytext);
		if (j == -1) quit(0);

		if (j == 1 && curnode != NULL) {
			node = curnode;
			evaluate(node);

			v = pop(scope_tos());

			pp_print(v);
			free_tree(node);
			indent = 0;
			cleanup(scope_tos());
		}
	}

	yy_delete_buffer((struct yy_buffer_state*)buffer);
	if (parent_buffer) yy_switch_to_buffer(parent_buffer);
}

void log_line(char* str)
{
	if (lfile) {
		fprintf(lfile, "%s", str);
		fflush(lfile);
	}
}

void fake_data()
{
	Var* v;
	int i, j;

	v         = newVar();
	V_NAME(v) = strdup("a");
	V_TYPE(v) = ID_VAL;
	v         = put_sym(v);

	V_DSIZE(v)   = 4 * 3 * 2;
	V_SIZE(v)[0] = 4;
	V_SIZE(v)[1] = 3;
	V_SIZE(v)[2] = 2;
	V_ORG(v)     = BSQ;
	V_FORMAT(v)  = DV_FLOAT;
	V_DATA(v)    = calloc(4 * 3 * 2, sizeof(float));

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 3; j++) {
			((float*)V_DATA(v))[i + j * 4] = (float)i + j * 4;
		}
	}

	// srand48(getpid());
	srand((unsigned int)time(NULL));
	srand48((unsigned int)time(NULL));

	for (i = 0; i < 12; i++) {
#ifdef __MINGW32__
		((float*)V_DATA(v))[i + 12] = ((double)rand()) / ((double)(RAND_MAX));
// for some reason calling drand48()
// messes up the application
#else
	((float*)V_DATA(v))[i + 12] = drand48();
#endif
	}

	// Set VZERO, read only variable used twice in pp_math.c when
	// variables are NULL they're set to this.
	v            = (Var*)calloc(1, sizeof(Var));
	V_NAME(v)    = NULL;
	V_TYPE(v)    = ID_VAL;
	V_DSIZE(v)   = 1;
	V_SIZE(v)[0] = 1;
	V_SIZE(v)[1] = 1;
	V_SIZE(v)[2] = 1;
	V_ORG(v)     = BSQ;
	V_FORMAT(v)  = DV_UINT8;
	V_DATA(v)    = calloc(1, sizeof(u8));
	VZERO        = v;


	v         = newVar();
	V_NAME(v) = strdup("tmp");
	V_TYPE(v) = ID_VAL;
	v         = put_sym(v);

	V_DSIZE(v)   = 2 * 2 * 2;
	V_SIZE(v)[0] = 2;
	V_SIZE(v)[1] = 2;
	V_SIZE(v)[2] = 2;
	V_ORG(v)     = BSQ;
	V_FORMAT(v)  = DV_FLOAT;
	V_DATA(v)    = calloc(2 * 2 * 2, sizeof(float));

	for (i = 0; i < 8; i++) {
		((float*)V_DATA(v))[i] = (float)i;
	}
}

void env_vars()
{
	char* path;
	Var* s;

	if ((path = getenv("DATAPATH")) != NULL) {
		s           = newVar();
		V_NAME(s)   = strdup("datapath");
		V_TYPE(s)   = ID_STRING;
		V_STRING(s) = strdup(path);
		put_sym(s);
	}
}

void log_time()
{
	time_t t;
	char* uname;
	char tbuf[30];
	char* host;
	char cwd[1024];
	if (lfile) {
		t = time(0);
/* eandres: ctime() seems to return invalid pointers on x86_64 systems.
* ctime_r with a provided buffer fills the buffer correctly, but still
* returns an invalid pointer. */

#ifdef __MINGW32__
		time(&t);
		strcpy(tbuf, ctime(&t));
#elif __sun
	ctime_r(&t, tbuf, sizeof(tbuf));
#else
	ctime_r(&t, tbuf);
#endif

		/* eandres: This really shouldn't be a problem, but it shouldn't be a crash, either. */
		if ((uname = getenv("USER")) == NULL) {
			uname = (char*)"<UNKNOWN>";
		}
		if ((host = getenv("HOST")) == NULL) {
			host = (char*)"<UNKNOWN>";
		}
		if (getcwd(cwd, 1024) == NULL) {
			strcpy(cwd, "<UNKNOWN>");
		}

		fprintf(lfile, "###################################################\n");
		fprintf(lfile, "# User: %8.8s    Date: %26.26s", uname, tbuf);
		fprintf(lfile, "# Host: %-11s Cwd: %s\n", host, cwd);
		fprintf(lfile, "###################################################\n");
	}
}

void init_history(char* fname)
{
	char buf[2048];
	FILE* fp;

	if ((fp = fopen(fname, "r")) == NULL) return;
	printf("loading history\n");

	while (fgets(buf, sizeof(buf) - 1, fp)) {
		if (buf[0] == '#') continue;
		buf[strlen(buf) - 1] = '\0';
		add_history((char*)buf);
	}
}




#ifdef HAVE_LIBREADLINE

char* member_generator(const char* text, int state);

// Generator function for command completion.  STATE lets us know whether
// to start from scratch; without any state (i.e. STATE == 0), then we
// start at the top of the list.
char* command_generator(const char* text, int state)
{
	static int list_index, len, search_state, mem_state;
	static avl_node_t* tree_node;

	Scope* scope = global_scope();

	cvector_varptr* vec = &scope->symtab;
	Var* v;

	// If this is a new word to complete, initialize now.  This includes
	// saving the length of TEXT for efficiency, and initializing the index
	// variable to 0.
	if (!state) {
		len = strlen(text);
		search_state = 0;
		list_index = 0;
		mem_state = 0;
		tree_node = NULL;
	}

	int i, cmp_result;

	// Return the next name which partially matches from the command list.
	
	// search builtin functions
	if (search_state == 0) {
		for (i=list_index; i<num_internal_funcs; ++i) {
			cmp_result = strncmp(vfunclist[i].name, text, len);
			if (cmp_result == 0) {
				list_index = i+1;
				return strdup(vfunclist[i].name);
			} else if (cmp_result > 0) {
				// since they're alphabetical if name > text we're done
				break;
			}
		}
		search_state++;
		list_index = 0;
	}

	//search user defined functions
	if (search_state == 1) {
		if (!list_index) {
			tree_node = avl_head(&ufuncs_avl);
			list_index = 1;
		}
		while (tree_node) {
			UFUNC* u = avl_ref(tree_node, UFUNC, node);
			cmp_result = strncmp(u->name, text, len);

			//since we're going through the tree in alphabetical order
			//if u->name > text we're past any possible matches
			if (cmp_result > 0)
				break;

			tree_node = avl_next(tree_node);

			if (cmp_result == 0)
				return strdup(u->name);
		}
		search_state++;
		list_index = 0;
	}

	//global variables
	if (search_state == 2) {
		for (i=list_index; i<vec->size; ++i) {
			v = vec->a[i];

			if (V_NAME(v) != NULL) {
				int n_len = strlen(V_NAME(v));

				if (!strncmp(V_NAME(v), text, len)) {
					list_index = i+1;
					return strdup(V_NAME(v));
				}

				if (!strncmp(V_NAME(v), text, strlen(V_NAME(v))) && text[n_len] == '.') {
					//
					// NOTE(rswinkle): I'm manually doing what rl_completion_matches does
					// here.  I used to try to logic it out in dv_complete_func and call
					// rl_completion_matches with member_generator directly but it wasn't
					// ideal and even though this duplicates some work (refinding the top level
					// structure) I think it's better
					char* ret = member_generator(text, mem_state);
					mem_state = 1;
					
					if (!ret) {
						list_index = i+1;
						mem_state = 0;
						continue;
					}
					return ret;
				}
			}
		}
	}

	// If no names matched, then return NULL.
	return NULL;
}

char* member_generator(const char* text, int state)
{
	static int list_index, len;
	static Var* v;
	static char* dot;

	Var* member;
	char* name;

	Scope* scope = global_scope();
	cvector_varptr* vec = &scope->symtab;

	char* word = NULL;

	word = text;

	char* tmp;
	int nlen;
	int i, n_memb;

	if (!state) {
		len = strlen(word);
		list_index = 0;
		v = NULL;
		dot = word;
	}
	char buf[256];

	if (!v) {
		tmp = strchr(word, '.');
		nlen = tmp - dot;
		dot = tmp;

		for (i=0; i<vec->size; ++i) {
			v = vec->a[i];
			if (V_TYPE(v) != ID_STRUCT)
				continue;

			if (V_NAME(v) && !strncmp(V_NAME(v), word, nlen)) {
				break;
			}
		}

		if (i == vec->size)
			return NULL;

		while ((tmp = strchr(&dot[1], '.'))) {
			memcpy(buf, &dot[1], tmp-dot-1);
			buf[tmp-dot-1] = 0;
			i = Narray_find(V_STRUCT(v), buf, (void**)&member);
			if (i == -1 || V_TYPE(member) != ID_STRUCT)
				return NULL;
			
			v = member;
			dot = tmp;
		}
	}

	nlen = len - (dot - word) - 1;
	char* result = NULL;
	n_memb = get_struct_count(v);
	for (int j=list_index; j<n_memb; ++j) {
		get_struct_element(v, j, &name, &member);
		if (name && !strncmp(name, &dot[1], nlen)) {

			list_index = j+1;

			//comment out these 4 lines if '.' is a breaking character
			result = malloc(dot-word+1 + strlen(name)+1);
			memcpy(result, word, dot-word+1);
			strcpy(&result[dot-word+1], name);
			return result;

			return strdup(name);
		}
	}

	return NULL;
}

/*
 * NOTE(rswinkle): Not currently used
char* global_var_generator(const char* text, int state)
{
	static int list_index, len;

	Scope* scope = global_scope();
	cvector_varptr* vec = &scope->symtab;
	Var* v;

	if (!state) {
		len = strlen(text);
		list_index = 0;
	}

	int i;
	for (i=0; i<vec->size; ++i) {
		if (i < list_index)
			continue;

		v = vec->a[i];
		if (V_NAME(v) != NULL && !strncmp(V_NAME(v), text, len)) {
			list_index = i+1;
			return strdup(V_NAME(v));
		}
	}
	return NULL;
}
*/




/* Attempt to complete on the contents of TEXT.  START and END bound the
	 region of rl_line_buffer that contains the word to complete.  TEXT is
	 the word to complete.  We can use the entire contents of rl_line_buffer
	 in case we want to do some simple parsing.  Return the array of matches,
	 or NULL if there aren't any. */
char** dv_complete_func(const char* text, int start, int end)
{
	char** matches = NULL;

	rl_completion_append_character = '\0';

	static const char* qb = "\'\"";

	if (start == 0 || (rl_line_buffer[start-1] != qb[0] && rl_line_buffer[start-1] != qb[1])) {
		matches = rl_completion_matches(text, command_generator);
	}

	return matches;
}

#else

void add_history() {}
char* readline(char* prompt)
{
	char buf[2048];
	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(buf, sizeof(buf) - 1, stdin) != NULL) {
		return (strdup(buf));
	} else {
		return (NULL);
	}
}
#endif

const char* usage_str =
	"usage: %s [-Viwq] [-v#] [-l logfile] [-e cmd] [-f script] args\n"
	" Options:\n"
	"    -V            dump version information\n"
	"    -i            force interactive mode\n"
	"    -w            don't use X windows\n"
	"    -q            quick startup.  Don't load history or .dvrc\n"
	"    -H            force loadig of history, even in quick mode\n"
	"    -h            print this help\n"
	"    -l logfile    use logfile for loading/saving history instead of ./.dvrc\n"
	"    -e cmd        execute the specified command and exit\n"
	"    -f script     exectue the specified script and exit\n"
	"    --            indicates this is the last command line option\n"
	""
	"  Note: Any --option style options are always passed as $ARGV values\n";

int usage(char* prog)
{
	fprintf(stderr, usage_str, prog);
	return (1);
}
