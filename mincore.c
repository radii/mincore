#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

void die(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

void basic_info(char *fname, unsigned char *vec, int npg);
void summary_info(char *fname, unsigned char *vec, int npg);

static int o_verbose = 0;
static int o_fullname = 0;

void usage(char *progname)
{
    die("Usage: %s [-f] [-v] file1 [file2 ...]\n", progname);
}

static int pagesize;

int mincore_one(char *filename)
{
    int fd, npg;
    struct stat st;
    char *outname;
    unsigned char *vec;
    void *map = 0;
    int retval = 1;

    if((fd = open(filename, O_RDONLY)) == -1) {
	fprintf(stderr, "%s: %s\n", filename, strerror(errno));
	retval = 0;
	goto out;
    }

    if(fstat(fd, &st) == -1) {
	fprintf(stderr, "fstat: %s\n", strerror(errno));
	retval = 0;
	goto out_close;
    }

    npg = st.st_size / pagesize + ((st.st_size % pagesize) != 0);
    vec = malloc(npg);
    if(!vec) {
	fprintf(stderr, "malloc(%d): %s\n", npg, strerror(errno));
	retval = 0;
	goto out_close;
    }
    if(st.st_size) {
	map = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(map == MAP_FAILED) {
	    fprintf(stderr, "mmap(%s): %s\n", filename, strerror(errno));
	    retval = 0;
	    goto out_free;
	}
	if(mincore(map, st.st_size, vec) == -1) {
	    fprintf(stderr, "mincore: %s\n", strerror(errno));
	    retval = 0;
	    goto out_munmap;
	}
    }
    if(o_fullname)
	outname = filename;
    else {
	outname = strrchr(filename, '/');
	outname = outname ? outname + 1 : filename;
    }
    if(o_verbose)
	summary_info(outname, vec, npg);
    else
	basic_info(outname, vec, npg);

out_munmap:
    if(map && munmap(map, st.st_size) == -1)
	fprintf(stderr, "munmap(%p, %ld): %s\n", map, st.st_size,
		strerror(errno));
out_free:
    free(vec);
out_close:
    close(fd);
out:
    return retval;
}

int main(int argc, char **argv)
{
    int c;

    pagesize = getpagesize();

    while((c = getopt(argc, argv, "fhv")) != -1) {
	switch(c) {
	    case 'f': o_fullname = 1; break;
	    case 'h': usage(argv[0]);
	    case 'v': o_verbose = 1; break;
	    default:
	        die("%s: unknown option '%c'\n", argv[0], c);
	}
    }

    for(; optind < argc; optind++) {
	mincore_one(argv[optind]);
    }
    return 0;
}

int count_present(unsigned char *vec, int n, int *runs)
{
    int i, ret, last = -1;

    ret = 0;
    for(i=0; i<n; i++) {
	if(vec[i])
	    ret++;
	if(vec[i] != last && runs) {
	    if(vec[i]) ++*runs;
	    last = vec[i];
	}
    }
    return ret;
}

void basic_info(char *fname, unsigned char *vec, int npg)
{
    int nrun, npresent;

    npresent = count_present(vec, npg, &nrun);
    printf("%s: %d/%d (%d%%) pages present (%d runs)\n",
	    fname, npresent, npg, npg ? npresent * 100 / npg : 0, nrun);
}

/* output bar showing presence of pages in file
 * |      ____....oooOOOXXXXXXXXXXXXX|
 * Each character of output shows a fraction of the file; for a 750-page
 * file in a 75-column display, each character represents 10 file pages.
 *  ' '  no pages present
 *  '_'  <25% pages present
 *  '.'  25%-50% pages present
 *  'o'  50%-75% pages present
 *  'O'  75%-(n-1) pages present
 *  'X'  all pages present
 * Uses Bresenham's algorithm to determine which pages map to a given
 * character in the output.
 */
void summary_info(char *fname, unsigned char *vec, int npg)
{
    int i, width = 60, pgc, err, de, slope;
    const long long SCALE = 1024;
    char out[] = " _.oOX";
    int grads = sizeof(out) - 3;
    int extrawidth = 0;

    if(width > npg) {
	extrawidth = width - npg;
	width = npg;
    }
    putchar('|');
    slope = npg / width;
    pgc = err = 0;
    de = ((SCALE * npg) / width) % SCALE;
    for(i=0; i<width; i++) {
	int chunk = slope;
	int x;

	if(err > SCALE/2) {
	    chunk++;
	    err -= SCALE;
	}
	x = count_present(vec+pgc, chunk, 0);
	pgc += chunk;
	err += de;
	if(x == 0)
	    x = 0;
	else if(x == chunk)
	    x = grads + 1;
	else
	    x = (x * grads) / chunk + 1;
	putchar(out[x]);
    }
    printf("|%*s%s\n", extrawidth, fname ? " " : "", fname);
}
