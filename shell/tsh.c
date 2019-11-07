/* 
 * tsh - A tiny shell program with job control
 * shell is interface that user can run program, with job control indicate that user can stop, terminate and contine the program run. 
 * <Put your name and login ID here>
 * author: Lambert Zhaglog
 * institute: HUST
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_SLEEP 100000
#define CONVERT(val) (((double)val)/(double)RAND_MAX)

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
  pid_t pid;              /* job PID */
  int jid;                /* job ID [1, 2, ...] */
  int state;              /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);// clear every entry of the struct specified by the arguemtn job  
void initjobs(struct job_t *jobs);// initial the job array jobs
int maxjid(struct job_t *jobs); // return the max allocated job id, job id can be 0xffff,
//but the array jobs can only have 16 job at the same time
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);// add new job to job table,
// return 0 if anything wrong
int deletejob(struct job_t *jobs, pid_t pid); // delete a job from job table,
                                              //return 0 if anything wrong
pid_t fgpid(struct job_t *jobs); // return pid of current foreground job, 0 if no such job
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);// find a job by pid from job list
struct job_t *getjobjid(struct job_t *jobs, int jid); // find a job by jobid from job list
int pid2jid(pid_t pid); // map process id to job id, return 0 if no match
void listjobs(struct job_t *jobs);// list jobs in the job table

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* Here are self added helper function */
pid_t Fork(void);
int Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int Sigemptyset(sigset_t *set);
int Sigaddset(sigset_t *set, int signum);
/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);
  //ano for anotation
  //ano: dup2(int oldfd, int newfd); to make the newfd point to the same file as oldfd
  //ano: the fd of stdout and stderr is 1, 2 respectively

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             /* print help message */
      usage();
      break;
    case 'v':             /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':             /* don't print a prompt */
      emit_prompt = 0;  /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT,  sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
  //ano: signal is to tell the OS, when he find a signal send to this program,
  //ano cnt: he should let the handler executed use this program time.
  //ano cnt: the program and the handler belong to the same process

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler); 

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {

    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    //ano: if read nothing and occur error
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
void eval(char *cmdline) 
{
  // parse the command line
  // handle built-in, foreground, background seperately
  //  foreground use stop the while loop that means stop the eval function, use the waitfg, mainly control the terminal 
  //  built-in process immediately
  // handle the jobs table issue
  // use sigprocmask issue
  // set the child process to a new process group
  // restore the signal handler 
  char *argv[MAXARGS]; /* Argument list execve() */
  int bg; /* Should the job run in bg or fg?*/
  pid_t pid; /* process id */
  sigset_t mask;

  bg=parseline(cmdline,argv);
  if(argv[0]==NULL){
    return; /* ignore empty lines */
  }

  if(!builtin_cmd(argv)){//built-in job process immediately
    Sigemptyset(&mask);
    Sigaddset(&mask,SIGCHLD);
    Sigprocmask(SIG_BLOCK,&mask,NULL);
      
    if((pid=Fork())==0){ /* Child runs user job */
      //  printf("enter child first after fork\n");
      Sigprocmask(SIG_UNBLOCK,&mask,NULL);	
      //      Signal(SIGINT,  SIG_DFL);   /* ctrl-c */
      //      Signal(SIGTSTP, SIG_DFL);  /* ctrl-z */
      //     Signal(SIGCHLD, SIG_DFL);  /* Terminated or stopped child */
      setpgid(0,0);
      if(execve(argv[0],argv,environ)<0){
	printf("%s: Command not found.\n",argv[0]);
	exit(0);
      }
    }else{
      //     printf("entered the parent after fork\n");
      if(!bg){
	if(addjob(jobs, pid, FG,cmdline)==0){
	  app_error("addjob error");
	}
      }else{
	if(addjob(jobs,pid,BG,cmdline)==0){
	  app_error("addjob error");
	}
      }
      if(bg){
	struct job_t *target=getjobpid(jobs,pid);
	printf("[%d] (%d) %s",target->jid,target->pid,target->cmdline);
      }
      Sigprocmask(SIG_UNBLOCK,&mask,NULL);
      /* parent waits for foreground jobs to terminates */
      if(!bg){
	//	printf("start wait fg\n");
	waitfg(pid);
      }
    }
	
      
  }
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
  // quit function just exit
  if(!strcmp(argv[0],"quit")){
    exit(0);
  }else if(!strcmp(argv[0],"&")){
    return 1;
  }else if(!strcmp(argv[0],"jobs")){
    listjobs(jobs);
    return 1;
  }else if((!strcmp(argv[0],"fg"))||(!strcmp(argv[0],"bg"))){
    do_bgfg(argv);
    return 1;
  }
  return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
  int jobid;
  int intpid;
  int ispid=0;
  struct job_t *target;
  
  if(argv[1]==NULL){
    printf("%s command requires PID or %%jobid argument\n",argv[0]);
    return;
  }else{
    if(argv[1][0]=='%'){
      ispid=0;
      if(sscanf(argv[1]+1,"%d",&jobid)==0){
	printf("%s: argument must be PID or %%jobid\n",argv[0]);
	return;
      }
    }else{
      ispid=1;
      if(sscanf(argv[1],"%d",&intpid)==0){
	printf("%s: argument must be PID or %%jobid\n",argv[0]);
	return;
      }
    }
  }
  if(ispid==0){
    if((target=getjobjid(jobs,jobid))==NULL){
      printf("%%%d: No such job\n",jobid);
      return;
    }
  }else{
    if((target=getjobpid(jobs,(pid_t)intpid))==NULL){
      printf("(%d): No such process\n",intpid);
      return;
    }
  }
  //to do, the main issue to handle the fg and bg command
  // send the job a SIGCONT signal, continue run it in foreground or background
  // update jobs table

  sigset_t mask;
  Sigemptyset(&mask);
  Sigaddset(&mask,SIGCHLD);
  Sigprocmask(SIG_BLOCK,&mask,NULL);
  if(kill(-target->pid,SIGCONT)<0){
    unix_error("kill error");
  }
  if(strcmp(argv[0],"bg")==0){
    target->state=BG;
  }else{
    target->state=FG;
  }
  pid_t tmppid=target->pid;

  Sigprocmask(SIG_UNBLOCK,&mask,NULL);
  if(strcmp(argv[0],"fg")==0){
    waitfg(tmppid);
  }else{
    printf("[%d] (%d) %s",target->jid,target->pid,target->cmdline);
  }
  return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
  // just sleep a little time many times, until when it has reaped ? until when, can not judge when to return from this function
  while(pid==fgpid(jobs)){
    // sleep(1);// the parent proces will awark ahead if handle some signal
    // pause();//this system call better
    sleep(0);
  }
  return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
  // update job table
  // reap the terminate job, use the while loop methond to tackle the pending overlapping case
  /*
   *tsh shoudl reap all of its zombine children. if any job terminates because it receives a signal
   * that it didn't catch, then tsh should recognize this event and print a message with the job's 
   * and a description of the offending signal. 
   */
  pid_t pid;
  int status;
  //  printf("pid %d receive child halt\n",getpid());
  while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0){
    
    //       printf("Handler reaped child %d\n",(int)pid);
    if (WIFSTOPPED(status)) {
      getjobpid(jobs,pid)->state=ST;
      printf("Job [%d] (%d) stopped by signal %d\n",pid2jid(pid),pid,WSTOPSIG(status));
    }
    if(WIFSIGNALED(status)){
      printf("Job [%d] (%d) terminated by signal %d\n",pid2jid(pid),pid,WTERMSIG(status));
    }
    if(WIFSIGNALED(status)||WIFEXITED(status))
      deletejob(jobs,pid);//doesn't need the error handling
  }
  if((pid==-1)&&(errno!= ECHILD)){
    unix_error("waitpid error");
  }
  return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
  // send this signal to foreground job, that is to the child process group that run foreground
  // update the jobs table
  sigset_t mask;
  Sigemptyset(&mask);
  Sigaddset(&mask,SIGCHLD);
  Sigprocmask(SIG_BLOCK,&mask,NULL);
  int pid=fgpid(jobs);
  if(pid!=0){

    if(kill(-pid,SIGINT)<0){
      unix_error("kill: transfer ctrl-c error");
    }
    //    printf("send a kill\n");
    getjobpid(jobs,pid)->state=UNDEF;
    // maybe need to block some signal 
  }else{
    ;
  }
  Sigprocmask(SIG_UNBLOCK,&mask,NULL);
  //  if(kill(getppid(),SIGCHLD)<0){
  //   unix_error("kill: tell parent receive a SIGINT not from terminal; error");
  //  }
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
  //same as sigint_handler
  // send foreground job signal
  // update job table
  //  printf("receive stop signal\n");
  sigset_t mask;
  Sigemptyset(&mask);
  Sigaddset(&mask,SIGCHLD);
  Sigprocmask(SIG_BLOCK,&mask,NULL);
  int pid=fgpid(jobs);
  if(pid!=0){
    if(kill(-pid,SIGTSTP)<0){
      unix_error("kill: transfer ctrl-z error");
    }
    //   getjobpid(jobs,pid)->state=ST;
    // maybe need to block some signal 
  }else{
    ;
  }
  Sigprocmask(SIG_UNBLOCK,&mask,NULL);
  return;
}

/*********************
 * End signal handlers
 *********************/
/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;                   /* number of args */
  int bg;                     /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  }//ano: to eliminate the probality of regard the quote string as a series argument
  else {
    delim = strchr(buf, ' ');
  }
 

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    }
    else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;
    
  if (argc == 0)  /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc-1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
  //ano: if bg==0, then should run in the background
}

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
  int i, max=0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
  int i;
    
  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
	nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if(verbose){
	printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs)+1;
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
  int i;
    
  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG: 
	printf("Running ");
	break;
      case FG: 
	printf("Foreground ");
	break;
      case ST: 
	printf("Stopped ");
	break;
      default:
	printf("listjobs: Internal error: job[%d].state=%d ", 
	       i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
  struct sigaction action, old_action;

  action.sa_handler = handler;  
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}

/*********************
 * self added helper function
 *********************/

pid_t Fork(void){
  pid_t pid;
  static struct timeval time;
  unsigned bool,secs;

  gettimeofday(&time,NULL);
  srand(time.tv_usec);
  bool=(unsigned)(CONVERT(rand())+0.5);
  secs=(unsigned)(CONVERT(rand())*MAX_SLEEP);
  if((pid=fork())<0){
    unix_error("Fork error");
  }
  if(pid==0){
    if(bool){
      usleep(secs);
    }
    //    printf("pid = %d from Fork\n",(int)pid);
  }else{
    if(!bool){
      usleep(secs);
    }
  }
  //  printf("pid = %d from Fork\n",pid);
  return pid;
}

int Sigprocmask(int how, const sigset_t *set, sigset_t *oldset){
  int r=sigprocmask(how,set,oldset);
  if(r<0)
    unix_error("sigprocmask error");
  return r;
}
  
int Sigemptyset(sigset_t *set){
  int r=sigemptyset(set);
  if(r<0)
    unix_error("sigemptyset error");
  return r;
}
int Sigaddset(sigset_t *set, int signum){
  int r=sigaddset(set,signum);
  if(r<0)
    unix_error("sigaddset error");
  return r;
}
