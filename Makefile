ramdisk: ramdisk.c rdstructs.h
	echo "gcc -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk" | bash
clean:	
	rm -f ramdisk