
JOBNAME="presentation" # The name for the output pdf file.
MAIN="main" # Without .tex.

TEXFILES = $(wildcard *.tex) $(wildcard */*.tex)

all: ${JOBNAME}.pdf

${JOBNAME}.pdf:
	pdflatex -jobname ${JOBNAME} ${MAIN}.tex

spellcheck: ${TEXFILES}
	hunspell -l -t -d de_DE -i utf-8 $^

clean:
	rm *.log
	rm *.nav
	rm *.aux
	rm *.snm
	rm *.out
	rm *.toc
	rm sections/*.aux
	rm sources/*.aux
