#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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
		printf("[getListSize] Error: argument e is NULL.\n");
		return -1;
	}
	if (e->type != EXPRLIST) {
		printf
		    ("[getListSize] Error: argument e is not an expression list.\n");
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
			printf("Error. Variable %s not defined.\n",
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
			printf("Error. Variable %s not defined here.\n",
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

expr *eval(expr * e, env * en)
{
	printf("Eval called with");
	printexpr(e);
	printf("\n");
	if (e->type == EXPRSYM) {
		printf("SYM\n");
		expr *res = findInDict(e, en);
		if (res == NULL) {
			printf("[eval] Error: Variable not defined here: %s.\n",
			       e->symvalue);
			exit(-1);
		}
		return res;
	}
	if (e->type != EXPRLIST) {
		return e;
	}
	if (e->listptr == NULL) {
		printf("Error empty list probably ()\n");
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
			    ("[eval] Error: Too few arguments for 'define'.\n");
			exit(-1);
		}
		expr *key = getNext(e, 1);
		expr *value = getNext(e, 2);
		if (key->type != EXPRSYM) {
			printf
			    ("[eval] Error: Argument 1 for 'define' is not a symbol.\n");
			exit(-1);
		}
		key->next = eval(value, en);
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
			printf("[eval] Error. Too few arguments for 'if'.\n");
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
	expr *te = e->listptr;
	expr *procexpr = e->listptr;
	/* first element is the proc skip this and evaluate the shorter list */
	e->listptr = 0;
	while (te != 0 && te->next != 0) {
		expr *savednext = te->next->next;
		te->next = eval(te->next, en);
		te->next->next = savednext;
		te = te->next;
		if (e->listptr == 0)
			e->listptr = te;
	}
	procexpr = eval(procexpr, en);
	if (procexpr->type == EXPRPROC) {
		printf("Call proc!\n");
		printexpr(e);
		printf("---\n");
		expr *res = procexpr->proc(e);
		res->next = 0;
		printexpr(res);
		printf("---\n");
		return res;
	}

	/* We should never arrive here... */
	printf("[eval] Error: Could not evaluate expression: ");
	printexpr(e);
	printf("\n");
	exit(-1);
}

expr *read(char *s[])
{
	printf("Read called with %s\n", *s);
	char *tptr;
	for (tptr = *s; *tptr != 0 && *tptr == ' '; tptr++) ;
	if (*tptr == 0) {
		printf("Error EOF not expected\n");
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

expr *math(expr * args, int neutral, int (*func) (int, int))
{
	if (args->type == EXPRINT)
		return args;
	else if (args->type == EXPRLIST) {
		int res = neutral;
		expr *arg = args->listptr;
		while (arg != NULL) {
			if (arg->type != EXPRINT) {
				printf("Error Math without int\n");
				exit(1);
			}
			res = func(res, arg->intvalue);
			arg = arg->next;
		}
		expr *newexpr = malloc(sizeof(expr));
		newexpr->type = EXPRINT;
		newexpr->intvalue = res;
		return newexpr;
	}
	return NULL;		//What to do??
}

int addInt(int a, int b)
{
	return a + b;
}

expr *add(expr * args)
{
	math(args, 0, addInt);
}

bool testInt(char *str, int intvalue, env * en)
{

	char *tmp = str;
	expr *retval = eval(read(&str), en);
	if (retval->type == EXPRINT && retval->intvalue == intvalue) {
		printf("[testInt] Success. %s == %d\n", tmp, intvalue);
		return true;
	}
	printf("[testInt] Error. Test failed for %s : %d. Result: ", tmp,
	       intvalue);
	printexpr(retval);
	printf("\n");
	return false;
}

void initGlobal(env * en)
{
	en->outer = 0;
	en->list = 0;
	addToEnv(en, createExprSym(TRUE), createExprSym(TRUE), false);
	addToEnv(en, createExprSym(FALSE), createExprSym(FALSE), false);
	addToEnv(en, createExprSym("+"), createExprProc(add), false);
}

int runTests()
{
	env *test_env = malloc(sizeof(env));
	initGlobal(test_env);

	printf("Running tests...\n");

	testInt("(+ 1337 42)", 1379, test_env);
	testInt("0", 1, test_env);

	free(test_env);
}

#define MAXINPUT 512
int main(int argc, char **argv)
{
	char inputbuf[MAXINPUT];
	global_env = malloc(sizeof(env));
	initGlobal(global_env);

	runTests();

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
