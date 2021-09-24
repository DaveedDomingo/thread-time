# -fpic https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html#Code-Gen-Options
# -Wall https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options
# -shared https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html#Link-Options
# -ldl Dynamically Loaded Library https://tldp.org/HOWTO/Program-Library-HOWTO/dl-libraries.html

CC=g++
CFLAGS= -std=c++11 -g -fpic -Wall
LINKS=-ldl -lpthread


all: shim

shim:
	@echo compiling shim library
	$(CC) $(CFLAGS) $(USER_CFLAGS) -shared time.cpp -o time.so $(LINKS)

clean:
	@echo cleaning everything
	rm -f *.o *.so

