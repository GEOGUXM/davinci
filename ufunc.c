#include "ff_source.h"
#include "func.h"
#include "parser.h"
#include <string.h>

// Load function from file.
// Find and verify name.
// Find split and verify args.

int nufunc         = 0;

avl_tree_t ufuncs_avl;

void list_funcs();

void* get_current_buffer();
void* yy_scan_string();
void yy_delete_buffer(void*);
void yy_switch_to_buffer(void*);

void init_ufunc_tree()
{
	avl_init(&ufuncs_avl, NULL);
}

void free_ufunc_tree()
{
	avl_node_t* next;
	avl_node_t* cur = avl_head(&ufuncs_avl);
	while (cur) {
		next = avl_next(cur);
		free_ufunc(avl_ref(cur, UFUNC, node));
		cur = next;
	}
}

static int avl_cmp_ufunc(const avl_node_t* lhs, const avl_node_t* rhs, const void* aux)
{
	const UFUNC* a = avl_ref(lhs, UFUNC, node);
	const UFUNC* b = avl_ref(rhs, UFUNC, node);
	return strcmp(a->name, b->name);
}


UFUNC* locate_ufunc(char* name)
{
	UFUNC tmp;
	tmp.name = name;
	avl_node_t* found = avl_search(&ufuncs_avl, &tmp.node, avl_cmp_ufunc);
	if (found) {
		return avl_ref(found, UFUNC, node);
	}

	return NULL;
}

int destroy_ufunc(char* name)
{
	UFUNC tmp;
	tmp.name = name;
	avl_node_t* found = avl_search(&ufuncs_avl, &tmp.node, avl_cmp_ufunc);
	if (found) {
		avl_remove(&ufuncs_avl, found);
		UFUNC* u = avl_ref(found, UFUNC, node);
		free_ufunc(u);
		--nufunc;
		return 1;
	}

	return 0;
}

void store_ufunc(UFUNC* f)
{
	avl_insert(&ufuncs_avl, &f->node, avl_cmp_ufunc);
	++nufunc;
}

void free_ufunc(UFUNC* f)
{
	free(f->text);
	free(f->name);
	free(f->fname);

	if (f->argbuf) free(f->argbuf);
	if (f->args) free(f->args);
	if (f->tree) free_tree(f->tree);
	free(f);
}

void save_ufunc(char* filename)
{
	UFUNC* f;

	f = load_function(filename);
	if (f == NULL) return;
	/**
	 ** If a ufunc with this name exists, destroy it
	 **/
	if (destroy_ufunc(f->name)) {
		if (VERBOSE) fprintf(stderr, "Replacing function %s\n", f->name);
	} else {
		if (VERBOSE) fprintf(stderr, "Loaded function %s\n", f->name);
	}
	store_ufunc(f);
}

UFUNC* load_function(char* filename)
{
	// locate and verify important portions of function definition
	int i, j;
	struct stat sbuf;
	char *buf, *str, *p;
	int nlen = 0;
	char name[256];
	char line[2048];
	char* q;

	UFUNC *f = NULL, *f2 = NULL;
	void* handle;
	extern char* yytext;
	extern Var* curnode;
	FILE* fp;
	void* parent_buffer;
	extern int local_line;
	extern char* pp_str;

	char* fname;

	/**
	 ** Get text from file
	 **/
	if (stat(filename, &sbuf) != 0) {
		fprintf(stderr, "Internal error: load_function, no file\n");
		return NULL;
	}
	buf = (char*)calloc(1, sbuf.st_size + 1);
	fp  = fopen(filename, "rb");
	fread(buf, sbuf.st_size, 1, fp);
	fclose(fp);

	unlink(filename);

	buf[sbuf.st_size] = '\0';

	str = buf;

	while (isspace(*str)) str++;
	if (strncasecmp(str, "define", 6)) {
		free(buf);
		fprintf(stderr, "Internal error: load_function, no 'define' in file\n");
		return NULL;
	}
	str += 6;
	while (*str && isspace(*str)) str++;
	while (*str && (isalnum(*str) || *str == '_')) name[nlen++] = *str++;
	if (nlen == 0) {
		/**
		 ** If we want to be nice to the user, we could spit out all
		 ** the ufunc's here.
		 **/
		free(buf);

		list_funcs();

		return NULL;
	}
	name[nlen] = '\0';
	if (!(isalpha(*name) || *name == '_')) {
		parse_error("error: Function name must begin with a letter: %s\n", name);
		free(buf);
		return NULL;
	}

	f        = (UFUNC*)calloc(1, sizeof(UFUNC));
	f->name  = strdup(name);
	f->text  = buf;
	f->ready = 0;

	fname = top_input_filename();
	if (!fname) {
		fname = (char*)":no filename:";
	}

	f->fname = strdup(fname);
	f->fline = local_line + 1;

	// See if the function we are replacing is exactly the same.
	// If so, do nothing.
	if ((f2 = locate_ufunc(f->name)) != NULL) {
		if (!strcmp(f->text, f2->text)) {
			/*
			** Functions are identical, move on.
			*/
			free_ufunc(f);
			return NULL;
		}
	}

	/**
	 ** should now find '( args )'.
	 ** args should be limited to ids, space and numbers
	 **/
	while (*str && isspace(*str)) str++;

	if (*str && *str == '(') {
		str++;
		p = str;
		while (*str && (isalnum(*str) || isspace(*str) || strchr(",_", *str))) str++;
		if (*str && *str == ')') {
			/**
			 ** Believe we found a complete args section.  Parse 'em up.
			 ** Duplicate the string for cutting on.
			 **/
			if (p != str) {
				f->argbuf = strndup((char*)p, str - p);
				split_string(f->argbuf, &f->nargs, &f->args, ",");
			}
			/**
			 ** verify arg limits
			 **/
			f->min_args = -1;
			f->max_args = -1;
			if (f->nargs) {
				if (strspn(f->args[f->nargs - 1], "0123456789")) {
					f->min_args = atoi(f->args[f->nargs - 1]);
					f->nargs--;
					if (f->nargs && strspn(f->args[f->nargs - 1], "0123456789")) {
						f->max_args = f->min_args;
						f->min_args = atoi(f->args[f->nargs - 1]);
						f->nargs--;
					}
				}
				if (f->max_args != -1 && f->min_args > f->max_args) {
					parse_error("min_args > max_args.\n");
					return f;
				}
			}
			/**
			 ** Verify arg names.
			 **/
			for (i = 0; i < f->nargs; i++) {
				if (!(isalpha(f->args[i][0]))) {
					parse_error("error: Illegal argument name: %s\n", f->args[i]);
					return f;
				}
				for (j = 0; j < strlen(f->args[i]); j++) {
					if (!(isalnum(f->args[i][j]) || f->args[i][j] == '_')) {
						parse_error("error: Illegal argument name: %s\n", f->args[i]);
						return f;
					}
				}
			}
		} else {
			/**
			 ** Next character is not ')'.  Panic.
			 **/
			parse_error("error: function %s, bad args.\n", f->name);
			return f;
		}
	} else {
		/**
		 ** If we wanted to be nice to the user, we could spit out the
		 ** contents of the function named in f->name here.
		 **/
		parse_error("error: loading function %s, no args.\n", f->name);
		return f;
	}
	str++;

	/**
	 ** Presumably, everything else is body.
	 **/
	if (*str) f->body = str;

	/**
	 ** Shove the function into the stream for evaluation.
	 **/
	if (debug) {
		p = f->text;
		q = f->body;
		memcpy(line, p, q - p + 1);
		line[q - p + 1] = '\0';
		printf("%s", line);
		fflush(stdout);
	}

	pp_line = local_line;
	p       = str;

	parent_buffer = (void*)get_current_buffer();

	while (p && *p) {
		q                = strchr(p, '\n');
		if (q == NULL) q = p + strlen(p) - 1;
		memcpy(line, p, q - p + 1);
		line[q - p + 1] = '\0';
		if (debug) {
			printf("%s", line);
			fflush(stdout);
		}
		handle = (void*)yy_scan_string(line);
		pp_str = line;
		pp_line++;
		while ((i = yylex()) != 0) {
			j = yyparse(i, (Var*)yytext);

			if (j == 1) {
				f->tree = curnode;
				break;
			}
		}
		yy_delete_buffer((struct yy_buffer_state*)handle);
		p = q + 1;
	}
	yy_switch_to_buffer((struct yy_buffer_state*)parent_buffer);
	f->ready = 1;
	pp_str   = NULL;

	/*
	** Take one off  because save_ufunc put one on.
	*/
	pp_line--;

	return f;
}

/**
 ** dispatch_ufunc - dispatch a ufunc.
 **
 ** Start a new scope.
 ** Initialize all the functions named args to NOT_PRESENT.
 ** Scan args for keyword pairs.  Pull em off and store 'em in scope
 ** Init $N variables.
 ** Send function text to parser.
 **
 **
 ** Scope should include a symtab pointer to hold memory allocated
 ** while in this scope.  To be deallocated on exit.
 **/
Var* dispatch_ufunc(UFUNC* f, Var* arg)
{
	int i, argc, j;
	Var *v, *p, *e;
	int insert = 0;
	int ac     = 0;


	Scope s;
	init_scope(&s);
	Scope* scope = &s;


	/**
	 ** Create identifiers for all the named arguments.  These don't
	 ** yet have pointers to data, indicating they're NOT_PRESENT.
	 **/
	for (i = 0; i < f->nargs; i++) {
		dd_put(scope, f->args[i], NULL);
	}
	/**
	 ** Parse through args looking for keyword pairs, and storing their
	 ** values.  While we are at it, if we encounter a value without a
	 ** keyword, store it in ARGV
	 **
	 ** Total number of args is stored in $0
	 **
	 **** In the future, we might be able to use the V_ARGS(narray) as
	 **** a new scope, directly
	 **/

	if (arg) {
		ac = Narray_count(V_ARGS(arg));
	}
	for (j = 0; j < ac; j++) {
		Narray_get(V_ARGS(arg), j, NULL, (void**)&p);

		if (V_TYPE(p) == ID_KEYWORD) {
			for (i = 0; i < f->nargs; i++) {
				if (!strcmp(f->args[i], V_NAME(p))) {
					break;
				}
			}
			if (i == f->nargs) {
				parse_error("error: Unknown keyword to ufunc: %s(... %s= ...)\n", f->name, V_NAME(p));
				free_scope(scope);
				return NULL;
			} else {
				v = V_KEYVAL(p);
				if ((e = eval(v)) != NULL) v = e;
				dd_put(scope, V_NAME(p), v);
			}
		} else {
			v = p;
			if ((e = eval(v)) != NULL) v = e;
			argc                         = dd_put_argv(scope, v);

			if (f->max_args >= 0 && argc > f->max_args) {
				parse_error("error: Too many arguments to ufunc: %s().  Only expecting %d\n",
				            f->name, f->max_args);
				dd_unput_argv(scope);
				free_scope(scope);
				return NULL;
			}
		}
	}
	if (f->min_args && (argc = dd_argc(scope)) < f->min_args) {
		parse_error("error: Not enough arguments to ufunc: %s().  Expecting %d.\n", f->name, f->min_args);
		dd_unput_argv(scope);
		free_scope(scope);
		return NULL;
	}
	/**
	 ** Okay, now we have dealt with all the args.
	 ** Push this scope into the scope stack, and run the function.
	 **/
	scope->ufunc = f;
	scope_push(scope);

	//save location in vector
	int loc = scope_stack_count()-1;
	evaluate(f->tree);

	//retrieve in case vector was realloced and because the scope
	//on the scope_stack has changed from the scope that was pushed
	scope = scope_stack_get(loc);

	/**
	 **  Additionally, we need to transfer this value OUT of this scope's
	 **  symtab, since that memory will go away when the scope is cleaned.
	 **/
	//NOTE(rswinkle): this literally does nothing, 0 side effects, see scope.c
	//it's been that way for a decade at least
	dd_unput_argv(scope);

	/**
	 ** options the user may exercise with the RETURN statement:
	 **
	 ** return($1)  - scope doesn't own this memory.
	 ** return(arg) - scope doesn't own this memory either.
	 ** return(sym) - must get this value out of the local symtab, to keep
	 **                   it from being free'd when the scope exits.  Stuff
	 **                   it into the parent's tmptab to be claimed or free'd
	 ** return(tmp) - must get this value out of the local tmptab, to keep
	 **                   it from being free'd when the scope exits.  Stuff
	 **                   it into the parent't tmptab to be claimed or free'd
	 **
	 ** when we promote a value up to the parent's scope, if the value had
	 ** a name, we need to get rid of it.
	 **/
	if ((v = scope->rval) != NULL) {
		/**
		*** DANGER!
		***
		*** If v is an element from a structure, then this test will fail.
		*** v can neither be memclaimed, nor is it in the local symtab.
		**/
		if (mem_claim(v) != NULL || rm_symtab(v) != NULL) {
			insert = 1;
		}
	}
	scope_pop();

	if (insert) {
		scope = scope_tos();

		if (V_NAME(v) != NULL) {
			free(V_NAME(v));
			V_NAME(v) = NULL;
		}

		/*
		if (!scope->tmp.capacity || !scope->tmp.a) {
			printf("%p %p %zu %zu\n", &scope->tmp, scope->tmp.a, scope->tmp.size, scope->tmp.capacity);
		}
		*/
		cvec_push_varptr(&scope->tmp, &v);
	}
	return v;
}

/**
 ** find ufunc.
 ** Split to file.
 ** Call editor.
 ** Check file time, and reload if newer.
 **/

Var* ufunc_edit(vfuncptr func, Var* arg)
{
	UFUNC* ufunc;
	char* name;
	struct stat sbuf;
	time_t time = 0;
	FILE* fp;
	char buf[256];
	char *fname, *filename, *editor;
	int temp = 0;
	Var* function;

	/*
	** This function does not call parse_args, as it uses a
	** polymorphic argument which can be an unknown enumeration
	** (ie: a function name, unquoted)
	*/

	if (arg == NULL) return NULL;
	Narray_get(V_ARGS(arg), 0, NULL, (void**)&function);

	if (V_TYPE(function) == ID_STRING) {
		filename = V_STRING(function);
		if ((fname = dv_locate_file(filename)) == NULL) {
			fname = filename;
		}
	} else if (V_TYPE(function) == ID_VAL) {
		/**
		 ** Numeric arg, call hedit to do history editing()
		 **/
		return ff_hedit(func, arg);
	} else {
		if ((name = V_NAME(function)) == NULL) {
			return NULL;
		}

		if ((ufunc = locate_ufunc(name)) == NULL) return NULL;

		fname = make_temp_file_path();
		if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
			parse_error("%s: unable to open temp file", func->name);
			if (fname) free(fname);
			return NULL;
		}
		fprintf(fp, "# Original location: %s:%d\n", ufunc->fname, ufunc->fline);
		fputs(ufunc->text, fp);
		fclose(fp);
		temp = 1;
	}

	if (stat(fname, &sbuf) == 0) {
		time = sbuf.st_mtime;
	}

	if ((editor = getenv("EDITOR")) == NULL) editor = (char*)"/bin/vi";

	sprintf(buf, "%s %s", editor, fname);
	system(buf);

	if (stat(fname, &sbuf) == 0) {
		if (time != sbuf.st_mtime) {
			fp = fopen(fname, "r");
			push_input_stream(fp, (char*)":ufunc edit:");
		} else {
			fprintf(stderr, "File not changed.\n");
		}
	}
	if (temp) {
		unlink(fname);
		free(fname);
	} else if (filename && fname != filename) {
		free(fname);
	}

	return NULL;
}

void list_funcs()
{
	avl_node_t *cur = avl_head(&ufuncs_avl);
	while (cur) {
		UFUNC* u = avl_ref(cur, UFUNC, node);
		printf("%s\n", u->name);
		cur = avl_next(cur);
	}
}

Var* ff_global(vfuncptr func, Var* arg)
{
	Var *e = NULL, *v = NULL;
	char* aname = NULL;
	Alist alist[2];

	alist[0]      = make_alist("object", ID_ENUM, NULL, &aname);
	alist[1].name = NULL;

	if (parse_args(func, arg, alist) == 0) return NULL;

	if (aname == NULL) {
		return NULL;
	}

	if ((e = get_global_sym(aname)) == NULL) {
		/*
		 * Doesn't exist in the global scope.
		 * If it exists in the local scope, get it and give it's memory
		 * to the global scope.  Otherwise, create a new value.
		 */
		if ((e = get_sym(aname)) == NULL) {
			char* zero = (char*)calloc(1, 1);
			e          = newVal(BSQ, 1, 1, 1, DV_UINT8, zero);
			mem_claim(e);
			V_NAME(e) = strdup(aname);
			//printf("path 1\n");
		} else if ((v = rm_symtab(e)) != NULL) {
			e = v;
			//fprintf(stderr, "path 2\n");
		} else {
			/* Ok, at this point, this named argument exists in the
			 * local scope, but it has to be in the dd, not the symtab.
			 * The means it's owned by a parent's scope.  Lets just quit.
			 */
			parse_error("error: symbol is not part of the local scope");
			return NULL;
		}
		put_global_sym(e);
	}
	dd_put(scope_tos(), V_NAME(e), e);
	return NULL;
}
