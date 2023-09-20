#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";

void do_it(int fd);
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host);
void do_response(int p_connfd, int p_clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
//이 함수는 쓰지를 않는데 선언을 안 해두면 채점이 안 된다...
int parse_responsehdrs(rio_t *rp, int length);

int main(int argc, char **argv) {
  int listenfd, p_connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while(1) {
    clientlen = sizeof(clientaddr);
    //client의 connection request를 Accept. p_connfd = proxy의 connfd
    p_connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    //display GET request
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    do_it(p_connfd);
    Close(p_connfd);
  }

  return 0;
}
//HTTP request line from client to proxy
//-> GET http://www.google.com:80/index.html HTTP/1.1

//parsing
//host: www.google.com
//port: 80
//uri_ptos(proxy to server): /index.html

//HTTP request line from proxy to server
//-> GET /index.html HTTP/1.0

void do_it (int p_connfd) {
  int p_clientfd;
  
  char buf[MAXLINE], host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char uri_ptos[MAXLINE];

  rio_t rio;

  //client가 보낸 req header에서 method, uri, version을 가져옴
  //GET HTTP://www.google.com:80/index.html HTTP/1.1
  //rio 버퍼와 fd(proxy의 connfd)를 연결
  Rio_readinitb(&rio, p_connfd);
  //rio(proxy의 connfd)의 req line을 모두 buffer로 옮김
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers to proxy: \n");
  printf("%s", buf);
  //버퍼에서 문자열 3개를 읽어와서 각각 method, uri, version에 저장
  sscanf(buf, "%s %s %s", method, uri, version); 

  if ((strcasecmp(method, "GET"))) {
    printf("Proxy does not implement this method.");
    return;
  }
  
  //parse the uri to get hostname, filepath, port
  //parsing the informations to send to end server
  parse_uri(uri, uri_ptos, host, port);

  //connection to end server
  //proxy의 clientfd connection 시작
  p_clientfd = Open_clientfd(host, port);
  //p_clientfd에 request headers 저장과 동시에 server의 connfd에 쓰기
  do_request(p_clientfd, method, uri_ptos, host);
  do_response(p_connfd, p_clientfd);
  Close(p_clientfd);
}

//proxy -> server
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host) {
  char buf[MAXLINE];
  printf("Request headers to server: \n");
  printf("%s %s %s\n", method, uri_ptos, new_version);

  //read request headers
  sprintf(buf, "GET %s %s\r\n", uri_ptos, new_version); //GET /index.html HTTP/1.0
  sprintf(buf, "%sHost: %s\r\n", buf, host); //Host: www.google.com
  sprintf(buf, "%s%s", buf, user_agent_hdr); //User-Agent: ~~
  sprintf(buf, "%sConnections: close\r\n", buf); //Connections: close
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf); //Proxy-Connection: close
  
  //buf에서 p_clientfd로 strlen(buf)바이트 전송
  Rio_writen(p_clientfd, buf, (size_t)strlen(buf));
}

//server -> proxy
void do_response(int p_connfd, int p_clientfd) {
  char buf[MAX_CACHE_SIZE];
  ssize_t n;
  rio_t rio;

  Rio_readinitb(&rio, p_clientfd);
  //buffer의 내용을 전부 읽고
  n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);
  //전부 쓴다.
  Rio_writen(p_connfd, buf, n);
}

int parse_uri(char *uri, char *uri_ptos, char *host, char *port) {
  char *ptr;

  if (!(ptr = strstr(uri, "://"))) {
    return -1;
  }
  ptr += 3;
  strcpy(host, ptr); //host = www.google.com:80/index.html

  if ((ptr = strchr(host, ':'))) {
    *ptr = '\0';
    ptr += 1;
    strcpy(port, ptr); //port = 80/index.html
  } else {
    if ((ptr = strchr(host, '/'))) {
      *ptr = '\0';
      ptr += 1;
    }
    strcpy(port, "80");
  }

  if ((ptr = strchr(port, '/'))) { //port = 80/index.html
    *ptr = '\0';
    ptr += 1;
    strcpy(uri_ptos, "/"); //uri_ptos = /
    strcat(uri_ptos, ptr); //uri_ptos = /index.html
  } else {
    strcpy(uri_ptos, "/");
  }

  return 0;
}