#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "util.h"

#define DEBUG 0

#define MAXTOKENLEN 32

const char *TRUE = "#t";
const char *FALSE = "#f";

enum exprtype { EXPRPROC, EXPRSYM, EXPRINT, EXPRLAMBDA, EXPRLIST, EXPREMPTY };
typedef struct expr {
	union {
		long int intvalue;
		char symvalue[MAXTOKENLEN];
		struct expr *listptr;
		struct {
			struct expr *lambdavars;
			struct expr *lambdaexpr;
		};
		struct expr *(*proc) (struct expr *);
	};
	enum exprtype type;
	struct expr *next;
	bool in_use;		/* For the garbage collection. */
} expr;

typedef struct dictentry {
	expr *sym;
	expr *value;
	struct dictentry *next;
} dictentry;

typedef struct env {
	dictentry *list;
	struct env *outer;
	bool in_use;		/* For the garbage collection. */
} env;

static env *global_env;
static env *current_env;

typedef struct expr_list {
	expr *exprptr;
	struct expr_list *next;
} expr_list;

/* This stores every expression created with `create_expr'. */
static expr_list *saved_expressions;

typedef struct env_list {
	env *envptr;
	struct env_list *next;
} env_list;

/* This stores every environment created with `create_env'. */
static env_list *saved_environments;

void print_expr(expr *);

/*
 * Frees an environment structure as well as the enclosed dictionary.
 * Params:
 *   e : the env struct to freed.
 * Returns:
 *   the total size of bytes freed.
 */
size_t free_env(env * e)
{
	size_t byte_count = 0;
	dictentry *dictptr = e->list;
	dictentry *tmp;
	while (dictptr != NULL) {
		tmp = dictptr->next;
		free(dictptr);
		dictptr = tmp;
		byte_count += sizeof(dictentry);
	}
	free(e);
	return byte_count += sizeof(env);
}

/*
 * Save an expression pointer for the garbage collection.
 * Params:
 *   e : the expression pointer to be saved.
 * Returns:
 *   the pointer to the expr_list entry where e was stored.
 */
expr_list *gc_collect_expr(expr * e)
{
	if (e == NULL)
		return NULL;

	expr_list *tmp = malloc(sizeof(expr_list));
	tmp->exprptr = e;
	tmp->next = NULL;

	debug_info("Collecting expression pointer %p.\n", e);

	if (saved_expressions == NULL)
		return saved_expressions = tmp;

	expr_list *listptr = saved_expressions;
	while (listptr->next != NULL) {
		if (listptr->exprptr == e) {
			free(tmp);
			return listptr;
		}
		listptr = listptr->next;
	}
	return listptr->next = tmp;
}

/*
 * Save an environment pointer for the garbate collection.
 * Params:
 *   e : the environment pointer to be saved.
 * Returns:
 *  the pointer to the env_list entry where e was stored;
 * TODO: Remove redundancy with gc_collect_expr; somehow...
 */
env_list *gc_collect_env(env * e)
{
	if (e == NULL)
		return NULL;

	env_list *tmp = malloc(sizeof(env_list));
	tmp->envptr = e;
	tmp->next = NULL;

	debug_info("Collecting environment pointer %p.\n", e);

	if (saved_environments == NULL)
		return saved_environments = tmp;

	env_list *listptr = saved_environments;
	while (listptr->next != NULL) {
		if (listptr->envptr == e)
			return listptr;
		listptr = listptr->next;
	}
	return listptr->next = tmp;
}

/*
 * Mark an expression recursively as in_use. This includes
 * every subexpression if it's an expression list.
 * Param:
 *   e : a pointer to the expression which should be marked.
 */
void gc_mark_expr(expr * e)
{
	if (e == NULL || e->in_use == true)
		return;

	e->in_use = true;

	if (e->type == EXPRLIST) {
		expr *listentry = e->listptr;
		while (listentry != NULL) {
			gc_mark_expr(listentry);
			listentry = listentry->next;
		}
	}
}

/*
 * Runs the garbage collection. This is a simple mark-and-sweep
 * garbage collector. First, the in_use flag for every saved expression
 * and environment is set to false and then we traverse the
 * environment (starting with current_env) and mark every found
 * expression and environment as in_use.
 * Afterwards, every struct which is not in_use will be freed as well as
 * the corresponding entries in the saved_expressions and
 * saved_environments lists.
 *
 * IMPORTANT: Don't use `free()' on expr and env pointers anywhere else
 * in this program.
 *
 * Params:
 *   unused : for compatibility with the other Scheme procedures.
 * Returns:
 *   NULL; for compatibility
 */
expr *gc(expr * unused)
{
	printf("Running garbage collection...\n");

	if (saved_expressions == NULL || saved_environments == NULL)
		return NULL;

	/* MARK */

	expr_list *exprlistptr = saved_expressions;
	env_list *envlistptr = saved_environments;

	int max_exprs = 0;
	int max_envs = 0;

	/* Mark all exprs and envs as unused. */
	do {
		exprlistptr->exprptr->in_use = false;
		max_exprs++;
	} while ((exprlistptr = exprlistptr->next) != NULL);
	do {
		envlistptr->envptr->in_use = false;
		max_envs++;
	} while ((envlistptr = envlistptr->next) != NULL);

	/* Find all used environments and used expressions. */
	env *envptr = current_env;
	dictentry *dictptr;
	while (envptr != NULL) {
		dictptr = envptr->list;
		while (dictptr != NULL) {
			gc_mark_expr(dictptr->sym);
			gc_mark_expr(dictptr->value);
			dictptr = dictptr->next;
		}

		envptr->in_use = true;
		envptr = envptr->outer;
	}

	/* SWEEP */

	int count_expr = 0, count_env = 0, byte_count_env = 0;
	exprlistptr = saved_expressions;
	envlistptr = saved_environments;

	expr_list *tmp_expr;
	env_list *tmp_env;

	expr_list *prev_expr = NULL;
	env_list *prev_env = NULL;

	/* Free each unused expr and env. */
	while (exprlistptr != NULL) {
		if (!exprlistptr->exprptr->in_use) {
			/* Free the expr if it's  not in_use: */
			free(exprlistptr->exprptr);
			/* Free the corresponding struct in the list: */
			tmp_expr = exprlistptr->next;
			free(exprlistptr);
			/* Update the next-pointer of the previous expr: */
			if (prev_expr == NULL) {
				saved_expressions = tmp_expr;
				prev_expr = saved_expressions;
			} else {
				prev_expr->next = tmp_expr;
			}
			exprlistptr = tmp_expr;
			count_expr++;
		} else {
			exprlistptr = exprlistptr->next;
			if (prev_expr == NULL)
				prev_expr = saved_expressions;
			else
				prev_expr = prev_expr->next;
		}
	}
	while (envlistptr != NULL) {
		if (!envlistptr->envptr->in_use) {
			byte_count_env += free_env(envlistptr->envptr);
			tmp_env = envlistptr->next;
			free(envlistptr);
			if (prev_env == NULL) {
				saved_environments = tmp_env;
				prev_env = saved_environments;
			} else
				prev_env->next = tmp_env;
			envlistptr = tmp_env;
			count_env++;
		} else {
			envlistptr = envlistptr->next;
			if (prev_env == NULL)
				prev_env = saved_environments;
			else
				prev_env = prev_env->next;
		}
	}

	printf("Garbage collection done.\n");
	printf
	    ("Freed %d/%d expressions (%d bytes).\n", count_expr, max_exprs,
	     count_expr * sizeof(expr));
	printf("Freed %d/%d environments (%d bytes).\n", count_env, max_envs,
	       byte_count_env);
	return NULL;
}

expr *find_in_dict(expr * e, env * en)
{
	if (e == NULL || en == NULL)
		return NULL;
	dictentry *d = en->list;
	while (d != NULL) {
		if (strcmp(e->symvalue, d->sym->symvalue) == 0)
			return d->value;
		d = d->next;
	}
	return find_in_dict(e, en->outer);
}

/*
 * Returns the i-th entry of an expr list.
 * Params:
 *   i : the number of the requested entry. 0 is the list head.
 * Returns:
 *   a pointer to the list entry i or NULL if none was found.
 */
expr *get_next(expr * e, int i)
{
	if (e == NULL || e->type != EXPRLIST || i < 0)
		return NULL;

	int counter = 0;
	expr *current = e->listptr;
	while (current != NULL) {
		if (counter == i)
			return current;
		current = current->next;
		counter++;
	}

	return NULL;
}

/*
 * Return the length of an expression list.
 * Params:
 *   e : An expr struct pointer. This should be an expression list.
 * Returns:
 *   the length of the list or -1 if e is not an expression list.
 */
int get_list_size(expr * e)
{
	if (e == NULL) {
		print_err("%s", "Argument e is NULL.\n");
		return -1;
	}
	if (e->type != EXPRLIST) {
		print_err("%s", "Argument e is not an expression list.\n");
		return -1;
	}
	int counter = 0;
	expr *current_list_entry = e->listptr;
	while (current_list_entry != NULL) {
		current_list_entry = current_list_entry->next;
		counter++;
	}
	return counter;
}

void _print_expr(expr * e, bool verbose)
{
	if (e == NULL) {
		print_err("%s", "Argument e is NULL.");
	} else if (e->type == EXPRLIST) {
		printf("%s", verbose ? " EXPRLIST[" : "(");
		expr *t = e->listptr;
		while (t != NULL) {
			_print_expr(t, verbose);
			t = t->next;
		}
		printf("%s", verbose ? "] " : ")");
	} else if (e->type == EXPREMPTY) {
		printf("%s", verbose ? "()" : " [] ");
	} else if (e->type == EXPRINT) {
		if (verbose)
			printf(" INT: %ld ", e->intvalue);
		else
			printf("%ld", e->intvalue);
	} else if (e->type == EXPRPROC) {
		printf(" PROC: %p ", e->proc);
	} else if (e->type == EXPRLAMBDA) {
		printf("[LAMBDA EXPR ARGS:");
		_print_expr(e->lambdavars, verbose);
		printf(" BODY ");
		_print_expr(e->lambdaexpr, verbose);
		printf("]");
	} else {
		printf(" SYM:'%s' ", e->symvalue);
	}
}

void print_expr(expr * e)
{
	_print_expr(e, true);
	printf("\n");
}

void quote_expr(expr * e)
{
	_print_expr(e, false);
	printf("\n");
}

void print_expr_debug(expr * e)
{
	if (DEBUG)
		print_expr(e);
}

void add_to_exprlist(expr * list, expr * new)
{
	if (list == 0)
		return;
	else if (list->listptr == 0)
		list->listptr = new;
	else {
		expr *t = list->listptr;
		while (t->next != 0) {
			t = t->next;
		}
		t->next = new;
	}
}

static env *create_env(env * outer, dictentry * list)
{
	env *new = malloc(sizeof(env));
	new->outer = outer;
	new->list = list;

	gc_collect_env(new);

	return new;
}

static expr *create_expr(enum exprtype type)
{
	expr *new = malloc(sizeof(expr));
	new->type = type;
	new->next = NULL;
	memset(new->symvalue, 0, sizeof(new->symvalue));

	gc_collect_expr(new);

	return new;
}

static expr *create_exprempty()
{
	return create_expr(EXPREMPTY);
}

static expr *create_exprproc(struct expr *(*proc) (struct expr *))
{
	expr *new = create_expr(EXPRPROC);
	new->proc = proc;

	return new;
}

expr *create_exprsym(const char *s)
{
	expr *new = create_expr(EXPRSYM);

	strncat(new->symvalue, s, MAXTOKENLEN);

	return new;
}

expr *create_exprint(long int i)
{
	expr *new = create_expr(EXPRINT);
	new->intvalue = i;

	return new;
}

expr *deep_copy(expr * e)
{
	if (e == NULL)
		return NULL;

	expr *new = create_expr(EXPRSYM);
	memcpy(new, e, sizeof(expr));

	if (new->type != EXPRLIST)
		return new;

	expr *listptr_orig = e->listptr;

	new->listptr = deep_copy(listptr_orig);
	expr *listptr_new = new->listptr;

	while ((listptr_orig = listptr_orig->next) != NULL) {
		listptr_new->next = deep_copy(listptr_orig);
		listptr_new = listptr_new->next;
	}

	return new;
}

/*
 * Add a key-value pair to an environment.
 * Params:
 *   env : An environment struct pointer. Must be non-NULL.
 *   sym : New key to be inserted. Must be non-NULL.
 *   value : Value to be inserted/updated. Must be non-NULL.
 *   set : true if value should be set globally.
 * Returns:
 *   The updated dict entry or NULL if there was an error.
 */
dictentry *add_to_env(env * env, expr * sym, expr * value, bool set)
{
	if (env == NULL || sym == NULL || value == NULL)
		return NULL;

	dictentry *current_dict_entry = env->list;

	/* Add new list head if the dictionary is emtpy. */
	if (current_dict_entry == NULL) {
		if (env->outer == NULL && set) {
			print_err("Error. Variable '%s' not defined.\n",
				  sym->symvalue);
			return NULL;
		} else if (set) {
			return add_to_env(env->outer, sym, value, set);
		}
		current_dict_entry = malloc(sizeof(dictentry));
		current_dict_entry->sym = sym;
		current_dict_entry->value = value;
		current_dict_entry->next = NULL;
		env->list = current_dict_entry;
		return current_dict_entry;
	}

	/* Search dictionary for the last entry or an existing key. */
	while (current_dict_entry->next != NULL) {
		/* Break if an exising key was found. */
		if (current_dict_entry->next->sym->type == EXPRSYM
		    && strcmp(current_dict_entry->next->sym->symvalue,
			      sym->symvalue) == 0) {
			break;
		}
		current_dict_entry = current_dict_entry->next;
	}

	/* Add new last entry. */
	if (current_dict_entry->next == NULL) {
		if (env->outer != NULL && set) {
			return add_to_env(env->outer, sym, value, set);
		} else if (set) {
			print_err("Variable '%s' not defined.\n",
				  sym->symvalue);
			return NULL;
		}
		current_dict_entry->next = malloc(sizeof(dictentry));
		current_dict_entry->next->next = NULL;
	}

	/* Update new entry. */
	current_dict_entry->next->sym = sym;
	current_dict_entry->next->value = value;

	return current_dict_entry->next;
}

expr *eval(expr *, env *);

expr *evalList(expr * e, env * en)
{
	if (e->type != EXPRLIST) {
		print_err("%s", "Argument e is not a list.\n");
		exit(-1);
	}
	expr *te = e->listptr;
	expr *prev = NULL;
	while (te != 0) {
		expr *savednext = te->next;
		te = eval(te, en);
		if (te == NULL) {
			debug_info("%s", "Removing empty entry from list.\n");
		} else {
			if (prev == NULL) {
				e->listptr = te;
			} else {
				prev->next = te;
				te->next = 0;
			}
			prev = te;
			te->next = savednext;
		}
		te = savednext;
	}
	return prev;

}

expr *eval(expr * e, env * en)
{
	debug_info("%s", "eval called with");
	print_expr_debug(e);

	/* Store the current environment for the garbage collection. */
	current_env = en;

	if (e->type == EXPRSYM) {
		expr *res = find_in_dict(e, en);
		if (res == NULL) {
			print_err("Variable not defined here: %s.\n",
				  e->symvalue);
			exit(-1);
		}
		expr *copy = create_expr(EXPRSYM);
		memcpy(copy, res, sizeof(expr));
		return copy;
	}
	if (e->type != EXPRLIST) {
		return e;
	}
	if (e->listptr == NULL) {
		print_err("%s", "Empty list (probably...)\n");
		exit(-1);
	}
	/* DEFINE */
	if (e->listptr->type == EXPRSYM
	    && (strcmp(e->listptr->symvalue, "define") == 0
		|| strcmp(e->listptr->symvalue, "set!") == 0)) {
		int size = get_list_size(e);
		if (size != 3) {
			print_err
			    ("Wrong number of arguments for 'define'/'set!': %d\n",
			     size);
			exit(-1);
		}
		expr *key = get_next(e, 1);
		expr *value = get_next(e, 2);
		if (key->type != EXPRSYM) {
			print_err
			    ("%s",
			     "Argument 1 for 'define'/'set!' is not a symbol.\n");
			exit(-1);
		}
		value = eval(value, en);
		if (add_to_env
		    (en, key, value,
		     strcmp(e->listptr->symvalue, "set!") == 0) == NULL) {
			print_err("Could not define/set %s.\n", key->symvalue);
			exit(-1);
		}
		debug_info("Defined value %s.\n", key->symvalue);
		return 0;
	}
	/* QUOTE */
	if (e->listptr->type == EXPRSYM
	    && strcmp(e->listptr->symvalue, "quote") == 0) {
		expr *next = e->listptr->next;
		e->listptr = next;
		return e;
	}
	/* IF */
	if (e->listptr->type == EXPRSYM
	    && strcmp(e->listptr->symvalue, "if") == 0) {
		if (get_list_size(e) != 4) {
			print_err
			    ("%s", "Wrong number of arguments for 'if'.\n");
			exit(-1);
		}
		expr *cond = eval(get_next(e, 1), en);
		expr *trueex = get_next(e, 2);
		expr *falseex = get_next(e, 3);
		if (cond->type == EXPRINT)
			return eval(trueex, en);
		else if (cond->type != EXPRSYM) {
			print_err
			    ("%s",
			     "Illegal if condition. Must be Symbol or number\n");
			exit(-1);
		} else if (strcmp(cond->symvalue, TRUE) == 0)
			return eval(trueex, en);
		else if (strcmp(cond->symvalue, FALSE) == 0)
			return eval(falseex, en);
		else {
			print_err("%s", "Wrong Symbol");
			exit(-1);
		}

	}
	if (e->listptr->type == EXPRSYM
	    && strcmp(e->listptr->symvalue, "begin") == 0) {
		e->listptr = e->listptr->next;
		return evalList(e, en);
	}
	/* LAMBDA */
	if (e->listptr->type == EXPRSYM
	    && strcmp(e->listptr->symvalue, "lambda") == 0) {
		expr *args = get_next(e, 1);
		if (args->type != EXPRLIST) {
			print_err
			    ("%s", "First Lambda Parameter must be a list\n");
			exit(-1);
		}
		/* List with only the body */
		e->listptr = args->next;
		expr *lambda = create_expr(EXPRLAMBDA);
		lambda->lambdavars = args;
		lambda->lambdaexpr = e;
		return lambda;

	}
	evalList(e, en);
	if (e->listptr->type == EXPRLAMBDA) {
		env *newenv = create_env(en, NULL);

		int argnum = get_list_size(e->listptr->lambdavars);
		if (argnum != get_list_size(e) - 1) {
			print_err
			    ("Wrong number of arguments for lambda %d required: %d\n",
			     argnum, get_list_size(e) - 1);
			exit(-1);
		}
		expr *args = e->listptr->lambdavars->listptr;
		expr *val = e->listptr->next;
		while (args != NULL) {
			if (args->type != EXPRSYM) {
				print_err
				    ("%s", "Wrong parameter list for lambda\n");
				exit(-1);
			}
			add_to_env(newenv, args, val, false);
			args = args->next;
			val = val->next;
		}
		debug_info("%s", "Evaluate Lambda Expr\n");
		expr *res = 0;
		print_expr_debug(e->listptr->lambdaexpr);
		expr *lambda_new = deep_copy(e->listptr->lambdaexpr);
		if (lambda_new->type == EXPRLIST) {
			debug_info("%s", " as list\n");
			res = evalList(lambda_new, newenv);
		} else {
			debug_info("%s", "as single expr\n");
			expr *res = eval(lambda_new, newenv);
		}
		return res;
	}
	if (e->listptr->type == EXPRPROC) {
		expr *proc = e->listptr;
		/** delete proc from list **/
		e->listptr = e->listptr->next;
		debug_info("%s", "Call proc!\n");
		print_expr_debug(e);
		expr *res = proc->proc(e);
		print_expr_debug(res);
		return res;
	}

	/* We should never arrive here... */
	print_err("%s", "Could not evaluate expression: ");
	print_expr(e);
	exit(-1);
}

expr *read(char *s[])
{
	debug_info("Read called with %s\n", *s);
	char *tptr;
	for (tptr = *s; *tptr != 0 && *tptr == ' '; tptr++) ;
	if (*tptr == 0) {
		print_err("%s", "EOF not expected\n");
		exit(-1);
	}
	int tokenlen;
	char *tmpptr;
	if (*tptr == '(') {
		expr *exprlist = create_expr(EXPRLIST);
		exprlist->listptr = 0;
		tptr++;
		while (*tptr != ')') {
			add_to_exprlist(exprlist, read(&tptr));
			for (; *tptr != 0 && *tptr == ' '; tptr++) ;
		}
		tptr++;
		*s = tptr;
		return exprlist;
	} else if (*tptr == ')') {
		print_err("%s", "')' was not expected here\n");
		exit(-1);
		tokenlen = 1;
	} else {
		/* Create empty expression. */
		if (strncmp(tptr, "'()", 3) == 0) {
			*s = tptr + 3;
			return create_exprempty();
		}

		/* Calculate the token length. */
		for (tokenlen = 0, tmpptr = tptr;
		     *tmpptr != 0 && *tmpptr != ' ' && *tmpptr != '('
		     && *tmpptr != ')'; tokenlen++, tmpptr++) ;

		expr *new;

		char *endptr = NULL;
		long int intval = strtol(tptr, &endptr, 0);
		if (endptr == tptr) {
			if (tokenlen > MAXTOKENLEN) {
				print_err
				    ("Token to long! Maximum token size: %d\n",
				     MAXTOKENLEN);
				exit(-1);
			}
			/* Create a symbol if the int parsing fails: */
			char token[tokenlen + 1];
			token[tokenlen] = 0;
			strncpy(token, tptr, tokenlen);
			new = create_exprsym(token);
		} else {
			new = create_exprint(intval);
		}

		*s = tmpptr;

		return new;
	}
}

/**        LAMBDA PREDEFINED FUNCTIONS: **/

/*
 * Apply a general integer arithmetic function on an expression list.
 * Params:
 *   args : a pointer to an expr struct.
 *          This must be an integer expr or a list of integer exprs.
 *   func : a function pointer to an arithmetic function, e.g. add().
 *   neutral : the neutral element for the arithmeitc operation.
 *             E.g. 0 for add and 1 for mulitplication.
 *   as_bool : true if the resulting expression should be a boolean
 *             symbol expression instead of an integer expression.
 *             If the return value of func is 0, then the bool value
 *             is set to FALSE. Othewise it's TRUE.
 * Returns:
 *   a pointer to an expression struct which equals the evaluation of
 *   the given integer expressions according to func.
 *   Returns NULL if the given expression pointer (args) is not an
 *   EXPRINT or an EXPRLIST of EXPRINTs.
 */
expr *math(expr * args, int (*func) (int, int, bool *), int neutral,
	   bool as_bool)
{
	if (args->type == EXPRINT)
		return args;
	else if (args->type == EXPRLIST) {
		int result = neutral;
		bool b = true;
		expr *arg = args->listptr;
		while (arg != NULL) {
			if (arg->type != EXPRINT) {
				print_err("%s", "Error Math without int\n");
				exit(1);
			}
			result = func(result, arg->intvalue, &b);
			arg = arg->next;
		}

		expr *newexpr;

		if (as_bool) {
			if (!b)
				newexpr = create_exprsym(FALSE);
			else
				newexpr = create_exprsym(TRUE);
		} else {
			newexpr = create_exprint(result);
		}

		return newexpr;
	}
	return NULL;		//What to do??
}

/* a + b */
int addInt(int a, int b, bool * c)
{
	return a + b;
}

/* a - b */
int subInt(int a, int b, bool * c)
{
	return a - b;
}

/* a * b */
int mulInt(int a, int b, bool * c)
{
	return a * b;
}

/* a < b */
int lessInt(int a, int b, bool * c)
{
	if (!(a < b))
		*c = false;
	return b;
}

/* a > b */
int greaterInt(int a, int b, bool * c)
{
	if (a <= b)
		*c = false;
	return b;
}

expr *add(expr * args)
{
	math(args, addInt, 0, false);
}

expr *sub(expr * args)
{
	math(args, subInt, 0, false);
}

expr *mul(expr * args)
{
	math(args, mulInt, 1, false);
}

expr *less(expr * args)
{
	math(args, lessInt, INT_MIN, true);
}

expr *greater(expr * args)
{
	math(args, greaterInt, INT_MAX, true);
}

/*
 * Inititalizes an environment with global values.
 * Params:
 *   en : the environment which should be initialized.
 */
void init_global(env * en)
{
	add_to_env(en, create_exprsym("gc"), create_exprproc(gc), false);
	add_to_env(en, create_exprsym(TRUE), create_exprsym(TRUE), false);
	add_to_env(en, create_exprsym(FALSE), create_exprsym(FALSE), false);
	add_to_env(en, create_exprsym("+"), create_exprproc(add), false);
	add_to_env(en, create_exprsym("-"), create_exprproc(sub), false);
	add_to_env(en, create_exprsym("*"), create_exprproc(mul), false);
	add_to_env(en, create_exprsym("<"), create_exprproc(less), false);
	add_to_env(en, create_exprsym(">"), create_exprproc(greater), false);
}

expr *test(char *str, env * en)
{
	return eval(read(&str), en);
}

bool test_int(char *str, int intvalue, env * en)
{

	char *tmp = str;
	expr *retval = test(str, en);
	if (retval->type == EXPRINT && retval->intvalue == intvalue) {
		debug_info("Success. %s == %d\n\n", tmp, intvalue);
		return true;
	}
	print_err("Test failed for %s : %d. Result: ", tmp, intvalue);
	print_expr(retval);
	return false;
}

/*
 * Run some tests...
 * A nice collection of basic scheme test can be found on:
 *   http://norvig.com/lispytest.py
 * TODO: Remove all of those exit(-1) so we can test several wrong inputs.
 */
int run_tests()
{
	printf("Running tests...\n");

	test_int("(+ 2 2)", 4, global_env);
	test_int("(+ (* 2 100) (* 1 10))", 210, global_env);
	test_int("(if (> 6 5) (+ 1 1) (+ 2 2))", 2, global_env);
	test_int("(if (< 6 5) (+ 1 1) (+ 2 2))", 4, global_env);
	test("(define x 3)", global_env);
	test_int("x", 3, global_env);
	test_int("(+ x x)", 6, global_env);
	test_int("((lambda (x) (+ x x)) 5)", 10, global_env);
	test("(define twice (lambda (x) (* 2 x)))", global_env);
	test_int("(twice 5)", 10, global_env);
	test("(define fact (lambda (n) (if (< (+ n -1) 1) 1 (* n (fact (+ n -1))))))", global_env);
	test_int("(fact 10)", 3628800, global_env);
	test("(define a 0)", global_env);
	test("(define f_def (lambda (n) (begin (define a n) a)))", global_env);
	test_int("(f_def 10)", 10, global_env);
	test_int("a", 0, global_env);
	test("(define f_set (lambda (n) (begin (set! a n) a)))", global_env);
	test_int("(f_set 12)", 12, global_env);
	test_int("a", 12, global_env);
}

#define MAXINPUT 512

int main(int argc, char **argv)
{
	char inputbuf[MAXINPUT];
	global_env = create_env(NULL, NULL);
	init_global(global_env);
#ifdef DEBUG
	run_tests();
#endif
	printf("Interactive Mini-Scheme interpreter:\n");
	printf("  available forms are: define, set!, lambda, begin and if.\n");
	printf("  available functions are: +, *, <, >\n");
	while (1) {
		printf("> ");
		fflush(stdout);
		fgets(inputbuf, MAXINPUT, stdin);
		char *newline = strrchr(inputbuf, '\n');
		if (newline != NULL)
			*newline = 0;
		debug_info("CALL READ with'%s'\n", inputbuf);
		char *ptr = inputbuf;
		print_expr(eval(read(&ptr), global_env));
	}
	system("/bin/sh");
}
