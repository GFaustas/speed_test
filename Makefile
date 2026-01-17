LIBS = -lcurl -lcjson
SOURCES = main.c
all: clean main

main: $(SOURCES:%.c=%.o)
	gcc -o main $(SOURCES) $(LIBS)

%.o: %.c
	gcc -c $< -o $@

clean:
	rm -f main main.o