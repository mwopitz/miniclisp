all:
	gcc -fstack-protector-all -m32 -g -o miniclisp miniclisp.c
	gcc hexto32byte.c -o hexto32byte
	indent -linux miniclisp.c
clean:
	rm miniclisp
