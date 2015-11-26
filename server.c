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




/*
 * Copyright (c) 2008 Bob Beck <beck@obtuse.com>
 * Some changes (related to the port number) by ***REMOVED***, March 2011.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * select_server.c - an example of using select to implement a non-forking
 * server. In this case this is an "echo" server - it simply reads
 * input from clients, and echoes it back again to them, one line at
 * a time.
 *
 * to use, cc -DDEBUG -o select_server select_server.c
 * or cc -o select_server select_server.c after you read the code :)
 *
 * then run with select_server PORT
 * where PORT is some numeric port you want to listen on.
 * (***REMOVED***:  You can also get the OS to choose the port by not specifying PORT)
 *
 * to connect to it, then use telnet or nc
 * i.e.
 * telnet localhost PORT
 * or
 * nc localhost PORT
 * 
 */


#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* we use this structure to keep track of each connection to us */
struct con {
  int sd; 	/* the socket for this connection */
  int state; 	/* the state of the connection */
  struct sockaddr_in sa; /* the sockaddr of the connection */
  size_t  slen;   /* the sockaddr length of the connection */
  char *buf;	/* a buffer to store the characters read in */
  char *bp;	/* where we are in the buffer */
  size_t bs;	/* total size of the buffer */
  size_t bl;	/* how much we have left to read/write */
};

int connectionWeSendMessageTo = -1;
#define MAXCONN 2
struct con connections[2]; 
#define BUF_SIZE 63
#define	MY_PORT	2222

/* states used in struct con. */
#define STATE_UNUSED 0
#define STATE_READING 1
#define STATE_WRITING 2


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
struct node * clientNames;
struct node * monitoredPIDs;
struct node * childPIDs;
int messagesFromChildren = 0;
int DEBUGGING = 1;
int sock, snew;
socklen_t fromlength;
struct sockaddr_in master, from;
int sighup_called = 0; // set to false 


void sig_handler(int signum);
void debugPrint(char * message,...);
void writeToLogs(char * logLevel, char * message);
void clearLogs();
void getDate(char * dateVar);
void getPIDs(char * processName, int * PIDs, int numberOfPIDs);
int getNumberOfPIDsForProcess(char * processName);

// kill previous procnannies
void killOldProcNannies()  {
  int numberOfProcNanniePIDs = getNumberOfPIDsForProcess("procnanny.ser");
  
  if (numberOfProcNanniePIDs > 1) {
    char * inputBuffer = malloc(150);
    FILE * fp;
    //printf("found %d processes of procnanny running. Commencing deletion... \n", numberOfProcNanniePIDs);
    int * procNanniePIDs = malloc(numberOfProcNanniePIDs + 50);
    getPIDs("procnanny.ser", procNanniePIDs, numberOfProcNanniePIDs);

    // remove current pid
    int currentProcPID = getpid();
    int tmpCounter = 0;
    for (tmpCounter = 0; tmpCounter < numberOfProcNanniePIDs; tmpCounter = tmpCounter + 1) {
      if (currentProcPID != procNanniePIDs[tmpCounter]) {
	snprintf(inputBuffer, 140, "kill -9 %d", procNanniePIDs[tmpCounter]);
	//printf("%s \n", inputBuffer);

	if ((fp = popen(inputBuffer, "r")) != NULL) {
	  snprintf(inputBuffer, 120, "An old procnanny.server (%d) was killed \n",  procNanniePIDs[tmpCounter]);
	  writeToLogs("Warning", inputBuffer);
	}

      }
    }


    free(inputBuffer);
    free(procNanniePIDs);
  }
  
}


void extractClientNames(char * message) {
  char * clientName = NULL;
  clientName = strstr(message, "on node") + strlen("on node ");
  if (clientNames == NULL) {
    clientNames = init(clientName, 0);
  }
  else if (searchNodes(clientNames, clientName, 0) == -1){
    addNode(clientNames, clientName, 0);
  }
}


void writeToProcNannyServerInfo() {
  // PROCNANNYSERVERINFO
  FILE *logFile;


  if (getenv("PROCNANNYSERVERINFO") == NULL) { 
    fprintf(stderr,"PROCNANNYSERVERINFO variable not set\n");
    exit(0);
  }
  
  if ((logFile = fopen(getenv("PROCNANNYSERVERINFO"), "w+")) == NULL) {
    fprintf(stderr,"Error in opening the PROCNANNYSERVERINFO file \n");
    exit(0);
  }

  char * hostName = malloc(50);
  gethostname(hostName, 45);

  char * newLogMessage = malloc(150);
  int port = MY_PORT;
  snprintf(newLogMessage, 150, "NODE %s PID %d PORT %d", hostName, getpid(), port);
  
  fputs(newLogMessage, logFile);
  free(hostName);
  free(newLogMessage);
  fclose(logFile);  
}


void writeParentPIDToLogs()  {
  char * hostName = malloc(50);
  gethostname(hostName, 45);

  char * parentPIDMessage;
  parentPIDMessage = malloc(300);
  int port = MY_PORT;

  snprintf(parentPIDMessage, 290, "PID %d on %s, port %d.", getpid(), hostName, port);
  writeToLogs("procnanny server", parentPIDMessage);
      
  free(parentPIDMessage);
  free(hostName);
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



/* --------------------------------------------------------------------- */
/* This	 is  a sample server which opens a stream socket and then awaits */
/* requests coming from client processes. In response for a request, the */
/* server sends an integer number  such	 that  different  processes  get */
/* distinct numbers. The server and the clients may run on different ma- */
/* chines.							         */
/* --------------------------------------------------------------------- */





/* --------------------------------------------------------------------- */
/* This	 is  a sample server which opens a stream socket and then awaits */
/* requests coming from client processes. In response for a request, the */
/* server sends an integer number  such	 that  different  processes  get */
/* distinct numbers. The server and the clients may run on different ma- */
/* chines.							         */
/* --------------------------------------------------------------------- */





/*
 * get a free connection structure, to save a new connection in
 */
struct con * get_free_conn()
{
	int i;
	for (i = 0; i < MAXCONN; i++) {
		if (connections[i].state == STATE_UNUSED)
			return(&connections[i]);
	}
	return(NULL); /* we're all full - indicate this to our caller */
}



/*
 * close or initialize a connection - resets a connection to the default,
 * unused state.
 */
void closecon (struct con *cp, int initflag)
{
	if (!initflag) {
		if (cp->sd != -1)
			close(cp->sd); /* close the socket */
		free(cp->buf); /* free up our buffer */
	}
	memset(cp, 0, sizeof(struct con)); /* zero out the con struct */
	cp->buf = NULL; /* unnecessary because of memset above, but put here
			 * to remind you NULL is 0.
			 */
	cp->sd = -1;
}

/* ***REMOVED*** */
/*
 * getPortNumber - given a valid file descriptor/socket, return the port number
 */
int getPortNumber( int socketNum )
{
	struct sockaddr_in addr;
	int rval;
	socklen_t addrLen;

	addrLen = (socklen_t)sizeof( addr );

	/* Use getsockname() to get the details about the socket */
	rval = getsockname( socketNum, (struct sockaddr*)&addr, &addrLen );
	if( rval != 0 )
		err(1, "getsockname() failed in getPortNumber()");

	/* Note cast and the use of ntohs() */
	return( (int) ntohs( addr.sin_port ) );
} /* getPortNumber */



void writeToClient2(char * message, struct con * cp) {

  
  int w = write(cp->sd, message, 63);
  debugPrint("wrote %d b's which was %s\n", w, message);
  while (w != 63) {
    w = write (snew, message, 63);
    printf("trying to write again cuz we wrote %d \n", w);
    sleep(5);
  }
  
}




int readFromClient2(struct con *cp) {
  // returns 1 meaning send process name and lifespan
  // else do nothing


  // check for messages from client

  char * logMessage = malloc(200);
  int w2 = read (cp->sd, logMessage, 200);
  //  close(snew);
  while (w2 < 0) {
    w2 = read (snew, logMessage, 200);
    printf("trying to read again \n");
    sleep(5);
  }
  if (w2 == 0) {
    // client disconnected according to 
    // http://stackoverflow.com/questions/2416944/can-read-function-on-a-connected-socket-return-zero-bytes
    closecon(cp, 0);
    return -1;
  }
  debugPrint("we got %s from client \n", logMessage);


  if (logMessage[0] == '[') {
    debugPrint("request for logging received \n");
    FILE *logFile;
    logFile = fopen(getenv("PROCNANNYLOGS"), "a");    

    debugPrint("logging this: %s \n", logMessage);    
    char * newLogMessage = trimwhitespace(logMessage);
    extractClientNames(newLogMessage);
    fputs(newLogMessage, logFile);
    fputs("\n",logFile);
    fclose(logFile);
    free(logMessage);
    return 0;
  }
  else if (logMessage[0] == 'k') {
    logMessage[0] = '0';
    debugPrint("updating kill count: %s\n", logMessage);
    totalProcsKilled += atoi(logMessage);
    debugPrint("new kill count %d\n", totalProcsKilled);
    free(logMessage);
    return 0;
  }

  else {
    free(logMessage);
    return 1;
  }
  

}






int main(int argc, char * argv[]) {
  debugPrint("starting program\n");
  clearLogs();
  killOldProcNannies();
  writeToProcNannyServerInfo();
  writeParentPIDToLogs();
  int max = -1, omax;
  fd_set *readable = NULL;
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = INADDR_ANY;
  master.sin_port = htons (MY_PORT);

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror ("Server: cannot open master socket");
    exit (1);
  }

  if (bind (sock, (struct sockaddr*) &master, sizeof (master))) {
    perror ("Server: cannot bind master socket");
    exit (1);
  }

  if (listen(sock, 5) == -1) {
    perror("Server: cant listen");
    exit(1);
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
  int i = 0;
  for (i = 0; i < MAXCONN; i++) {
    closecon(&connections[i], 1);
  }
  
  while(rereadFromConfigFile == 1) {

    while(keepLooping == 1) {

      if (rereadFromConfigFile == 1) {
	if (processList) {
	  freeList(processList); // free it in case it is still set
	  processList = NULL;
	}

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


      int i = 0;
      int maxfd = -1;
      omax = max;
      max = sock;
      
      for (i = 0; i < MAXCONN; i++) {
	if (connections[i].sd > max) {
	  max = connections[i].sd;
	}	
      }
      
      if (max > omax) {
	/* we need bigger fd_sets allocated */

	/* free the old ones - does nothing if they are NULL */
	if (readable) {
	  free(readable);	  
	}

	/*
	 * this is how to allocate fd_sets for select
	 */
	readable = (fd_set *)calloc(howmany(max + 1, NFDBITS),
				    sizeof(fd_mask));
	if (readable == NULL)
	  err(1, "out of memory");
	omax = max;
      } else {
	/*
	 * our allocated sets are big enough, just make
	 * sure they are cleared to 0. 
	 */
	memset(readable, 0, howmany(max+1, NFDBITS) *
	       sizeof(fd_mask));
      }


      FD_SET(sock, readable);
      if (maxfd < sock)
	maxfd = sock;

      /*
       * now go through the list of connections, and if we
       * are interested in reading from, or writing to, the
       * connection's socket, put it in the readable, or
       * writable fd_set - in preparation to call select
       * to tell us which ones we can read and write to.
       */
      for (i = 0; i<MAXCONN; i++) {
	if (connections[i].state == STATE_READING) {
	  FD_SET(connections[i].sd, readable);
	  if (maxfd < connections[i].sd)
	    maxfd = connections[i].sd;
	}

      }

      i = select(maxfd + 1, readable, NULL, NULL,NULL);
      if (i == -1  && errno != EINTR)
	err(1, "select failed");
      if (i > 0) {

	if (FD_ISSET(sock, readable)) {
	  struct con *cp;
	  int newsd;
	  socklen_t slen;
	  struct sockaddr_in sa;

	  slen = sizeof(sa);
	  newsd = accept(sock, (struct sockaddr *)&sa,
			 &slen);
	  if (newsd == -1)
	    err(1, "accept failed");

	  cp = get_free_conn();
	  if (cp == NULL) {
	    /*
	     * we have no connection structures
	     * so we close connection to our
	     * client to not leave him hanging
	     * because we are too busy to
	     * service his request
	     */
	    close(newsd);
	  } else {
	    /*
	     * ok, if this worked, we now have a
	     * new connection. set him up to be
	     * READING so we do something with him
	     */
	    cp->state = STATE_READING;
	    cp->sd = newsd;
	    cp->slen = slen;
	    memcpy(&cp->sa, &sa, sizeof(sa));
	  }
	}
	for (i = 0; i<MAXCONN; i++) {
	  if (connections[i].state == STATE_READING &&
	      FD_ISSET(connections[i].sd, readable)) {
	    // something is readable
	    int readFromClientReturnValue = readFromClient2(&connections[i]);
	    if (readFromClientReturnValue == 1) {


	      // writing
	      debugPrint("request for info received... sending\n");
	      debugPrint("beginning write to internet socket\n");

	      if (sighup_called == 1) {
		snprintf(tmpStr, 63, "%-20s#%20d!%20d", "null", getSize(processList), 99);  
		sighup_called = 0;
	      }
	      else {
		snprintf(tmpStr, 63, "%-20s#%20d!%20d", "null", getSize(processList), 5);		
	      }

	      debugPrint("writing this to pipe: %s \n", tmpStr);
	      int w2 = write(connections[i].sd, tmpStr, 63);
	      debugPrint("w2 = %d\n", w2);
	      sleep(0.5);
	      struct node * currentNode = processList;
	      while (currentNode != NULL) {
	  
		snprintf(tmpStr, 63, "%-20s#%20d!%20d", currentNode->value, 11, currentNode->key);
		debugPrint("writing this to pipe: %s \n", tmpStr);
		int w3 = write(connections[i].sd, tmpStr, 63);
		debugPrint("w3 = %d\n", w3);
	    
		currentNode = currentNode->next;
	      }
      
  
	      debugPrint("finished writing to internet socket\n");
	    }

	  }


	}

      }
    }
    free(tmpStr);

    if (readable) {
      free(readable);	  
    }

      
  }

  debugPrint("server receieved sigint, signalling clients...\n");
  char * lastMessage = malloc(63);
  snprintf(lastMessage, 63, "!%-19s#%20d!%20d", "null", 1, 5);
  debugPrint("writing lastMessage to pipe: %s \n", lastMessage);
  for (i = 0; i < MAXCONN; i++) {
    int w3 = write(connections[i].sd, lastMessage, 63);
    debugPrint("last message bytes sent = %d\n", w3);
  }

  char * message;
  message = malloc(200);

  //  printf("Caught SIGINT. Exiting cleanly. %d process(es) killed on ", totalProcsKilled);

  char * pointerToMessage = message;
  message += sprintf(message, "Caught SIGINT. Exiting cleanly. %d process(es) killed on", totalProcsKilled);
  
  struct node * currentNode = clientNames;
  while(currentNode != NULL) {
    message += sprintf(message, " %s,", currentNode->value);
    currentNode = currentNode->next;
  }
  message = pointerToMessage;
  printf("%s", message);
  writeToLogs("Info", pointerToMessage);

  //  free(message);
  free(pointerToMessage);
  fclose(fp2);
  free(lastMessage);
  freeList(processList);
  freeList(clientNames);
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
    sighup_called = 1;
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
  fputs("\n", logFile);
  free(dateVar);
  free(logMessage);
  //  free(newLogMessage);
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


int getNumberOfPIDsForProcess(char * processName) {
  // populates PIDs with 
  // the PIDs that correspond 
  // to process processName

  // returns size of int array containging 
  // the PIDs.
  // Use the size to walk through the array

  FILE * fp;
  char * numberOfPIDSbuffer = malloc(100);
  char * numberOfPIDScommand = malloc(150);
  int numberOfPIDs = -1;
  snprintf(numberOfPIDScommand, 140, "ps -e | grep ' %s' | awk '{print $1}' | wc -l", processName);
  //printf("%s \n", command);

  fp = popen(numberOfPIDScommand, "r");
  if (fp != NULL) {
    fgets(numberOfPIDSbuffer, 15, fp);
    numberOfPIDs = atoi(numberOfPIDSbuffer);
    //printf("%s number of PIDs are %d \n", processName, numberOfPIDs);
  }

  pclose(fp);
  free(numberOfPIDSbuffer);
  free(numberOfPIDScommand);

  return numberOfPIDs;
  
}

void getPIDs(char * processName, int * PIDs, int numberOfPIDs) {
  // populates PIDs with 
  // the PIDs that correspond 
  // to process processName

  // Use numberOfPIDs to walk through the array

  FILE * getPIDfp;
  char * getPIDbuffer = malloc(100);
  char * getPIDcommand = malloc(150);
  int counter = 0;
  PIDs[0] = -1; // default value
  for (counter = 0; counter < numberOfPIDs; counter = counter + 1) {
    snprintf(getPIDcommand, 140, "ps -e | grep ' %s' | awk '{print $1}' | tail -n %d | head -n 1", processName, numberOfPIDs - counter);
    //printf("%s \n", command);

    getPIDfp = popen(getPIDcommand, "r");
    //    popen.wait();
    if (getPIDfp != NULL) {
      fgets(getPIDbuffer, 15, getPIDfp);
      PIDs[counter] = atoi(getPIDbuffer);
      //printf("%s process has PID %d \n", processName, PIDs[counter]);
    }
    pclose(getPIDfp);
    
  }

  free(getPIDbuffer);
  free(getPIDcommand);
  
}

