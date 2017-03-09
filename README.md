An in-memory filesystem (ie, RAMDISK) using FUSE.

Objective: Design, implement, and evaluate an in-memory filesystem.

##FUSE

Modern operating systems support multiple filesystems. The operating system directs each filesystem operation to the appropriate implementation of the routine. It multiplexes the filesystem calls across many distinct implementations. For example, on a read system call the operation system uses NFS code if it is an NFS file but uses ext3 code if it is an ext3 file.

FUSE (Filesystem in Userspace) is an interface that exports filesystem operations to user-space. Thus filesystem routines are executed in user-space. Continuing the example, if the file is a FUSE file then operating system upcalls into user space in order to invoke the code associated with read.

##RAMDISK

Create an in-memory filesystem. Instead of reading and writing disk blocks, the RAMDISK filesystem will use main memory for storage. 

The RAMDISK filesystem will support the basic POSIX/Unix commands listed below. Externally, the RAMDISK appears as a standard Unix FS. Notably, it is hierarchical (has directories) and must support standard accesses, such as read, write, and append. However, the filesystem is not persistent. The data and metadata are lost when the process terminates, which is also when the process frees the memory it has allocated.

The internal design of the filesystem is When RAMDISK is started it will setup the filesystem, including all metadata. In other words, there is no need to run mkfs(8).

Filesystem is also persistent. Therefore provided a mount point, when unmounted saves the data to disk( as one file). If the named file exists, then the image is read and loaded into the RAM filesystem at startup. If it does not exist, then start up a fresh (empty) filesystem. In either case, write the image to the named file when the RAM filesystem is unmounted.

Makefile creates an executable program named ramdisk, and this program can accept three parameters: 
- the directory to mount (should be empty) 
- the size (MB) of your filesystem 
- an additional, optional argument, path of a file to load data from when mounted or save data to when unmounted. The size of the file will the size you provide as argument 2

    ramdisk <mount_point> <size> [<filename>] 

Be able to run postmark in unbuffered mode. This requires supporting at least the following system calls.

    open, close
    read, write
    creat [sic], mkdir
    unlink, rmdir
    opendir, readdir 

RAMDISK is not expected to support the following:

    Access control,
    Links, and
    Symbolic links.

Resources

    FUSE
        Home page
        Wiki
        FUSE on VCL 
    Relevant man pages
        close(2), ioctl(2), lseek(2), mmap(2), open(2), pread(2), readdir(4), readlink(2), write(2), fread(3) 
    Useful Tools
        strace (help you understand the FUSE operations required to implement) 

