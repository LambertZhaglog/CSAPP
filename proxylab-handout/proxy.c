#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


typedef struct object{
  char *url;//point to a string that represent an URL 
  int lru;//indicate the times that this object not used
  int size;//indicate the size of this response include the response header and response content;
  char *response;//the response object,include header and content
  struct object *next;//point to the next object, to construct a linklist.
  sem_t mutex,w;//to handle reader-writer issue
  int readcnt;//to handle reader-writer issue
} Object;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
int totalcache=0;//the allocated space for cache
sem_t mtxtc;// for mutex of totalcache
Object *head=NULL;//the header of the object linklist

void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
void *thread(void *vargp);
void requestheader(char *uri,char *serverName,char *serverPort,char *content);
void request(rio_t *rio,char *buf,int clientfd,char *serverName);
void response(int connfd,int clientfd,char *buf,char *url);
void freeObject(Object *p);//free the the memory allocated for the object p point to
void eatupheader(rio_t *rio);
Object *searchurl(char *url);
Object *searchevit();
void *addlist(void *xx);
void returncached(int connfd,Object *cached,rio_t *rio);
void response_depratched(int connfd,int clientfd,char *buf);
    
int main(int argc, char **argv)
{
  int listenfd,*connfdp;
  socklen_t clientlen;
  char clientHost[MAXLINE],clientPort[MAXLINE];
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  /*supplement for part C */
  Sem_init(&mtxtc,0,1);
  head=NULL;
  
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
 * handle the request header and get url
 */

void requestheader(char *uri,char *serverName,char *serverPort,char *content){
  //parse the uri to get hostname,port and content
  //much thing shound not consider about
  //build the minimume system that can test first
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
}
/*
 * transfer the request from the client to server
 */
void request(rio_t *rio,char *buf,int clientfd,char *serverName){
  char hdname[MAXLINE],hddata[MAXLINE];
  Rio_readlineb(rio,buf,MAXLINE);
  int ifhost=0;
  char *p;
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
    rio_readlineb(rio,buf,MAXLINE);
  }
  if(ifhost==0){
    sprintf(buf,"Host: %s\r\n",serverName);
    Rio_writen(clientfd,buf,strlen(buf));
  }
  sprintf(buf,"%sConnect: close\r\nProxy-Connection: close\r\n\r\n",user_agent_hdr);
  Rio_writen(clientfd,buf,strlen(buf));
}
/*
 * proxy as server accept a connection from client,
 * doit complete the work of parse the client's request and request server
 * and response the client
 */
void doit(int connfd){
  char buf[MAXLINE], method[MAXLINE],uri[MAXLINE],version[MAXLINE];
  rio_t rio;//rio as proxy connect with client
  char serverName[MAXLINE],serverPort[MAXLINE],content[MAXLINE];
  /* read request line and header */
  Rio_readinitb(&rio,connfd);
  if(!Rio_readlineb(&rio,buf,MAXLINE))
    return;

  /* parse the request header */
  sscanf(buf,"%s %s %s ",method,uri,version);
  if(strcasecmp(method,"GET")){
    clienterror(connfd,method,"501","Not Implemented",
		"Proxy does not implement this method");
    return ;
  }
  char *url=(char *)Malloc(strlen(uri)+1);
  *url='\0';
  strcpy(url,uri);
  Object *cached=searchurl(uri);
  //Object *cached=NULL;
  if(cached==NULL){
  
    requestheader(uri,serverName,serverPort,content);
  
    /* connect to the web server */
    int clientfd;
    clientfd=Open_clientfd(serverName,serverPort);
    /* request */
    sprintf(buf,"GET %s HTTP/1.0\r\n",content);
    Rio_writen(clientfd,buf,strlen(buf));
  
    /* request header */
    request(&rio,buf,clientfd,serverName);

    /* transfer the response from server to client */
    response(connfd,clientfd,buf,url);
    //response_depratched(connfd,clientfd,buf);
  
    Close(clientfd);
  }else{
    returncached(connfd,cached,&rio);
  }
  
}
void response_depratched(int connfd,int clientfd,char *buf){
  rio_t rios; 
  Rio_readinitb(&rios,clientfd);
  Rio_readlineb(&rios,buf,MAXLINE);
  Rio_writen(connfd,buf,strlen(buf));
  Rio_readlineb(&rios,buf,MAXLINE);
  char hdname[MAXLINE],hddata[MAXLINE];
  char *p;
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
}
void response(int connfd,int clientfd,char *buf,char *url){
  rio_t rios;
  char *newres=(char *)Malloc(MAX_OBJECT_SIZE);//new response space;
  *newres='\0';   
  Rio_readinitb(&rios,clientfd);
  Rio_readlineb(&rios,buf,MAXLINE);
  // Rio_writen(connfd,buf,strlen(buf));
  strcat(newres,buf);// Rio_writen(connfd,buf,strlen(buf));
  Rio_readlineb(&rios,buf,MAXLINE);
  char hdname[MAXLINE],hddata[MAXLINE];
  char *p;
  int contentlength=0;
  while(strcmp(buf,"\r\n")!=0){
    strcpy(hdname,buf);
    p=index(hdname,':');
    strcpy(hddata,p+1);
    *p='\0';
    if(strcmp(hdname,"Content-length")==0){
      sscanf(hddata,"%d",&contentlength);
    }
    // Rio_writen(connfd,buf,strlen(buf));
    strcat(newres,buf);//Rio_writen(connfd,buf,strlen(buf));
    Rio_readlineb(&rios,buf,MAXLINE);
  }
  // Rio_writen(connfd,buf,strlen(buf));
  strcat(newres,buf);//Rio_writen(connfd,buf,strlen(buf));//writen \r\n
  int size=strlen(newres)+contentlength;
  char *tmp=(char *)newres;
  tmp+=strlen(newres);
  if(size<MAX_OBJECT_SIZE){
    // Rio_writen(connfd,newres,strlen(newres));
    int readlen=0;
    while(contentlength>0){
      readlen=Rio_readnb(&rios,buf,contentlength>MAXLINE?MAXLINE:contentlength);
      contentlength-=readlen;
      memcpy(tmp,buf,readlen);  // strcat(newres,buf);
      tmp+=readlen;
      // Rio_writen(connfd,buf,readlen);
    }
    Rio_writen(connfd,newres,tmp-newres);
   
    Object *obj=(Object *)Malloc(sizeof(Object));//new object
    obj->url=url;
    obj->response=newres;
    obj->size=size+1;
    obj->lru=1;
    obj->readcnt=0;
    pthread_t tid;
    Pthread_create(&tid,NULL,addlist,obj);   
    Pthread_detach(tid);
  }else{
    Rio_writen(connfd,newres,strlen(newres));
    free(newres);
    
    int readlen=0;
    //  printf("proxy> contentlength= %d\n",contentlength);
    while(contentlength>0){
      readlen=Rio_readnb(&rios,buf,contentlength>MAXLINE?MAXLINE:contentlength);
      Rio_writen(connfd, buf,readlen);
      contentlength-=readlen;
    }
  }
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

void freeObject(Object *p){
  free(p->url);
  free(p->response);
  free(p);
}

Object *searchurl(char *url){
  Object *result=NULL;
  Object *p;
  P(&mtxtc);
  p=head;
  V(&mtxtc);
  for(;p!=NULL;p=p->next){
    if((result==NULL)&&(strcmp(url,p->url)==0)){
      result=p;
      p->lru=1;
    }else{
      p->lru=p->lru+1;
    }
  }
  
  return result;
}

Object *searchevit(){
  Object *result=head;
  int max=head->lru;
  for(Object *p=head;p->next!=NULL;p=p->next){
    if(p->next->lru>max){
      result=p;
      max=p->next->lru;
    }
  }
  return result;
}

void eatupheader(rio_t *rio){
  char buf[MAXLINE];
  Rio_readlineb(rio,buf,MAXLINE);
  
  while(strcmp(buf,"\r\n")!=0){
    Rio_readlineb(rio,buf,MAXLINE);
  }
 
  return;
}

void returncached(int connfd,Object *cached,rio_t *rio){ 
  //eat up all the requset header
  eatupheader(rio);
  //lock from be evict
  
  P(&(cached->mutex));
  (cached->readcnt)++;
  if(cached->readcnt==1){
    P(&(cached->w));
  }
  V(&(cached->mutex));
  

  /* critical section */
  Rio_writen(connfd,cached->response,cached->size);

  //unlock
  P(&(cached->mutex));
  (cached->readcnt)--;
  if(cached->readcnt==0)
    V(&(cached->w));
  V(&(cached->mutex));
  
}

void *addlist(void *xx){
  Object *obj=(Object *)xx;
  Object *tmp,*tmp2;
  
  Sem_init(&obj->mutex,0,1);
  Sem_init(&obj->w,0,1);
  
  P(&mtxtc);
  totalcache+=obj->size;
  V(&mtxtc);
  while(totalcache>MAX_CACHE_SIZE){
    tmp=searchevit();
    if(tmp!=head||(tmp->lru<tmp->next->lru)){
      tmp2=tmp->next;
      tmp->next=tmp2->next;
      P(&mtxtc);
      totalcache-=tmp2->size;
      V(&mtxtc);

      P(&tmp2->w);
      free(tmp2);
    }else{
      tmp2=head;
      head=tmp2->next;
      P(&mtxtc);
      totalcache-=tmp2->size;
      V(&mtxtc);

      P(&tmp2->w);
      free(tmp2);
    }
  }
  
  P(&mtxtc);
  obj->next=head;
  //obj->next=NULL;
  head=obj;
  V(&mtxtc);
      
  return NULL;
}
