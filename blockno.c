#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/fs.h>

void die(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

void usage (char *progname)
{
    fprintf(stderr, "Usage: %s file b1 [b2 ...]\n", progname);
    exit(1);
}

size_t getblk(const char *, off_t);
int parse_intrange(const char *, size_t *, size_t *);
dev_t getdev(const char *);

int o_verbose;

int main(int argc, char **argv)
{
    int c, i;
    char *fname;

    while ((c = getopt(argc, argv, "v")) != EOF) {
	switch(c) {
	    case 'v': o_verbose++; break;
	    default: usage(argv[0]);
	}
    }

    if (argc < optind + 2) usage(argv[0]);

    fname = argv[optind++];

    for (i = optind; i < argc; i++) {
	size_t diskblk, blkno, a, b;
	dev_t dev = getdev(fname);
	parse_intrange(argv[i], &a, &b);

	for(blkno = a; blkno <= b; blkno++) {
	    diskblk = getblk(fname, blkno);
	    printf("%s %lld %04x %lld\n",
		    fname, (long long)blkno,
		    (int)dev, (long long)diskblk);
	}
    }
    return 0;
}

int parse_intrange(const char *s, size_t *a, size_t *b)
{
    char *p;
    *a = strtoll(s, &p, 0);
    if(!p || !*p) {
	*b = *a;
	return 1;
    }
    if(*p != '-')
	return 0;
    p++;
    *b = strtoll(p, &p, 0);
    return 1;
}

int getfd(const char *fname)
{
    static char prevfname[PATH_MAX];
    static int prevfd = -1;
    int fd;

    if(!strcmp(fname, prevfname))
	return prevfd;

    if((fd = open(fname, O_RDONLY)) == -1)
	die("%s: %s\n", fname, strerror(errno));

    if(prevfd > 0) close(prevfd);
    prevfd = fd;
    strncpy(prevfname, fname, PATH_MAX);
    return fd;
}

dev_t getdev(const char *fname)
{
    int fd = getfd(fname);
    struct stat st;

    fstat(fd, &st);
    return st.st_dev;
}

size_t getblk(const char *fname, off_t b)
{
    int fd = getfd(fname);
    int bsz, bno;
    int pgsz = getpagesize();

    if(ioctl(fd, FIGETBSZ, &bsz) == -1)
	die("ioctl(FIGETBSZ, %s): %s\n", fname, strerror(errno));

    if(pgsz > 0 && bsz > 0 && pgsz != bsz)
	bno = (b * pgsz) / bsz;
    else
	bno = b;

    if(ioctl(fd, FIBMAP, &bno) == -1)
	die("ioctl(FIBMAP, %s): %s\n", fname, strerror(errno));

    return bno;
}
