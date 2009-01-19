all: liblru.so test

liblru.so: g-lru-cache.c g-lru-cache.h
	gcc -shared -o $@ -Werror -Wall g-lru-cache.c `pkg-config --libs --cflags gobject-2.0` -fPIC

test: main.c g-lru-cache.c
	gcc $(CFLAGS) -o $@ -Werror -Wall g-lru-cache.c main.c `pkg-config --libs --cflags gobject-2.0` -fPIC

clean:
	rm -rf liblru.so test

run:
	./test
