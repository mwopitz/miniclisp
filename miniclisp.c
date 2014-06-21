#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXTOKENLEN 32

const char * TRUE="#t";
const char * FALSE="#f";

enum exprtype {EXPRLIST, EXPRSYM, EXPRINT,EXPRLAMBDA,EXPPROC};
typedef struct expr{
	enum exprtype type;
	long int intvalue;
	char symvalue[MAXTOKENLEN];
	struct expr * listptr;		 
	struct expr * lambdavars;
	struct expr * lambdaexpr;
	struct expr * ( *proc)(struct expr*);
	struct expr * next;
} expr;

typedef struct dictentry{
	expr *sym;
	expr *value;
	struct dictentry *next;
}dictentry;

typedef struct env{
	dictentry *list;
	struct env *outer;
}env;

expr *findInDict(expr *e,env *en) {
	if(e==NULL || en ==NULL)
		return NULL;
	dictentry * d= en->list;
	while(d !=NULL){
		if(strcmp(e->symvalue, d->sym->symvalue) == 0)
			return d->value;
        	d=d->next;
	}
	return findInDict(e,en->outer);
}


void printexpr(expr* e){
	if(e->type==EXPRLIST){
		printf(" EXPRLIST[");
		expr *t =e->listptr;
		while(t!=NULL){
			printexpr(t);
			t=t->next;
		}
		printf("] ");
	}
	else if(e->type==EXPRINT){
		printf("INT: %lld",e->intvalue);
	}
	else{
		printf(" SYM:'%s' ",e->symvalue);
	}

}

void addToExprlist(expr *list,expr * new){
	if(list==0)
		return;
	else if(list->listptr==0)
		list->listptr=new;
	else {
		expr * t=list->listptr;
		while(t->next!=0){
			t=t->next;
		}
		t->next=new;
	}
}

env *global_env;

expr * eval(expr *e ,env *en){
	printf("Eval called with");
	printexpr(e);
	printf("\n");
	if(e->type==EXPRSYM)
	{
		printf("SYMMMM\n");	
		expr * res=findInDict(e,en);	
		if(res==NULL){
			printf("Variable not defined here %s\n",e->symvalue);
			exit(-1);
		}
		return res;
	}
	if(e->type !=EXPRLIST)
		return e;
	if(e->listptr==NULL)
	{
		printf("Error empty list probably ()\n");
		exit(-1);
	}
	if(e->listptr->type!=EXPRSYM)
	{
		printf("No valid symvalue aft (\n");
		exit(-1);
	}
	if(strcmp(e->listptr->symvalue,"quote")==0){
		expr* next=e->listptr->next;
		free(e->listptr);
		e->listptr=next;
		return e;
	}
	if(strcmp(e->listptr->symvalue,"if")==0){
		if(e->listptr->next== 0 || e->listptr->next->next==0 || e->listptr->next->next->next==0)
		{
			printf("not enough arguments for if\n");
			exit(-1);
		}
		expr* cond=eval(e->listptr->next,en);
		expr * trueex=e->listptr->next->next;
		expr * falseex=e->listptr->next->next->next;
		if(cond->type==EXPRINT)
			return eval(trueex,en);
		else if(cond->type!= EXPRSYM)
		{
			printf("Illegal if condition. Must be Symbol or number\n");
			exit(-1);
		}
		else if(strcmp(cond->symvalue,TRUE)==0)
			return eval(trueex,en);
		else if(strcmp(cond->symvalue,FALSE)==0)
			return eval(falseex,en);
		else{
			printf("Wrong Symbol");
			exit(-1);
		}
		
	}
	return e;
}


expr * read (char ** s){
	printf("Read called with %s\n",*s);
	char * tptr;
	for(tptr=*s;*tptr!=0 && *tptr==' '; tptr++);
	if(*tptr==0)
	{
		printf("Error EOF not expected\n");
		exit(-1);
	}
		int tokenlen;
		char *tmpptr;
		if(*tptr=='('){
			expr * exprlist= malloc(sizeof(expr));
			exprlist->type=EXPRLIST;
			exprlist->listptr=0;
			tptr++;
			while(*tptr!= ')'){
				addToExprlist(exprlist,read(&tptr));
				for(;*tptr!=0 && *tptr==' '; tptr++);
			}	
			tptr++;
			*s=tptr;
			return exprlist;	
		}
		else if(*tptr==')'){
			printf("Error ) was not expected here\n");
			exit(-1);
			tokenlen=1;
		}
		else{
			for(tokenlen=0,tmpptr=tptr; *tmpptr!=0 && *tmpptr!=' ' && *tmpptr!='(' && *tmpptr!=')'; tokenlen++,tmpptr++ );
			if(tokenlen>=MAXTOKENLEN-1)
			{
				printf("Too long token \n");
				exit(0);
			}
			expr * newexpr=malloc(sizeof(expr));
			newexpr->type=EXPRINT;
			char *afternum=0;
			newexpr->intvalue=strtol(tptr,&afternum,0);
			if(afternum==tptr)
			{
				newexpr->type=EXPRSYM;
				strncpy(newexpr->symvalue,tptr,tokenlen);
				newexpr->symvalue[tokenlen]=0;
				newexpr->next=0;
				//printf("New TOKEN: '%s'(%d)\n",newexpr->symvalue,tokenlen);
			}
			*s=tmpptr;
			return newexpr;
		}
}
/**        LAMBDA PREDEFINED FUNCTIONS: **/



expr * math(expr *args,int neutral,int (*func)(int,int)){
	if(args->type == EXPRINT)
		return args;
	else if (args->type == EXPRLIST){
		int res=neutral;
		expr *arg = args->listptr;
		while (arg != NULL)
			if (arg->type != EXPRINT) { 
				printf("Error Math without int\n");
				exit(1);
           		}
			res=func(res,arg->intvalue);
			arg=arg->next;
		expr * newexpr=malloc(sizeof(expr));
		newexpr->type=EXPRINT;
		newexpr->intvalue=res;
		return newexpr;
	}
	return NULL; //What to do??
}

int addInt (int a, int b){
     return a+b;
}

expr* add(expr *args){
    math(args,0,addInt);
}
#define MAXINPUT 512
int main (int argc, char **argv)
{
	char inputbuf[MAXINPUT];
	global_env=malloc(sizeof(env));
	global_env->list=NULL;
	global_env->outer=NULL;
	printf("Interactive Scheme interpreter:");
	while(1){
		fgets(inputbuf,MAXINPUT,stdin);
		char* newline=strrchr(inputbuf,'\n');
		if(newline!=NULL)
			*newline=0;
		printf("CALL READ with'%s'\n",inputbuf);
		char *ptr= inputbuf;
		printexpr(eval(read(&ptr),global_env));
	}
}




