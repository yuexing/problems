// listen then fork/exec, listen is to mark the socketfd as passive socket
// which is used for accept(). A listen has a queue, once the child is going
// to connect, then the connection is accepted by the kernel and thus even
// though the parent is dead the child is still alive.
#include <errno.h>                     // errno
#include <stdio.h>                     // perror, fprintf
#include <stdlib.h>                    // exit
#include <string.h>                    // strcmp

#ifdef _WIN32
  #include <winsock2.h>                  // Windows sockets stuff
  typedef int socklen_t;
#else
  #include <fcntl.h>                     // fcntl
  #include <netinet/in.h>                // ?
  #include <sys/socket.h>                // socket, etc.
  #include <sys/types.h>                 // pid_t
  #include <unistd.h>                    // fork, exec
  #define closesocket close
  #define Sleep(ms) sleep(ms/1000)
#endif

#include <string>                      // std::string

using std::string;


char const *proc_label = "parent";


void init_sockets() {
    static bool socket_init_done = false;
    if (!socket_init_done) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 0), &wsaData );
#endif // _WIN32
        socket_init_done = true;
    }
}


string get_last_OS_error_string()
{
  string ret;

#ifdef _WIN32
    DWORD code = GetLastError();
    char *msg;
    if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                       0, code, 0, (LPSTR)&msg, 0, 0)) {
                                // ^^^^^^^ This cast is bizarre, but it
                                // is how the API works.
      // Wonderful..
      char buf[200];    // should be plenty big
      sprintf(buf, "Windows API error code 0x%lX (FormatMessage failed with code 0x%lX)",
                   code, GetLastError());
      assert(strlen(buf) < 200);
      ret = buf;

      // Almost certainly it is possible for FormatMessage to fail
      // before/while allocating 'msg', hence it could not be correct
      // to unconditionally free it here.  If there is a way to
      // discover whether freeing is necessary, I don't know it, and
      // MSDN does not say.
    }
    else {
      // trim any trailing whitespace
      while (msg[0] && strchr(" \r\n", msg[strlen(msg)-1])) {
        msg[strlen(msg)-1] = 0;
      }

      ret = msg;         // make a copy
      LocalFree(msg);    // evidently FormatMessage uses LocalAlloc ...
    }

#else
  ret = strerror(errno);

#endif

  return ret;
}


void die(char const *context)
{
  string msg = get_last_OS_error_string();
  fflush(stdout);
  fprintf(stderr, "%s: %s: %s\n", proc_label, context, msg.c_str());
  exit(4);
}

int runChild(int port)
{
  proc_label = "child";

  printf("child attempting to connect to port %d\n", port);

  // make a socket for the connection
  int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) {
    die("socket(to connect)");
  }

  // connect
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
    die("connect");
  }

  // wait for activity
  while (true) {
    printf("child waiting for activity\n");

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    int res = select(s+1, &fds, NULL, NULL, NULL /*timeout*/);
    if (res < 0) {
      die("select");
    }
    if (res == 0) {
      printf("select timed out??\n");
      abort();
    }

    // Read a character.
    printf("child calling read()\n");
    char c;
    ssize_t len = recv(s, &c, 1, 0);
    if (len < 0) {
      die("read");
    }
    if (len == 0) {
      printf("child detected connection closure\n");
      break;
    }
    printf("child received character: %c\n", c);
  }

  // close my end
  if (closesocket(s) < 0) {
    die("closesocket");
  }

  printf("child terminating\n");
  return 0;
}


int main(int argc, char **argv)
{
  init_sockets();

  // set to true when we should apply the FD_CLOEXEC fix
  bool fix = false;

  if (argc >= 3) {
    if (0==strcmp(argv[1], "child")) {
      return runChild(atoi(argv[2]));
    }
    else if (0==strcmp(argv[1], "fix")) {
      printf("will use FD_CLOEXEC fix\n");
      fix = true;
    }
    else {
      printf("bad argument: %s\n", argv[1]);
      return 2;
    }
  }

  // create the listen socket
  int listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket < 0) {
    die("socket(to listen)");
  }

  // possibly set FD_CLOEXEC
  if (fix) {
#ifdef _WIN32
    if (!SetHandleInformation((HANDLE)listenSocket, HANDLE_FLAG_INHERIT, 0)) {
      die("SetHandleInformation");
    }
#else
    int prevFlags = fcntl(listenSocket, F_GETFD, 0);
    if (prevFlags < 0) {
      die("fcntl(F_GETFD)");
    }
    if (fcntl(listenSocket, F_SETFD, prevFlags | FD_CLOEXEC) < 0) {
      die("fcntl(F_SETFD)");
    }
#endif
  }

  // bind it to a free port
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0;
  if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
    die("bind");
  }

  // what port did we get?
  socklen_t socklen = sizeof(addr);
  if (getsockname(listenSocket, (sockaddr*)&addr, &socklen) < 0) {
    die("getsockname");
  }
  int port = ntohs(addr.sin_port);

  // listen for connection
  if (listen(listenSocket, 5) < 0) {
    die("listen");
  }

  printf("parent listening on port %d\n", port);

  // NOTE: Listen socket will be inherited by child.

#ifdef _WIN32
  // for diagnostic purposes, arrange to inherit stdin/out/err
  HANDLE stdInputHandle = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE stdOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE stdErrorHandle = GetStdHandle(STD_ERROR_HANDLE);

  // Now create the child process.
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;

  // Set up members of the PROCESS_INFORMATION structure.
  ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

  // Set up members of the STARTUPINFO structure.
  ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.hStdInput = stdInputHandle;
  siStartInfo.hStdOutput = stdOutputHandle;
  siStartInfo.hStdError = stdErrorHandle;

  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  char cmdlinebuf[100];
  sprintf(cmdlinebuf, "./listen-fork child %d", port);
  if (!CreateProcess("./listen-fork.exe", 
                     cmdlinebuf,  
                     NULL,  // lpProcessAttributes
                     NULL,  // lpThreadAttributes
                     TRUE,  // bInheritHandles (system_windows.cpp does this)
                     0,     // dwCreationFlags
                     NULL,  // environment
                     NULL,  // lpCurrentDirectory
                     &siStartInfo,
                     &piProcInfo)) {
    die("CreateProcess");
  }

#else
  // fork a child
  pid_t childPid = fork();
  if (childPid < 0) {
    die("fork");
  }

  if (childPid == 0) {
    // In the child.

    // Exec this same program from the start.
    char portbuf[20];
    sprintf(portbuf, "%d", port);
    execl("./listen-fork", "./listen-fork", "child", portbuf, NULL);
    die("exec");
  }
#endif

  // In the parent.

  printf("parent sleeping\n");
  Sleep(1000);

  // This is the key to reproducing the deadlock:
  printf("parent terminating WITHOUT accepting the connection\n");
  return 0;
}


// EOF
