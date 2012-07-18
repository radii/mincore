#define _GNU_SOURCE 1 /* for O_NOATIME */
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
#include <sys/time.h>

void die(char *fmt, ...) __attribute__((noreturn));
void die(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

typedef unsigned long long u64;

u64 rtc(void)
{
    static struct timeval t0;
    struct timeval t;

    if(t0.tv_sec == 0) {
	gettimeofday(&t0, 0);
	return 0;
    }
    gettimeofday(&t, 0);
    return (u64)(t.tv_sec - t0.tv_sec) * 1000000 + t.tv_usec - t0.tv_usec;
}

void usage (char *progname)
{
    fprintf(stderr, "Usage: %s -[afmprst] [-i infile | file b1 [b2 ...]]\n", progname);
    fprintf(stderr, "%s: opportunistically page-in file blocks\n", progname);
    fprintf(stderr, "  -a: set O_NOATIME (must be file owner or root)\n");
    fprintf(stderr, "  -i: read input blocklist from infile (use '-' for stdin)\n");
    fprintf(stderr, "  -f: use posix_fadvise(2) (default)\n");
    fprintf(stderr, "  -m: use madvise(2)\n");
    fprintf(stderr, "  -p: use mmap(2) + pagefault\n");
    fprintf(stderr, "  -r: use read(2)\n");
    fprintf(stderr, "  -s: compute success statistics using mincore(2)\n");
    fprintf(stderr, "  -t: print timing information\n");
    exit(1);
}

int parse_intrange(const char *, off_t *, off_t *);
int pagein_fadvise(const char *, off_t);
int account_pagein(u64, u64);
int account_open(u64, u64);

int pgsz;
int o_time = 0, o_noatime = 0;

int main(int argc, char **argv)
{
    int c, i;
    char *fname;
    char *infile = NULL;
    int (*pagein)(const char *, off_t) = pagein_fadvise;

    pgsz = getpagesize();

    while ((c = getopt(argc, argv, "afmprst")) != EOF) {
	switch(c) {
	    case 'a':
		o_noatime = 1; break;
	    case 'f':
		pagein = pagein_fadvise;
		break;
            case 'i':
                infile = optarg;
                break;
	    case 't':
		o_time = 1;
		/* XXX not done yet, fall through */
	    case 'm':
	    case 'p':
	    case 'r':
	    case 's':
		die("unimplemented option '%c'\n", c);
	    default: usage(argv[0]);
	}
    }

    if (infile) {
    } else {
        if (argc < optind + 2) usage(argv[0]);

        fname = argv[optind++];

        for (i = optind; i < argc; i++) {
            off_t blkno, a, b;
            parse_intrange(argv[i], &a, &b);

            for(blkno = a; blkno <= b; blkno++) {
                pagein(fname, blkno);
            }
        }
    }
    return 0;
}

int parse_intrange(const char *s, off_t *a, off_t *b)
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
    u64 t1;
    int onoatime = o_noatime ? O_NOATIME : 0;

    if(!strcmp(fname, prevfname))
	return prevfd;

    if(o_time) t1 = rtc();
    if((fd = open(fname, O_RDONLY|onoatime)) == -1)
	die("%s: %s\n", fname, strerror(errno));
    if(o_time) account_open(t1, rtc());

    if(prevfd > 0) close(prevfd);
    prevfd = fd;
    strncpy(prevfname, fname, PATH_MAX);
    return fd;
}

int pagein_fadvise(const char *fname, off_t o)
{
    int fd = getfd(fname);
    u64 t1;

    t1 = rtc();
    if(posix_fadvise(fd, o * pgsz, pgsz, POSIX_FADV_WILLNEED) == -1)
	fprintf(stderr, "fadvise(%s, %lld, %lld, WILLNEED): %s\n",
		fname, (long long)o, (long long)pgsz, strerror(errno));
    if(o_time) account_pagein(t1, rtc());
    return 1;
}

int account_pagein(u64 t1, u64 t2)
{
    die("sorry, accounting not implemented yet\n");
}

int account_open(u64 t1, u64 t2)
{
    die("sorry, accounting not implemented yet\n");
}
