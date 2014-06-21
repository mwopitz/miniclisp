all:
	gcc -g -D DEBUG -o miniclisp miniclisp.c
	indent -linux miniclisp.c
clean:
	rm miniclisp
