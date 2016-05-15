// http://www.gnu.org/software/libc/manual/html_node/Asynchronous-I_002fO.html
// http://www.wangafu.net/~nickm/libevent-book/01_intro.html
// pjlib - socket
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>         // sockaddr_in
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>

void *process(void* fd)
{
  int ifd = (int)fd;
 // printf("process(%d)\n", ifd);
  while(1) {
  }
  return 0;
}

int main()
{
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	if(listenfd == -1) {
		fprintf(stderr, "socket:  %s\n", strerror(errno));
		return -1;
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(5000); 

  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
    fprintf(stderr, "bind: %s\n", strerror(errno));
    return -1;
  }

  if(listen(listenfd, 1 << 10) == -1) {
    fprintf(stderr, "bind: %s\n", strerror(errno));
    return -1;
  }

  fd_set active_fd_set, read_fd_set;
  FD_ZERO(&active_fd_set);
  FD_SET(listenfd, &active_fd_set);
  int conn = 0;
  while(1) {
    read_fd_set = active_fd_set;
    /* select approach
    if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) == -1) {
      perror ("select");
      return -1; 
    }

    for(int i = 0; i < FD_SETSIZE; ++i) {
      if(FD_ISSET(i, &read_fd_set)) {
        if(i == listenfd) {
          struct sockaddr_in caddr;
          socklen_t caddr_len = sizeof(caddr);
          memset(&caddr, 0, sizeof(caddr));
          int cfd = accept(listenfd, (struct sockaddr*)&caddr, &caddr_len);
          if(cfd == -1) {
            perror("accept");
            return -1;
          }
          FD_SET(cfd, &active_fd_set);
        } else {
          // process now until eof
        } 
      }
    }*/

    /* pthread approach
    struct sockaddr_in caddr;
    socklen_t caddr_len = sizeof(caddr);
    memset(&caddr, 0, sizeof(caddr));
    int cfd = accept(listenfd, (struct sockaddr*)&caddr, &caddr_len);
    if(cfd == -1) {
      perror("accept");
      return -1;
    }
    printf("accept: %d\n", ++conn);
    
    pthread_t cth;
    pthread_attr_t cattr;
    pthread_attr_init(&cattr);
    pthread_attr_setdetachstate(&cattr, PTHREAD_CREATE_DETACHED); // since no way to join
    pthread_create(&cth, &cattr, &process, (void*)cfd);
    pthread_attr_destroy(&cattr);
    */
  }

	return 0;
}
