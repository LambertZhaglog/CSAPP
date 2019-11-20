#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
void *thread(void *vargp);
int main(int argc, char **argv)
{
  int listenfd,*connfdp;
  socklen_t clientlen;
  char clientHost[MAXLINE],clientPort[MAXLINE];
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  /* parse the command line */
  if(argc!=2){
    fprintf(stderr,"usage: %s <port.\n",argv[0]);
    exit(1);
  }
  /* opend listen */
  listenfd=Open_listenfd(argv[1]);
  while(1){
    /* accepte a connection */
    clientlen=sizeof(clientaddr);
    connfdp=Malloc(sizeof(int));
    *connfdp=Accept(listenfd,(SA*)&clientaddr,&clientlen);
    Getnameinfo((SA *) &clientaddr,clientlen,clientHost,MAXLINE,clientPort,MAXLINE,0);
    printf("proxy >Accepted connection from (%s, %s)\n",clientHost,clientPort);
    Pthread_create(&tid,NULL,thread,connfdp);
  }
  return 0;
}


/*
 * proxy as server accept a connection from client,
 * doit complete the work of parse the client's request and request server
 * and response the client
 */
void doit(int connfd){
  char buf[MAXLINE], method[MAXLINE],uri[MAXLINE],version[MAXLINE];
  rio_t rio;//rio as proxy connect with client

  /* read request line and header */
  Rio_readinitb(&rio,connfd);
  if(!Rio_readlineb(&rio,buf,MAXLINE))
    return;
  //  printf("proxy> %s",buf);
  sscanf(buf,"%s %s %s ",method,uri,version);
  if(strcasecmp(method,"GET")){
    clienterror(connfd,method,"501","Not Implemented",
		"Proxy does not implement this method");
    return;
  }
  //parse the uri to get hostname,port and content
  //much thing shound not consider about
  //build the minimume system that can test first
  char serverName[MAXLINE],serverPort[MAXLINE],content[MAXLINE];
  char *p;
  /* I suppose the uri in the right format */
  p=index(uri,'/');
  p=p+2;
  strcpy(serverName,p);
  if((p=index(serverName,':'))==NULL){
    p=index(serverName,'/');
    strcpy(content,p);
    *p='\0';
    strcpy(serverPort,"80");
  }else{
    strcpy(serverPort,p+1);
    *p='\0';
    p=index(serverPort,'/');
    strcpy(content,p);
    *p='\0';
  }

  /* connect to the web server */
  int clientfd;
  rio_t rios;
  clientfd=Open_clientfd(serverName,serverPort);
  Rio_readinitb(&rios,clientfd);
  /* request */
  sprintf(buf,"GET %s HTTP/1.0\r\n",content);
  Rio_writen(clientfd,buf,strlen(buf));
  
  /* request header */
  char hdname[MAXLINE],hddata[MAXLINE];
  Rio_readlineb(&rio,buf,MAXLINE);
  int ifhost=0;
  while(strcmp(buf,"\r\n")){
    strcpy(hdname,buf);
    p=index(hdname,':');
    strcpy(hddata,p+1);
    *p='\0';
    //    printf("proxy> hdname= %s\n",hdname);
    if(strcmp(hdname,"Host")==0){
      ifhost=1;
      Rio_writen(clientfd,buf,strlen(buf));
    }else if((strcmp(hdname,"Connect")!=0)||(strcmp(hdname,"User-Agent")!=0)||(strcmp(hdname,"Proxy-Connection")!=0)){
      //nothing
    }else{
      Rio_writen(clientfd,buf,strlen(buf));
    }
    rio_readlineb(&rio,buf,MAXLINE);
  }
  if(ifhost==0){
    sprintf(buf,"Host: %s\r\n",serverName);
    Rio_writen(clientfd,buf,strlen(buf));
  }
  sprintf(buf,"%sConnect: close\r\nProxy-Connection: close\r\n\r\n",user_agent_hdr);
  Rio_writen(clientfd,buf,strlen(buf));
  
  /* transfer the response from server to client */
  Rio_readlineb(&rios,buf,MAXLINE);
  Rio_writen(connfd,buf,strlen(buf));
  Rio_readlineb(&rios,buf,MAXLINE);
  int contentlength=0;
  while(strcmp(buf,"\r\n")!=0){
    strcpy(hdname,buf);
    p=index(hdname,':');
    strcpy(hddata,p+1);
    *p='\0';
    if(strcmp(hdname,"Content-length")==0){
      sscanf(hddata,"%d",&contentlength);
    }
    Rio_writen(connfd,buf,strlen(buf));
    Rio_readlineb(&rios,buf,MAXLINE);
  }
  Rio_writen(connfd,buf,strlen(buf));//writen \r\n
  int readlen=0;
  //  printf("proxy> contentlength= %d\n",contentlength);
  while(contentlength>0){
    readlen=Rio_readnb(&rios,buf,contentlength>MAXLINE?MAXLINE:contentlength);
    Rio_writen(connfd, buf,readlen);
    contentlength-=readlen;
  }
  Close(clientfd);
  
}
/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Web proxy</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

void *thread(void *vargp){
  int connfd=*((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}
