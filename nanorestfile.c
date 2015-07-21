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
//#  include <windows.h>
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
#define BUFSIZE 8096

void failure(char *type, int socketfd) {
	char buffer[128];
	printf(" - FAILED");
	sprintf(buffer,"HTTP/1.1 type\r\nServer: nanorestfile\r\nConnection: close\r\n\r\n", type);
	send(socketfd, buffer, strlen(buffer), 0);
}

void respond(int len, char *mime, int socketfd) {
	char buffer[128];
  sprintf(buffer,"HTTP/1.1 200 OK\r\nServer: nanorestfile\r\nContent-Length: %ld\r\n", len);
	send(socketfd, buffer, strlen(buffer), 0);
  sprintf(buffer,"Connection: close\r\nContent-Type: text/%s\r\n\r\n", mime);
	send(socketfd, buffer, strlen(buffer), 0);
}

void getf(char *uri, int socketfd) {
	int file_fd;
	long len;
	char buffer[BUFSIZE + 1], *mime;
	if((file_fd = open(uri, O_RDONLY)) == -1) {
		failure("404 Not found", socketfd); return;
	}
	if ((mime = strchr(uri, '.'))) mime++; else mime = "text";
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	      (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
	
	respond(len, mime, socketfd);
	while (	(len = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		send(socketfd, buffer, len, 0);
	}
	close(file_fd);
}

void putf(char *uri, int socketfd) {
	int file_fd, len;
	char *body = uri + strlen(uri) + 1;
  do { body = strchr(body, '\r') + 1; }
	while (body && strcmp(body, "\n\r\n"));
	len = strlen(body);
	if ((file_fd = open(uri, O_WRONLY)) == -1 || write(file_fd, body, len) != len) {
		failure("500 Write failed", socketfd); return;
	}
	close(file_fd);
	respond(0, "text", socketfd);
}

void postf(char *uri, int socketfd) {
  failure("405 POST not implemented", socketfd);	
}

void deletef(char *uri, int socketfd) {
	if (unlink(uri))
	  failure("500 Delete failed", socketfd);
	else
	  respond(0, "text", socketfd);
}

void getd(char *uri, int socketfd) {
	DIR *dp; char buffer[BUFSIZE], *bp = buffer;
  struct dirent *ep; int len;

  if ((dp = opendir("./")) != NULL)
    {
      while (ep = readdir(dp)) {
        strcpy(ep->d_name, bp);
				bp += strlen(bp);
				*bp++ = '\n';
      }
      closedir(dp);
			len = bp - buffer;
			respond(len, "text", socketfd);
			send(socketfd, buffer, len, 0);
    }
  else
    puts ("Couldn't open the directory.");

}

void putd(char *uri, int socketfd) {
  failure("405 PUT not implemented", socketfd);	
}

void postd(char *uri, int socketfd) {
	if (mkdir(uri, S_IRUSR | S_IWUSR))
	  failure("500 mkdir failed", socketfd);
	else
	  respond(0, "text", socketfd);
}

void deleted(char *uri, int socketfd) {
	if (rmdir(uri))
	  failure("500 Delete failed", socketfd);
	else
	  respond(0, "text", socketfd);
}

void request(int socketfd) {
	long len;
	char method[262144], *uri, *rest;

	len = recv(socketfd, method, sizeof(method), 0);
	if(len == 0 || len == -1) return;
	method[len] = 0;
	uri = strchr(method, ' '); *uri = 0; uri += 2;
	rest = strchr(rest, ' '); *rest = 0;
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

int main(int argc, char **argv) {
# ifdef WIN32
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR) {
		printf("Error creating socket.\n");
		WSACleanup();
		exit(1);
	}
#endif
	int port = (argc > 1) ? atoi(argv[1]) : 2020;
	int i, pid, listenfd, socketfd;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	socklen_t cli_len = sizeof(cli_addr);
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Socket instance creation error\n"); exit(1);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	serv_addr.sin_port = htons(port);
	if(bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("Socket binding error"); exit(1);
	}
	if(listen(listenfd, 64) < 0) { printf("Socket listen error\n"); exit(1); }
	printf("Nano-Rest-File listening on port %d\n", port);
	while(1) {
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
			printf("\naccept failed");
		else {
			request(socketfd);
    	closesocket(socketfd);
		}
	}
}
