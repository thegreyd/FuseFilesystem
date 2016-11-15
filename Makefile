hello: hello.c
	echo "gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello" | bash
clean:	
	rm -f hello
mount:
	./hello -d /tmp/fu/
unmount:
	fusermount -u /tmp/fu/