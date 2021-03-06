/* 
 * Copyright (C) 2004
 * 	Hartmut Brandt.
 * 	All rights reserved.
 * 
 * Author: Harti Brandt <harti@freebsd.org>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Begemot: bsnmp/lib/support.c,v 1.1 2004/08/06 08:47:58 brandt Exp $
 *
 * Functions that are missing on certain systems.
 */
#include "bsnmp/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/timeb.h>
#ifndef _WIN32
 #include <fcntl.h>
#endif

#include "support.h"

#ifndef HAVE_STRLCPY

size_t strlcpy(char *dst, const char *src, size_t len)
{
	size_t ret = strlen(dst);

	while (len > 1) {
		*dst++ = *src++;
		len--;
	}
	if (len > 0)
		*dst = '\0';
	return (ret);
}

#endif



int socket_set_blocking(socket_t fd, int blocking)
{
#ifdef WIN32
	{
		u_long nonblocking = (0 == blocking)?1:0;
		if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR) {
			return -1;
		}
	}
#else
	{
		int flags;
		if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
			return -1;
		}
        
        flags = (0 == blocking)?(flags | O_NONBLOCK): (flags & ~O_NONBLOCK);

		if (fcntl(fd, F_SETFL, flags) == -1) {
			return -1;
		}
	}
#endif
	return 0;
}



#ifndef HAVE_GETTIMEOFDAY
/* No gettimeofday; this muse be windows. */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct _timeb tb;

	if (tv == NULL)
		return -1;

	/* XXXX
	 * _ftime is not the greatest interface here; GetSystemTimeAsFileTime
	 * would give us better resolution, whereas something cobbled together
	 * with GetTickCount could maybe give us monotonic behavior.
	 *
	 * Either way, I think this value might be skewed to ignore the
	 * timezone, and just return local time.  That's not so good.
	 */
	_ftime(&tb);
	tv->tv_sec = (long) tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}
#endif

#ifndef HAVE_GETADDRINFO

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>

extern int h_nerr;
extern int h_errno;
extern const char *h_errlist[];

/*
 * VERY poor man's implementation
 */
int
getaddrinfo(const char *host, const char *port, const struct addrinfo *hints,
    struct addrinfo **res)
{
	struct hostent *hent;
	struct sockaddr_in *s;
	struct servent *sent;

	if ((hent = gethostbyname(host)) == NULL)
		return (h_errno);
	if (hent->h_addrtype != hints->ai_family)
		return (HOST_NOT_FOUND);
	if (hent->h_addrtype != AF_INET)
		return (HOST_NOT_FOUND);

	if ((*res = malloc(sizeof(**res))) == NULL)
		return (HOST_NOT_FOUND);

  INCREMENTMEMORY();
	(*res)->ai_flags = hints->ai_flags;
	(*res)->ai_family = hints->ai_family;
	(*res)->ai_socktype = hints->ai_socktype;
	(*res)->ai_protocol = hints->ai_protocol;
	(*res)->ai_next = NULL;

	if (((*res)->ai_addr = malloc(sizeof(struct sockaddr_in))) == NULL) {
		freeaddrinfo(*res);
		return (HOST_NOT_FOUND);
	}
  INCREMENTMEMORY();

	(*res)->ai_addrlen = sizeof(struct sockaddr_in);
	s = (struct sockaddr_in *)(*res)->ai_addr;
	s->sin_family = hints->ai_family;
#ifndef SUN_LEN
	s->sin_len = sizeof(*s);
#endif
	memcpy(&s->sin_addr, hent->h_addr, 4);

	if ((sent = getservbyname(port, NULL)) == NULL) {
		freeaddrinfo(*res);
		return (HOST_NOT_FOUND);
	}
	s->sin_port = sent->s_port;

	return (0);
}

const char *
gai_strerror(int e)
{

	if (e < 0 || e >= h_nerr)
		return ("unknown error");
	return (h_errlist[e]);
}

void
freeaddrinfo(struct addrinfo *p)
{
	struct addrinfo *next;

	while (p != NULL) {
		next = p->ai_next;
		if (p->ai_addr != NULL)
			free(p->ai_addr);
		free(p);
		p = next;
	}
}

#endif
