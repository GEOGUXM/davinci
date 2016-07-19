#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "system.h"

// TODO(rswinkle) we should stick to one test macro for windows or at least
// the same test everywhere.  We use __MINGW32__, _WIN32, __CYGWIN__ all over the
// place inconsistently.  Plus now I'm aiming for mingw64 so maybe we should just
// go with _WIN32 or whatever is always defined on windows no matter what toolchain
// you use

#ifdef __MINGW32__
#include <windows.h>
#endif

#ifndef _WIN32

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include <limits.h>

/* void xfree(void *data) { if (data) free(data); } */

#ifdef HAVE_SYS_TIME_H
#ifndef _WIN32
#include <sys/time.h>
#else
#include <sys/timeb.h>
#endif
#endif

#ifndef _WIN32
#ifndef HAVE_GETDTABLESIZE
getdtablesize()
{
	struct rlimit rlp;

	getrlimit(RLIMIT_NOFILE, &rlp);
	return (rlp.rlim_cur);
}
#endif
#endif /* _WIN32 */

#ifndef HAVE_GETWD
char* getwd(char* path)
{
	char buf[256];
	getcwd(buf, 256);
	if (path != NULL) strcpy(path, buf);
	return (path);
}
#endif

/*
 *  NAME:
 *      usleep     -- This is the precision timer for Test Set
 *                    Automation. It uses the select(2) system
 *                    call to delay for the desired number of
 *                    micro-seconds. This call returns ZERO
 *                    (which is usually ignored) on successful
 *                    completion, -1 otherwise.
 *
 *  ALGORITHM:
 *      1) We range check the passed in microseconds and log a
 *         warning message if appropriate. We then return without
 *         delay, flagging an error.
 *      2) Load the Seconds and micro-seconds portion of the
 *         interval timer structure.
 *      3) Call select(2) with no file descriptors set, just the
 *         timer, this results in either delaying the proper
 *         ammount of time or being interupted early by a signal.
 *
 *  HISTORY:
 *      Added when the need for a subsecond timer was evident.
 *
 *  AUTHOR:
 *      Michael J. Dyer                   Telephone:   AT&T 414.647.4044
 *      General Electric Medical Systems        GE DialComm  8 *767.4044
 *      P.O. Box 414  Mail Stop 12-27         Sect'y   AT&T 414.647.4584
 *      Milwaukee, Wisconsin  USA 53201                      8 *767.4584
 *      internet:  mike@sherlock.med.ge.com     GEMS WIZARD e-mail: DYER
 */

#ifndef _WIN32
#ifndef HAVE_USLEEP
int usleep(microSeconds) unsigned long int microSeconds;
{
	unsigned int Seconds, uSec;
	int nfds;
	struct timeval Timer;

	nfds = 0;

	if ((microSeconds == (unsigned long)0) || microSeconds > (unsigned long)4000000) {
		errno = ERANGE; /* value out of range */
		perror("usleep time out of range ( 0 -> 4000000 ) ");
		return -1;
	}

	Seconds = microSeconds / (unsigned long)1000000;
	uSec    = microSeconds % (unsigned long)1000000;

	Timer.tv_sec  = Seconds;
	Timer.tv_usec = uSec;

	if (select(nfds, NULL, NULL, NULL, &Timer) < 0) {
		perror("usleep (select) failed");
		return -1;
	}

	return 0;
}
#endif
#endif /* _WIN32 */

/*
 * Copyright (c) 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef HAVE_STRTOUL

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strtoul.c	5.3 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long strtoul(nptr, endptr, base) const char* nptr;
char** endptr;
register int base;
{
	register const char* s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c   = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0) base = c == '0' ? 8 : 10;
	cutoff              = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim              = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base) break;
		if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc   = ULONG_MAX;
		errno = ERANGE;
	} else if (neg)
		acc                  = -acc;
	if (endptr != 0) *endptr = any ? (char*)(s - 1) : (char*)nptr;
	return (acc);
}

#endif
/* Return the basename of a pathname.
   This file is in the public domain. */

/*
NAME
    basename -- return pointer to last component of a pathname

SYNOPSIS
    char *basename (const char *name)

DESCRIPTION
    Given a pointer to a string containing a typical pathname
    (/usr/src/cmd/ls/ls.c for example), returns a pointer to the
    last component of the pathname ("ls.c" in this case).

BUGS
    Presumes a UNIX style path with UNIX style separators.
*/

#ifndef HAVE_BASENAME

char* basename(const char* name)
{
	const char* base = name;

	while (*name) {
		if (*name++ == '/') {
			base = name;
		}
	}
	return (char*)base;
}

#endif

#ifndef HAVE_DIRNAME
char* dirname(char* path)
{
	char* newpath;
	char* slash;
	int length; /* Length of result, not including NUL.  */

	slash = strrchr(path, '/');
	if (slash == 0) {
		/* File is in the current directory.  */
		path   = ".";
		length = 1;
	} else {
		/* Remove any trailing slashes from the result.  */
		while (slash > path && *slash == '/') --slash;

		length = slash - path + 1;
	}
	newpath = (char*)malloc(length + 1);
	if (newpath == 0) return 0;
	strncpy(newpath, path, length);
	newpath[length] = 0;
	return newpath;
}
#endif

#ifndef HAVE_STRDUP
char* strdup(const char* s1)
{
	char* s2;
	if (s1 == NULL) s1 = "";

	s2 = (char*)malloc(strlen(s1) + 1);
	memcpy(s2, s1, strlen(s1) + 1);
	return (s2);
}

#endif

#ifndef HAVE_STRNDUP
char* strndup(const char* s1, size_t len)
{
	char* s2;
	if (s1 == NULL) s1 = "";

	s2 = (char*)malloc(len + 1);
	memcpy(s2, s1, len);
	s2[len] = '\0';
	return (s2);
}
#endif

void* my_realloc(void* ptr, int len)
{
	if (ptr == NULL) {
		return (calloc(1, len));
	} else {
		return (realloc(ptr, len));
	}
}

void rmrf(const char* path)
{
	DIR* dir;
	struct dirent* d;
	char target[1024];

	dir = opendir(path);
	/* eandres: Only delete it if it exists. */
	if (dir != NULL) {
		while ((d = readdir(dir)) != NULL) {
			if (strcmp(d->d_name, ".") && strcmp(d->d_name, "..")) {
				sprintf(target, "%s/%s", path, d->d_name);
				unlink(target);
			}
		}
		closedir(dir);
		rmdir(path);
	}
}

/*
** Create a temporary file, using the nameing convention dujour
** Returns NULL if it can't figure one out.
*/

char* make_temp_file_path()
{
	int fd;
	int uretval;
	char dirbuf[2048];
	char pathbuf[2048];
	char* tmpdir = getenv("TMPDIR");

	if (tmpdir == NULL) tmpdir = "/tmp";

/** To handle racing issues, since mkstemp does not exist in MINGW **/
#ifdef __MINGW32__
	sprintf(dirbuf, "%s", tmpdir);
	uretval = GetTempFileName(dirbuf,   // directory for tmp files
	                          "dv",     // temp file name prefix
	                          0,        // create unique name
	                          pathbuf); // buffer for name
	if (uretval == 0) {
		return (NULL);
	}
#else
	sprintf(pathbuf, "%s/XXXXXX", tmpdir);
	fd = mkstemp(pathbuf);
	if (fd == -1) {
		return (NULL);
	}
	close(fd);
#endif
	return (strdup(pathbuf));
}

char* make_temp_file_path_in_dir(char* dir)
{
	int fd;
	int uretval;
	char dirbuf[2048];
	char pathbuf[2048];
	char* tmpdir = getenv("TMPDIR");

	if (tmpdir == NULL) tmpdir = (char*)"/tmp";
	if (dir != NULL) tmpdir    = dir;

/** To handle racing issues, since mkstemp does not exist in MINGW **/
#ifdef __MINGW32__
	sprintf(dirbuf, "%s", tmpdir);
	uretval = GetTempFileName(dirbuf,   // directory for tmp files
	                          "dv",     // temp file name prefix
	                          0,        // create unique name
	                          pathbuf); // buffer for name
	if (uretval == 0) {
		return (NULL);
	}
#else
	sprintf(pathbuf, "%s/XXXXXX", tmpdir);
	fd = mkstemp(pathbuf);
	if (fd == -1) {

		return (NULL);
	}
	close(fd);
#endif

	return (strdup(pathbuf));
}

#ifndef HAVE_MMAP
void* mmap(void* x0, size_t len, int x1, int x2, int fd, size_t off)
{
	char* buf;

	lseek(fd, off, SEEK_SET);
	if ((buf = (char*)malloc(len)) == NULL) {
		fprintf(stderr, "Unable to allocate memory for mmap.\n");
		exit(1);
	}
	read(fd, buf, len);
	return (buf);
}

void munmap(void* buf, int len)
{
	free(buf);
}
#endif

#ifndef HAVE_RANDOM
#endif

#ifndef HAVE_DRAND48
#include <math.h>
const long r_l_max = 2 << 31;
const long r_l_min = -(2 << 31);

double drand48(void)
{
	return ((double)rand()) / ((double)(RAND_MAX));
}

long mrand48(void)
{
	return (long)(((double)rand() / (double)RAND_MAX) * ((double)(r_l_max - r_l_min) + r_l_min));
}
long lrand48(void)
{
	return (long)(((double)rand() / (double)RAND_MAX) * ((double)r_l_max));
}

void srand48(long seedval)
{
	srand(seedval);
}
#endif
