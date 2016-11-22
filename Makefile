hello: hello.c rdstructs.h
	echo "gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello" | bash
clean:	
	rm -f hello ramdisk.save
mount:
	./hello /tmp/fu/ 2
unmount:
	fusermount -u /tmp/fu/