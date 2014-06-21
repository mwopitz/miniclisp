all:
	gcc -g -o miniclisp miniclisp.c
	indent -linux miniclisp.c
clean:
	rm miniclisp
