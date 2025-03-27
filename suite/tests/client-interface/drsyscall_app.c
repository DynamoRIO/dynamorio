/* **********************************************************
 * Copyright (c) 2012-2014 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Test of the Dr. Syscall Extension */

#include <stdio.h>
#include <stdlib.h>
#ifdef UNIX
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/socket.h>
# ifdef MACOS
#  include <netinet/in.h>
#  include <sys/un.h>
# else
#  include <linux/in.h>
#  include <linux/un.h>
#  include <linux/in6.h>
# endif
#else
# include <windows.h>
#endif

static void
syscall_test(void)
{
#ifdef UNIX
    int fd = open("/dev/null", O_WRONLY);
    int *uninit = (int *) malloc(sizeof(*uninit));
    write(fd, uninit, sizeof(*uninit));
    free(uninit);
#else
    MEMORY_BASIC_INFORMATION mbi;
    void **uninit = (void **) malloc(sizeof(*uninit));
    VirtualQuery(*uninit, &mbi, sizeof(mbi));
    free(uninit);
#endif
}

#ifdef UNIX
static void
socket_test(void)
{
    int s;
    int i;
    socklen_t addrlen;

    s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    addrlen = sizeof(sa)/2; /* test i#1119 */
    getsockname(s, (struct sockaddr*)&sa, &addrlen);

    s = socket(AF_INET, SOCK_STREAM, 0);
    addrlen = sizeof(sa);
    getsockname(s, (struct sockaddr*)&sa, &addrlen);

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sa2;
    addrlen = sizeof(sa2);
    getsockname(s, (struct sockaddr*)&sa2, &addrlen);

    s = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 sa3;
    addrlen = sizeof(sa3);
    getsockname(s, (struct sockaddr*)&sa3, &addrlen);
}
#endif

int
main(int argc, char **argv)
{
    syscall_test();
#ifdef UNIX
    socket_test();
#endif
    printf("done\n");
    return 0;
}
