/**
 *The test:
 1. create N*2 non-blocking file descriptors with pipe2();
 2. take read end of each pipe (actual fd numbers are 3, 5, 7 and so on) and call FD_SET for reading for corresponding file descriptors;
 3. call select()

 Observed results:
 * N <= 0 - select() blocks as it should;
 * * 512 <= N <= 671 - select() returns 1 and rfds [511] is set;
 * * 672 <= N <= 1023 - select() returns 1 and no fd is set in rfds;
 * * 1024 <= N <= 1055 - select() returns more than 1 and no fd is set in rfds;
 * * 1056 <= N <= 1641 - select() returns more than 1 and corrupts stack (program segfaults afterwards);
 * * N >= 1642 - FD_SET segfaults
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

int main (int    argc,
          char **argv)
{
  printf ("FD_SETSIZE is %d\n", (int) FD_SETSIZE);

  if (argc < 2) {
    fprintf (stderr, "num_fds not specified\n");
    return EXIT_FAILURE;
  }

  unsigned long const num_fds = strtoul (argv [1], NULL, 0);
  printf ("num_fds: %lu\n", num_fds);

  int fds [num_fds];

  for (unsigned long i = 0; i < num_fds; ++i) {
    int p [2];
    if (pipe2 (p, O_NONBLOCK) == -1) {
      perror ("pipe() failed");
      return EXIT_FAILURE;
    }

    fds [i] = p [0]; // read end of the pipe
    printf ("fds [%lu] = %d\n", i, p [0]);
  }

  fd_set rfds;
  fd_set wfds;
  fd_set efds;

  FD_ZERO (&rfds);
  FD_ZERO (&wfds);
  FD_ZERO (&efds);

  int largest_fd = fds [0];
  printf ("num_fds: %lu\n", num_fds);
  for (unsigned long i = 0; i < num_fds; ++i) {
    printf ("adding fd [%lu] %d\n", i, fds [i]);

    FD_SET (fds [i], &rfds);

    if (fds [i] > largest_fd)
      largest_fd = fds [i];
  }

  printf ("calling select(), largest fd: %d ...\n", largest_fd);

  int const res = select (largest_fd + 1, &rfds, &wfds, &efds, /*timeout=*/ NULL);
  if (res == -1) {
    perror ("select() failed");
    return EXIT_FAILURE;
  }

  printf ("select() returned %d\n", res);

  for (unsigned long i = 0; i < num_fds; ++i) {
    if (FD_ISSET (fds [i], &rfds))
      printf ("rfd [%lu] is set\n", i);
  }

  return 0;
}

