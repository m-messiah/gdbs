all: main randgen

main: gdbs.c
	gcc -Wall --pedantic -Werror gdbs.c -o gdbs

clean:
	rm -f gdbs randgen

indent:
	indent gdbs.c

randgen: p.c
	gcc -Wall --pedantic -Werror p.c -o randgen

