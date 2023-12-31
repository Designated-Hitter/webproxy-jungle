/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;
  //두 개의 정수 추출
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);
    sscanf(buf, "first=%d", &n1);
    sscanf(p+1, "second=%d", &n2);

  }
  //making response body 
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);
  
  //HTTP response 만들기
  //res headers
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("content-type: text/html\r\n\r\n");
  //res body
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
