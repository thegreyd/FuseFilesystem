ramdisk: ramdisk.c rdstructs.h
	gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o ramdisk
clean:	
	rm -f ramdisk