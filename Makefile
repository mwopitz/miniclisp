all:
	gcc -fstack-protector-all -g -o miniclisp miniclisp.c
	indent -linux miniclisp.c
clean:
	rm miniclisp
