#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef WIN32
#  include <winsock2.h>
   typedef int socklen_t;
#  define mkdir(fn, mode) mkdir(fn)
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  define closesocket(fd) close(fd)
#  define recv(fd, rq, sz, md) read(fd, rq, sz)
#  define send(fd, content, sz, md) write(fd, content, sz)
#endif
#define BUFSIZE 50000

/* Let the client know when we are have failed to do what is asked */
void failure(char *type, int socketfd) {
  char buffer[128];
  printf(" - FAILED");
  sprintf(buffer,"HTTP/1.1 %s\r\nServer: nanorestfile\r\nConnection: close\r\n\r\n", type);
  send(socketfd, buffer, strlen(buffer), 0);
}

/* Start the response to the client - preparing for body and close */
void respond(int len, char *mime, int socketfd) {
  char buffer[128];
  sprintf(buffer,"HTTP/1.1 200 OK\r\nServer: nanorestfile\r\nContent-Length: %d\r\n", len);
  send(socketfd, buffer, strlen(buffer), 0);
  sprintf(buffer,"Connection: close\r\nContent-Type: text/%s\r\n\r\n", mime);
  send(socketfd, buffer, strlen(buffer), 0);
}

/* HTTP GET /uri returns the file contents in the body */
void getf(char *uri, int socketfd) {
  int file_fd;
  long len;
  char buffer[BUFSIZE + 1], *mime;
  if((file_fd = open(uri, O_RDONLY)) == -1) {
    failure("404 Not found", socketfd); return;
  }
  /* Shortcut uses the file extension as the mime type - works for javascript and css at least */
  if ((mime = strchr(uri, '.'))) mime++; else mime = "text";
  
  len = read(file_fd, buffer, BUFSIZE);
  respond(len, mime, socketfd);
  send(socketfd, buffer, len, 0);
  close(file_fd);
}

/* HTTP PUT /uri Client can create a new file or change the contents of an existing one */
void putf(char *uri, int socketfd) {
  int len; FILE *fp;
  char *body = uri + strlen(uri) + 1;
  do { body = strchr(body, '\r') + 1; }
  while (body && strncmp(body, "\n\r\n", 3));
  len = strlen(body);
  if (!(fp = fopen(uri, "w")) || fprintf(fp, body, len) != len) {
    failure("500 Write failed", socketfd); return;
  }
  fclose(fp);
  respond(0, "text", socketfd);
}

/* HTTP POST /uri - not used */
void postf(char *uri, int socketfd) {
  failure("405 POST not implemented", socketfd);  
}

/* HTTP DELETE /uri - delete the file */
void deletef(char *uri, int socketfd) {
  if (unlink(uri))
    failure("500 Delete failed", socketfd);
  else
    respond(0, "text", socketfd);
}

int isDirectory(char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0) return 0;
   return S_ISDIR(statbuf.st_mode);
}

/* HTTP GET /uri/ Return directory contents - / at end of directories */
void getd(char *uri, int socketfd) {
  DIR *dp; char buffer[BUFSIZE], *bp = buffer, *here;
  struct dirent *ep; int len;

  if (!*uri) uri = "./"; /* can't send ./ from browser */
  if ((dp = opendir(uri)) != NULL) {
    while ((ep = readdir(dp))) {
      if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, "..")) {
        strcpy(here = bp, uri);
        strcat(bp, ep->d_name);
        bp += strlen(bp);
        if (isDirectory(here)) *bp++ = '/';
        *bp++ = '\n';
      }
    }
    closedir(dp);
    len = bp - buffer;
    respond(len, "text", socketfd);
    send(socketfd, buffer, len, 0);
  } else {
    puts ("Couldn't open the directory.");
  }

}

/* HTTP PUT /uri/ not supported */
void putd(char *uri, int socketfd) {
  failure("405 PUT not implemented", socketfd);  
}

/* HTTP POST /uri/ creates a new directory inside */
void postd(char *uri, int socketfd) {
  if (mkdir(uri, S_IRUSR | S_IWUSR))
    failure("500 mkdir failed", socketfd);
  else
    respond(0, "text", socketfd);
}

/* HTTP DELETE /uri/ will delete a directory if it is empty */
void deleted(char *uri, int socketfd) {
  if (rmdir(uri))
    failure("500 Delete failed", socketfd);
  else
    respond(0, "text", socketfd);
}

/* Every time the socket makes a connection it comes here for action */
void request(int socketfd) {
  long len; /* can handle writing files up to 256kb */
  char method[262144], *uri, *rest;

  len = recv(socketfd, method, sizeof(method) - 1, 0);
  if (len == 0 || len == -1) return;
  method[len] = 0;
  uri = strchr(method, ' '); *uri = 0; uri += 2;
  rest = strchr(uri, ' '); *rest = 0;
  printf("\n%s %s", method, uri);
  if (!*uri || uri[strlen(uri) - 1] == '/') {
    if (strcmp(method, "GET") == 0) getd(uri, socketfd);
    if (strcmp(method, "PUT") == 0) putd(uri, socketfd);
    if (strcmp(method, "POST") == 0) postd(uri, socketfd);
    if (strcmp(method, "DELETE") == 0) deleted(uri, socketfd);
  } else {
    if (strcmp(method, "GET") == 0) getf(uri, socketfd);
    if (strcmp(method, "PUT") == 0) putf(uri, socketfd);
    if (strcmp(method, "POST") == 0) postf(uri, socketfd);
    if (strcmp(method, "DELETE") == 0) deletef(uri, socketfd);
  }
}

/* usage: nanorestfile.??? [port] [any] - defaults to 2020
 * If a any is placed as the second argument then the server can be accessed
 * from anywhere on the subnet. By default it is limited to localhost
 */
int main(int argc, char **argv) {
# ifdef WIN32
  WSADATA wsadata;
  if (WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR) {
    printf("Error creating socket.\n");
    WSACleanup();
    exit(1);
  }
# endif
  int port = (argc > 1) ? atoi(argv[1]) : 2020;
  int inaddr = (argc > 2) ? INADDR_ANY : INADDR_LOOPBACK;
  int i, pid, listenfd, socketfd;
  static struct sockaddr_in cli_addr;
  socklen_t cli_len = sizeof(cli_addr);
  static struct sockaddr_in serv_addr;

  if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Socket instance creation error\n"); exit(1);
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(inaddr);
  serv_addr.sin_port = htons(port);
  if(bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Socket binding error"); exit(1);
  }
  if(listen(listenfd, 64) < 0) { printf("Socket listen error\n"); exit(1); }
  printf("Nano-Rest-File listening on port %d\n", port);
  while(1) { /* Loop forever until killed or something fails */
    if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
      printf("\naccept failed");
    else {
      request(socketfd);
      closesocket(socketfd);
    }
  }
}
