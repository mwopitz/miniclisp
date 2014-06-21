#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define print_err(fmt, ...) \
        do { fprintf(stderr, "[%s:%d] Error. " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define debug_info(fmt, ...) \
        do { if (DEBUG) fprintf(stdout, "[%s:%d] " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define debug_err(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "[%s:%d] " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define MAXTOKENLEN 32

const char *TRUE = "#t";
const char *FALSE = "#f";

enum exprtype { EXPRLIST, EXPRSYM, EXPRINT, EXPRLAMBDA, EXPRPROC };
typedef struct expr {
	enum exprtype type;
	long int intvalue;
	char symvalue[MAXTOKENLEN];
	struct expr *listptr;
	struct expr *lambdavars;
	struct expr *lambdaexpr;
	struct expr *(*proc) (struct expr *);
	struct expr *next;
} expr;

typedef struct dictentry {
	expr *sym;
	expr *value;
	struct dictentry *next;
} dictentry;

typedef struct env {
	dictentry *list;
	struct env *outer;
} env;

expr *deepCopy(expr * e)
{
	if (e == NULL)
		return NULL;
	expr *newexpr = malloc(sizeof(expr));

	if (e->type != EXPRLIST) {
		memcpy(newexpr, e, sizeof(expr));
		return newexpr;
	}

	newexpr->type = EXPRLIST;

	expr *te = e->listptr;
	expr *prev = NULL;
	while (te != NULL) {
		expr *savednext = te->next;
		te = deepCopy(te);
		if (prev == NULL) {
			newexpr->listptr = te;
		} else {
			prev->next = te;
			te->next = 0;
		}
		prev = te;
		te->next = savednext;
		te = savednext;
	}

	return newexpr;
}

expr *findInDict(expr * e, env * en)
{
	if (e == NULL || en == NULL)
		return NULL;
	dictentry *d = en->list;
	while (d != NULL) {
		if (strcmp(e->symvalue, d->sym->symvalue) == 0)
			return d->value;
		d = d->next;
	}
	return findInDict(e, en->outer);
}

/*
 * Returns the i-th entry of an expr list.
 * Params:
 *   i : the number of the requested entry. 0 is the list head.
 * Returns:
 *   a pointer to the list entry i or NULL if none was found.
 */
expr *getNext(expr * e, int i)
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
int getListSize(expr * e)
{
	if (e == NULL) {
		fprintf(stderr, "[getListSize] Error: argument e is NULL.\n");
		return -1;
	}
	if (e->type != EXPRLIST) {
		fprintf
		    (stderr,
		     "[getListSize] Error: argument e is not an expression list.\n");
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

void printexpr(expr * e)
{
	if (e == NULL) {
		printf("nil");
	} else if (e->type == EXPRLIST) {
		printf(" EXPRLIST[");
		expr *t = e->listptr;
		while (t != NULL) {
			printexpr(t);
			t = t->next;
		}
		printf("]");
	} else if (e->type == EXPRINT) {
		printf(" INT: %lld ", e->intvalue);
	} else if (e->type == EXPRLAMBDA) {
		printf("[LAMBDA EXPR ARGS:");
		printexpr(e->lambdavars);
		printf(" BODY ");
		printexpr(e->lambdaexpr);
		printf("]");
	} else {
		printf(" SYM:'%s' ", e->symvalue);
	}

}

void addToExprlist(expr * list, expr * new)
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

expr *createExprProc(struct expr *(*proc) (struct expr *))
{
	expr *newexpr = malloc(sizeof(expr));
	newexpr->type = EXPRPROC;
	newexpr->next = NULL;
	newexpr->proc = proc;
	return newexpr;
}

expr *createExprSym(const char *s)
{
	expr *newexpr = malloc(sizeof(expr));
	newexpr->type = EXPRSYM;
	newexpr->next = NULL;
	int len = strlen(s);
	if (len >= MAXTOKENLEN - 1) {
		printf("To long token\n");
		exit(-1);
	}
	strcpy(newexpr->symvalue, s);
	return newexpr;
}

expr *createExprInt(int i)
{
	expr *newexpr = malloc(sizeof(expr));
	newexpr->type = EXPRINT;
	newexpr->next = NULL;
	newexpr->intvalue = i;
	return newexpr;
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
dictentry *addToEnv(env * env, expr * sym, expr * value, bool set)
{
	if (env == NULL || sym == NULL || value == NULL)
		return NULL;

	dictentry *current_dict_entry = env->list;

	/* Add new list head if the dictionary is emtpy. */
	if (current_dict_entry == NULL) {
		if (env->outer == NULL && set) {
			fprintf(stderr, "Error. Variable '%s' not defined.\n",
				sym->symvalue);
			return NULL;
		} else if (set) {
			return addToEnv(env->outer, sym, value, set);
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
			/* TODO: Double check whether both frees are necessary/legal. */
			free(current_dict_entry->next->sym);
			free(current_dict_entry->next->value);
			break;
		}
		current_dict_entry = current_dict_entry->next;
	}

	/* Add new last entry. */
	if (current_dict_entry->next == NULL) {
		if (env->outer == NULL && set) {
			return addToEnv(env->outer, sym, value, set);
		} else if (set) {
			fprintf(stderr,
				"Error. Variable '%s' not defined here.\n",
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

env *global_env;

expr *eval(expr *, env *);

expr *evalList(expr * e, env * en)
{
	if (e->type != EXPRLIST) {
		printf("ERROR not an list\n");
		exit(-1);
	}
	expr *te = e->listptr;
	expr *prev = NULL;
	while (te != 0) {
		expr *savednext = te->next;
		te = eval(te, en);
		if (te == NULL) {
			printf("[evalList] remove empty entry from list\n");
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
	printexpr(e);
	printf("\n");

	if (e->type == EXPRSYM) {
		expr *res = findInDict(e, en);
		if (res == NULL) {
			fprintf(stderr,
				"Error: Variable not defined here: %s.\n",
				e->symvalue);
			exit(-1);
		}
		expr *copyexpr = malloc(sizeof(expr));
		memcpy(copyexpr, res, sizeof(expr));
		return copyexpr;
	}
	if (e->type != EXPRLIST) {
		return e;
	}
	if (e->listptr == NULL) {
		fprintf(stderr, "Error empty list probably ()\n");
		exit(-1);
	}
	if (e->listptr->type != EXPRSYM) {
		printf("No valid symvalue aft (\n");
		exit(-1);
	}
	/* DEFINE */
	if (strcmp(e->listptr->symvalue, "define") == 0) {
		if (getListSize(e) != 3) {
			printf
			    ("[eval] Error: Wrong number of arguments for 'define'.\n");
			exit(-1);
		}
		expr *key = getNext(e, 1);
		expr *value = getNext(e, 2);
		if (key->type != EXPRSYM) {
			printf
			    ("[eval] Error: Argument 1 for 'define' is not a symbol.\n");
			exit(-1);
		}
		value = eval(value, en);
		if (addToEnv(en, key, value, false) == NULL) {
			printf("[eval] Error: Could not define %s.\n",
			       key->symvalue);
			exit(-1);
		}
		printf("[eval] Defined value %s.\n", key->symvalue);
		return 0;
	}
	/* QUOTE */
	if (strcmp(e->listptr->symvalue, "quote") == 0) {
		expr *next = e->listptr->next;
		free(e->listptr);
		e->listptr = next;
		return e;
	}
	/* IF */
	if (strcmp(e->listptr->symvalue, "if") == 0) {
		if (getListSize(e) != 4) {
			printf
			    ("[eval] Error. Wrong number of arguments for 'if'.\n");
			exit(-1);
		}
		expr *cond = eval(getNext(e, 1), en);
		expr *trueex = getNext(e, 2);
		expr *falseex = getNext(e, 3);
		if (cond->type == EXPRINT)
			return eval(trueex, en);
		else if (cond->type != EXPRSYM) {
			printf
			    ("Illegal if condition. Must be Symbol or number\n");
			exit(-1);
		} else if (strcmp(cond->symvalue, TRUE) == 0)
			return eval(trueex, en);
		else if (strcmp(cond->symvalue, FALSE) == 0)
			return eval(falseex, en);
		else {
			printf("Wrong Symbol");
			exit(-1);
		}

	}
	if (strcmp(e->listptr->symvalue, "begin") == 0) {
		e->listptr = e->listptr->next;
		return evalList(e, en);
	}
	if (strcmp(e->listptr->symvalue, "lambda") == 0) {
		expr *args = getNext(e, 1);
		if (args->type != EXPRLIST) {
			printf
			    ("[eval] Error First Lambda Parameter must be a list\n");
			exit(-1);
		}
		/* List with only the body */
		e->listptr = args->next;
		expr *lambda = malloc(sizeof(expr));
		lambda->type = EXPRLAMBDA;
		lambda->lambdavars = args;
		lambda->lambdaexpr = e;
		return lambda;

	}
	evalList(e, en);
	if (e->listptr->type == EXPRLAMBDA) {
		env *newenv = malloc(sizeof(env));
		newenv->outer = en;
		newenv->list = 0;
		int argnum = getListSize(e->listptr->lambdavars);
		if (argnum != getListSize(e) - 1) {
			printf
			    ("[eval] Error: Wrong number of arguments for lambda %d required: %d\n",
			     argnum, getListSize(e) - 1);
			exit(-1);
		}
		expr *args = e->listptr->lambdavars->listptr;
		expr *val = e->listptr->next;
		while (args != NULL) {
			if (args->type != EXPRSYM) {
				printf
				    ("(eval] Error: Wrong parameter list for lambda\n");
			}
			addToEnv(newenv, args, val, false);
			args = args->next;
			val = val->next;
		}
		debug_info("%s", "[eval] Evaluate Lambd Expr\n");
		expr *res = 0;
		printexpr(e->listptr->lambdaexpr);
		if (e->listptr->lambdaexpr->type == EXPRLIST) {
			debug_info("%s", " as list\n");
			res = evalList(e->listptr->lambdaexpr, newenv);
		} else {
			debug_info("%s", "as single expr\n");
			expr *res = eval(e->listptr->lambdaexpr, newenv);
		}
		free(newenv);
		return res;
	}
	if (e->listptr->type == EXPRPROC) {
		expr *proc = e->listptr;
		/** delete proc from list **/
		e->listptr = e->listptr->next;
		printf("Call proc!\n");
		printexpr(e);
		printf("---\n");
		expr *res = proc->proc(e);
		res->next = 0;
		printexpr(res);
		printf("---\n");
		return res;
	}

	/* We should never arrive here... */
	print_err("%s", "Could not evaluate expression: ");
	printexpr(e);
	printf("\n");
	exit(-1);
}

expr *read(char *s[])
{
	debug_info("Read called with %s\n", *s);
	char *tptr;
	for (tptr = *s; *tptr != 0 && *tptr == ' '; tptr++) ;
	if (*tptr == 0) {
		fprintf(stderr, "Error. EOF not expected\n");
		exit(-1);
	}
	int tokenlen;
	char *tmpptr;
	if (*tptr == '(') {
		expr *exprlist = malloc(sizeof(expr));
		exprlist->type = EXPRLIST;
		exprlist->listptr = 0;
		tptr++;
		while (*tptr != ')') {
			addToExprlist(exprlist, read(&tptr));
			for (; *tptr != 0 && *tptr == ' '; tptr++) ;
		}
		tptr++;
		*s = tptr;
		return exprlist;
	} else if (*tptr == ')') {
		printf("Error ) was not expected here\n");
		exit(-1);
		tokenlen = 1;
	} else {
		for (tokenlen = 0, tmpptr = tptr;
		     *tmpptr != 0 && *tmpptr != ' ' && *tmpptr != '('
		     && *tmpptr != ')'; tokenlen++, tmpptr++) ;
		if (tokenlen >= MAXTOKENLEN - 1) {
			printf("Too long token \n");
			exit(0);
		}
		expr *newexpr = malloc(sizeof(expr));
		newexpr->type = EXPRINT;
		char *afternum = 0;
		newexpr->intvalue = strtol(tptr, &afternum, 0);
		if (afternum == tptr) {
			newexpr->type = EXPRSYM;
			strncpy(newexpr->symvalue, tptr, tokenlen);
			newexpr->symvalue[tokenlen] = 0;
			newexpr->next = 0;
			//printf("New TOKEN: '%s'(%d)\n",newexpr->symvalue,tokenlen);
		}
		*s = tmpptr;
		return newexpr;
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
				printf("Error Math without int\n");
				exit(1);
			}
			result = func(result, arg->intvalue, &b);
			arg = arg->next;
		}

		expr *newexpr;

		if (as_bool) {
			if (!b)
				newexpr = createExprSym(FALSE);
			else
				newexpr = createExprSym(TRUE);
		} else {
			newexpr = createExprInt(result);
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
void initGlobal(env * en)
{
	en->outer = 0;
	en->list = 0;
	addToEnv(en, createExprSym(TRUE), createExprSym(TRUE), false);
	addToEnv(en, createExprSym(FALSE), createExprSym(FALSE), false);
	addToEnv(en, createExprSym("+"), createExprProc(add), false);
	addToEnv(en, createExprSym("*"), createExprProc(mul), false);
	addToEnv(en, createExprSym("<"), createExprProc(less), false);
	addToEnv(en, createExprSym(">"), createExprProc(greater), false);
}

expr *test(char *str, env * en)
{
	return eval(read(&str), en);
}

bool testInt(char *str, int intvalue, env * en)
{

	char *tmp = str;
	expr *retval = test(str, en);
	if (retval->type == EXPRINT && retval->intvalue == intvalue) {
		printf("[testInt] Success. %s == %d\n", tmp, intvalue);
		return true;
	}
	fprintf(stderr, "[testInt] Error. Test failed for %s : %d. Result: ",
		tmp, intvalue);
	printexpr(retval);
	printf("\n");
	return false;
}

/*
 * Run some tests...
 * A nice collection of basic scheme test can be found on:
 *   http://norvig.com/lispytest.py
 * TODO: Remove all of those exit(-1) so we can test several wrong inputs.
 */
int runTests()
{
	env *test_env = malloc(sizeof(env));
	initGlobal(test_env);

	printf("Running tests...\n");

	testInt("(+ 2 2)", 4, test_env);
	testInt("(+ (* 2 100) (* 1 10))", 210, test_env);
	testInt("(if (> 6 5) (+ 1 1) (+ 2 2))", 2, test_env);
	testInt("(if (< 6 5) (+ 1 1) (+ 2 2))", 4, test_env);
	test("(define x 3)", test_env);
	testInt("x", 3, test_env);
	testInt("(+ x x)", 6, test_env);

	char *test = "(+ 1 2 3 4 5 (+ 1 2))";
	expr *copyme = read(&test);
	expr *new = deepCopy(copyme);

	printf("copyme: %p, new: %p\n", copyme, new);
	printexpr(copyme);
	printf("\n");

	printexpr(new);
	printf("\n");

	free(test_env);
}

#define MAXINPUT 512

int main(int argc, char **argv)
{
	char inputbuf[MAXINPUT];
	global_env = malloc(sizeof(env));
	initGlobal(global_env);
#ifdef DEBUG
	runTests();
#endif
	printf("Interactive Mini-Scheme interpreter:\n");
	while (1) {
		printf("> ");
		fflush(stdout);
		fgets(inputbuf, MAXINPUT, stdin);
		char *newline = strrchr(inputbuf, '\n');
		if (newline != NULL)
			*newline = 0;
		printf("CALL READ with'%s'\n", inputbuf);
		char *ptr = inputbuf;
		printexpr(eval(read(&ptr), global_env));
		printf("\n");
	}
}
