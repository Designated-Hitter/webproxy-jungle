/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char* method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char* method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  //listen socket 과 connection socket 선언
  int listenfd, connfd;
  //connection을 위한 hostname 과 port 선언
  char hostname[MAXLINE], port[MAXLINE]; 
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
	//listen socket open, 인자로 포트 번호를 넘겨줌
  //Open_listenfd는 요청받은 준비가 된 듣기 식별자를 리턴(listenfd)
  listenfd = Open_listenfd(argv[1]); 
	
    //무한루프 서버
  while (1) {
  	//accept 함수 인자에 넣기 위한 주소 길이 계산
    clientlen = sizeof(clientaddr);
    //반복적으로 연결 요청을 접수
    //accept함수 (듣기 식별자, 소켓주소구조체의 주소, 주소의 길이)
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    //getnameinfo함수: 소켓주소구조체를 호스트주소, 포트 번호로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // transaction 실행
    Close(connfd);  // 연결 닫기
  }
}

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  //rio_readlineb를 위해 rio_t 타입 구조체의 읽기 버퍼를 선언
  rio_t rio;

  //req line, headers 읽기
  //&rio 주소를 가지는 읽기 버퍼와 식별자 connfd 연결
  Rio_readinitb(&rio, fd);
  //버퍼에서 읽은 것이 담겨있음
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  //"GET / HTTP/1.1"
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); //버퍼에서 자료형을 읽음
  //GET이 아닌 메소드를 req했다면 error를 return 
  if ((strcasecmp(method, "GET")) && (strcasecmp(method, "HEAD"))) { //숙제
    clienterror(fd, method, "501", "Not implementd", "Tiny does not implement this method.");
    return;
  }
  
  read_requesthdrs(&rio); //req headers 읽기

  //GET req로부터 URI parse
  //URI를 parse해서 파일 이름, 비어있을수 있는 CGI인자 스트링으로 분석
  //정적/동적 컨텐츠인지 판단
  is_static = parse_uri(uri, filename, cgiargs); 
  printf("uri: %s, filename: %s, cgiargs: %s \n", uri, filename, cgiargs);
  //file이 disk에 없다면 error를 return
  //stat은 파일 정보를 불러오고 sbuf에 내용을 적어줌. ok -> 0, error -> -1
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "tiny couldn't find this file.");
    return;
  }
  
  if (is_static) { //정적 콘텐츠 제공
    //(읽을 수 있는) 일반 파일인지, 파일에 대한 읽기 권한을 가지고 있는지 판단
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file."); 
      //못 읽으면 error return
      return;
    }
    //static content 제공
    serve_static(fd, filename, sbuf.st_size, method);
  } else { //동적 콘텐츠 제공
    //(읽을 수 있는) 일반 파일인지, CGI 프로그램을 실행할 수 없으면 error return
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program.");
      return;
    }
    //dynamic content 제공
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];
  //브라우저 사용자에게 에러를 설명하는 res 본체에 HTML도 함께 보낸다.
  //HTML res는 본체에서 컨텐츠의 크기와 타입을 나타내야 하기 때문에,
  //HTML 컨텐츠를 한 개의 스트링으로 만들고 그 길이도 잼.
  //이는 sprintf를 통해 body는 인자에 스택되어 하나의 긴 스트링으로 저장된다.
  
  //HTTP res body 만들기
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor =""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  //HTTP res 출력하기
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

//tiny 웹서버는 req header를 읽고 무시함.
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;
  //정적 콘텐츠: URI에 /cgi-bin을 포함하는 경로가 없다.
  if (!strstr(uri, "cgi-bin")) { 
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    
    //uri 문자열 끝이 / 인 경우 home.html을 filename에 붙여줌
    if (uri[strlen(uri)-1] == '/') {
      strcat (filename, "home.html");
    }

    return 1;

  } else { //동적 콘텐츠
    ptr = index(uri, '?');
	
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }

    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char* method) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //client에게 res headers 보내기
  //접미어를 통해 filetype 결정
  get_filetype(filename, filetype);
  //client 에게 req line, req headers 보내기
  //데이터를 보내기 전에 버퍼로 임시로 가지고 있다.
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  //서버에 출력하기
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") != 0) { //method 가 HEAD일 때 body를 보내지 않기 위함
    //client에게 res body 보내기
    //읽을 수 있는 파일로 열기(open read only)
    //숙제: malloc으로 바꾸기
    srcfd = Open(filename, O_RDONLY, 0);
    //malloc으로 가상 메모리 할당
    srcp = (char *)malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    free(srcp);
    
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    // Close(srcfd);
    // Rio_writen(fd, srcp, filesize);
    // Munmap(srcp, filesize);
  }
}

//filename 으로부터 file 의 형식을 알아내는 함수
void get_filetype(char *filename, char *filetype) { 
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mp4")) { //숙제
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
  char buf[MAXLINE], *emptylist[] = {NULL};

  //HTTP res의 first part return
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (strcasecmp(method, "HEAD") != 0) { //method 가 HEAD일 때 body를 보내지 않기 위함
    
    if (Fork() == 0) { //동적 콘텐츠를 실행하고 결과를 return할 자식 프로세스를 포크
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); //redirect stdout to client
    Execve(filename, emptylist, environ); //CGI program 실행
    }

    Wait(NULL); //자식 프로세스가 실행되고 결과를 출력하고 종료될 때까지 기다림
  }

  
}