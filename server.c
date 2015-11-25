#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h> // for avoiding assignment issues 
#include <errno.h> // for error printing
#include <string.h> // for string functions
#include <unistd.h>  // for sleep, close
#include <fcntl.h> // for setting non blocking pipe
#include <signal.h> // for signals
#include <ctype.h> // for isspace() function
#include "linkedList.h" // for linked list struct
#include "memwatch.h" // for proper memory use checks
#include <stdarg.h> // for variable argument support

#define	MY_PORT	2222

int totalProcsKilled = 0;
int freeChildren = 0;
char * fileName = NULL;
int keepLooping = 1; // 1 = true, 0 = false
int newProcessToChild[2]; // p2c[0] gets written to on parent, p2c[1] gets read on child
int killCountPipe[2]; // killCountPipe[0] gets written to on child, killCountPipe[1] gets read on parent
int numberOfChildren = 0;
struct sigaction sa;
int rereadFromConfigFile = 1;
int parentPid = 0;
struct node * processList;
struct node * monitoredPIDs;
struct node * childPIDs;
int messagesFromChildren = 0;
int DEBUGGING = 1;
int sock, snew;
socklen_t fromlength;
struct sockaddr_in master, from;


void sig_handler(int signum);
void debugPrint(char * message,...);
void writeToLogs(char * logLevel, char * message);
void clearLogs();
void getDate(char * dateVar);

void writeParentPIDToLogs()  {
  char * parentPIDMessage;
  parentPIDMessage = malloc(300);

  snprintf(parentPIDMessage, 290, "Parent process is PID %d. \n", getpid());
  writeToLogs("Info", parentPIDMessage);
      
  free(parentPIDMessage);
  return;
}


// function from http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;
  
  return str;
}


void writeToClient(char * message) {

  listen (sock, 5);
  fromlength = sizeof (from);
  snew = accept (sock, (struct sockaddr*) & from, & fromlength);
  while (snew < 0) {
    perror ("Server: accept failed");
    snew = accept (sock, (struct sockaddr*) & from, & fromlength);
  }
  
  int w = write(snew, message, 63);
  while (w != 63) {
    w = write (snew, message, 63);
    printf("trying to write again \n");
    sleep(5);
  }

  
}


int readFromClient() {
  // returns 1 meaning send process name and lifespan
  // else do nothing
  listen (sock, 5);

  fromlength = sizeof (from);
  snew = accept (sock, (struct sockaddr*) & from, & fromlength);
  while (snew < 0) {
    perror ("Server: accept failed");
    snew = accept (sock, (struct sockaddr*) & from, & fromlength);
  }


  // check for messages from client
  char * logMessage = malloc(200);
  int w2 = read (snew, logMessage, 200);
  while (w2 < 0) {
    w2 = read (snew, logMessage, 200);
    printf("trying to read again \n");
    sleep(5);
  }
  debugPrint("we got %s from client \n", logMessage);


  if (logMessage[0] == '[') {
    debugPrint("request for logging received \n");
    FILE *logFile;
    logFile = fopen(getenv("PROCNANNYLOGS"), "a");    

    debugPrint("logging this: %s \n", logMessage);    
    char * newLogMessage = trimwhitespace(logMessage);
    fputs(newLogMessage, logFile);
    fputs("\n",logFile);
    fclose(logFile);
    
    return 0;
  }
  else if (logMessage[0] == 'k') {
    debugPrint("updating kill count\n");
    totalProcsKilled += atoi(logMessage);
    debugPrint("new kill count %d\n", totalProcsKilled);
  }

  free(logMessage);
  return 1;


}



/* --------------------------------------------------------------------- */
/* This	 is  a sample server which opens a stream socket and then awaits */
/* requests coming from client processes. In response for a request, the */
/* server sends an integer number  such	 that  different  processes  get */
/* distinct numbers. The server and the clients may run on different ma- */
/* chines.							         */
/* --------------------------------------------------------------------- */

int main(int argc, char * argv[]) {
  debugPrint("starting program\n");
  clearLogs();
  writeParentPIDToLogs();

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror ("Server: cannot open master socket");
    exit (1);
  }


  master.sin_family = AF_INET;
  master.sin_addr.s_addr = INADDR_ANY;
  master.sin_port = htons (MY_PORT);


  if (bind (sock, (struct sockaddr*) &master, sizeof (master))) {
    perror ("Server: cannot bind master socket");
    exit (1);
  }





  debugPrint("starting readAndExecute\n");

  sa.sa_handler = sig_handler;
  int errorSig = sigaction(SIGHUP, &sa, NULL);
  errorSig += sigaction(SIGINT, &sa, NULL);
  errorSig += sigaction(SIGUSR1, &sa, NULL);
  if (errorSig < 0) {
    fprintf(stderr,"error in setting signal");
    exit(0);
  }

  
  FILE * fp2;
  char * fileName = argv[1]; 
  if (fileName == NULL) {
    printf("filename variable not set \n");
    exit(0);
  }


  char * line = NULL;
  size_t len = 0; 
  ssize_t read1;
  //int status = 0;
  int processLifeSpan = 0;
  char * processName = NULL;

  char delimiter = ' ';
  char * tmpStr = malloc(63);

  while(rereadFromConfigFile == 1) {


    if (rereadFromConfigFile == 1) {
      debugPrint("reread config file\n");
      fp2 = fopen(fileName,"r");
      if (fp2 == NULL) { 
	clearLogs();
	writeToLogs("Error", "Configuration file not found.\n");
	fprintf(stderr,"Configuration file not found.\n");
	exit(0); 
      }
    }
    
    while ((read1 = getline(&line, &len, fp2)) != -1 && rereadFromConfigFile == 1) {
      debugPrint("from parent: starting main while \n ");

    
      char * tmp = strchr(line, delimiter);
      processLifeSpan = atoi(tmp);
      strncpy(line, line, strlen(line) - strlen(tmp));
      line[strlen(line) - strlen(tmp)] = '\0'; // removes new line
      processName = line;

      debugPrint("%s \n", processName);
      debugPrint("%d \n", processLifeSpan);

      if (processList) { 
	if ( searchNodes(processList, processName, processLifeSpan) == -1) {
	  debugPrint("%d adding node \n", getpid());
	  addNode(processList, processName, processLifeSpan);	  
	}
      }
      else {
	debugPrint("creating process list... %d processListing node \n", getpid());
	processList = init(processName, processLifeSpan);
      }
      

    }// while loop for reading file

    rereadFromConfigFile = -1;
    while(keepLooping == 1) {
      if (readFromClient() == 1) {
	debugPrint("request for info received... sending\n");
	debugPrint("beginning write to internet socket\n");

	snprintf(tmpStr, 63, "%-20s#%20d!%20d", "null", getSize(processList), 5);
	debugPrint("writing this to pipe: %s \n", tmpStr);
	writeToClient(tmpStr);
	struct node * currentNode = processList;
	while (currentNode != NULL) {
	  
	  snprintf(tmpStr, 63, "%-20s#%20d!%20d", currentNode->value, 11, currentNode->key);
	  debugPrint("writing this to pipe: %s \n", tmpStr);
	  writeToClient(tmpStr);	  
	  currentNode = currentNode->next;
	}
      
  
	debugPrint("finished writing to internet socket\n");
	close (snew);
	//fclose(fp2);

	  
      }
	
    }
    free(tmpStr);
      

    //    sleep(5);
    //debugPrint("reread = %d\n", rereadFromConfigFile);
    //debugPrint("trying to read from client...\n");
    //    readFromClient();


    
  }

  debugPrint("server receieved sigint, signalling clients...\n");
  readFromClient();
  char * lastMessage = malloc(63);

  snprintf(lastMessage, 63, "!%-19s#%20d!%20d", "null", 1, 5);
  debugPrint("writing lastMessage to pipe: %s \n", lastMessage);
  writeToClient(lastMessage);
  close(snew);

  char * message;
  message = malloc(200);

  printf("Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
  snprintf(message, 190, "Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
  writeToLogs("Info", message);

  free(message);

  fclose(fp2);
  return 0;
}


void sig_handler(int signum) {
  //  int i = 0;

  switch(signum) {
    
  case 1:
    printf("Caught SIGHUP. Configuration file 'nanny.config' re-read.\n");
    writeToLogs("Info", "Caught SIGHUP. Configuration file 'nanny.config' re-read.\n");
    debugPrint("\n\n\nCaught SIGHUP. Configuration file 'nanny.config' re-read.\n\n\n");
    rereadFromConfigFile = 1;
    break;
  case 2:
    debugPrint("caught sigint, i am %d and parent is %d\n", getpid(), parentPid);
    keepLooping = 0;
    break;
  case 10:
    debugPrint("caught sigusr1\n");
    messagesFromChildren = messagesFromChildren + 1;
    //updateKillCount();        
    break;
  default:
    debugPrint("idk\n");
  }
}



void debugPrint(char * message,...) {
  // http://stackoverflow.com/questions/1056411/how-to-pass-variable-number-of-arguments-to-printf-sprintf?lq=1
  if (DEBUGGING) {
    va_list argptr;
    va_start(argptr, message);
    vfprintf(stdout, message, argptr);
    va_end(argptr);    
  }
  else {
    return;
  }
}


void clearLogs() {
  FILE *logFile;


  if (getenv("PROCNANNYLOGS") == NULL) { 
    fprintf(stderr,"PROCNANNYLOGS variable not set\n");
    exit(0);
  }
  
  if ((logFile = fopen(getenv("PROCNANNYLOGS"), "w+")) == NULL) {
    fprintf(stderr,"Error in opening the PROCNANNYLOGS file \n");
    exit(0);
  }    
  fclose(logFile);
  
}


void writeToLogs(char * logLevel, char * message) {
  FILE *logFile;
  char * logMessage = malloc(200);
  logFile = fopen(getenv("PROCNANNYLOGS"), "a");    
    
  char * dateVar = malloc(50);
  getDate(dateVar);
  
  snprintf(logMessage, 190, "[%s] %s: %s", dateVar, logLevel, message);
  char *newLogMessage = trimwhitespace(logMessage);
  //fprintf(logFile, "[%s] %s: %s", dateVar, logLevel, message);
  fputs(newLogMessage, logFile);
  free(dateVar);
  free(logMessage);
  free(newLogMessage);
  fclose(logFile);
  
}

void getDate(char * dateVar) {
  FILE * fpTemp;
  fpTemp = popen("date", "r");

  if (fpTemp != NULL) {
    fgets(dateVar, 50,fpTemp);
    dateVar[strlen(dateVar) - 1] = '\0';
    //printf("%s \n", dateVar);
  }
  pclose(fpTemp);
}
