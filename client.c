#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
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

void getDate(char * dateVar);
void clearLogs();
void writeToLogs(char * logLevel, char * message);
int countNumberOfPIDs(int PID);
void getPIDs(char * processName, int * PIDs, int numberOfPIDs);
int getNumberOfPIDsForProcess(char * processName);
void getCorrespondingProcessName(int PID, char * processName);
int waitAndKill(int processLifeSpan, int PID);
void killOldProcNannies();
void updateKillCount();
void sig_handler(int signum);
void sigint_handler(int signum);
void readAndExecute();
void dispatch(int parentPID, char * processName, int processPID, int processLifeSpan);
void monitorProcess(char * processName, int processLifeSpan);
void writeParentPIDToLogs();
void debugPrint(char * message,...);

/* --------------------------------------------------------------------- */
/* This is a sample client program for the number server. The client and */
/* the server need not run on the same machine.				 */
/* --------------------------------------------------------------------- */

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


void monitorProcess(char * processName, int processLifeSpan) { 
  // takes processName and lifeSpan
  // converts processName to PIDs
  // and monitors each PID by either
  // spawning a child for that process or
  // or reusing a "unbusy" child depending
  // on the global freeChildren variable.
  

  debugPrint("startin monitor Process for %s \n", processName);
  char * message;
  char * startOfMessage;
  int * PIDs;
  int counter = 0;
  int pid = 0;
  int parentPID = getpid();
  
  int numberOfPIDs = 0;
  // check to make sure process "processName" is still active
  if ((numberOfPIDs = getNumberOfPIDsForProcess(processName))  < 1) {
    debugPrint("no processes %s found \n", processName);
    /* char * message; */
    /* message = malloc(300); */

    /* snprintf(message, 290, "No '%s' processes found. \n", processName); */
    /* writeToLogs("Info", message); */
      
    /* free(message); */
    return;
  }
      
  debugPrint("we are parent: %d \n", getpid());
  debugPrint("for %d %s we have  the number of PIDs is equal to %d \n", getpid(), processName, numberOfPIDs);
  PIDs = malloc(numberOfPIDs + 100);
  message = malloc(300);
  startOfMessage = message;

  getPIDs(processName, PIDs, numberOfPIDs);

  // print out messages to logs regarding which
  // processes are being monitored
  for (counter = 0; counter < numberOfPIDs; counter = counter + 1) {

    if (monitoredPIDs != NULL && searchNodes(monitoredPIDs, processName, PIDs[counter]) != -1) {
      // only call monitor process if
      // process has more than 0 pids active
      // and it is not in monitoredPIDs linked list
      // ie it is not being monitored already
      debugPrint("process %s with pid %d is already being monitored \n", processName, PIDs[counter]);
      continue;
    }


    // during first loop
    if (counter == 0) {
      message += sprintf(message, "Initializing monitoring of process '%s' (%d", processName, PIDs[counter]);
    }
    // default loop
    else {
      message += sprintf(message, ", %d", PIDs[counter]);
    }

    // during last loop
    if (counter + 1 == numberOfPIDs) {
      message += sprintf(message, ") \n");
      message = startOfMessage;
      //      writeToLogs("Info", message);

    }
  }


  for (counter = 0; counter < numberOfPIDs; counter = counter + 1) {

    if (monitoredPIDs != NULL && searchNodes(monitoredPIDs, processName, PIDs[counter]) != -1) {
      // only call monitor process if
      // process has more than 0 pids active
      // and it is not in monitoredPIDs linked list
      // ie it is not being monitored already
      debugPrint("process %s with pid %d is already being monitored \n", processName, PIDs[counter]);
      continue;
    }


    debugPrint("adding pid %d to list \n", PIDs[counter]);
    if (monitoredPIDs) {
      debugPrint("ADDDING  pid %d to list! \n", PIDs[counter]);
      addNode(monitoredPIDs, processName, PIDs[counter]);
    }
    else {
      debugPrint("initialzing pid %d to list!\n", PIDs[counter]);
      monitoredPIDs = init(processName, PIDs[counter]);
    }
    if (freeChildren > 0) {
      // write to pipe newProcessToChild
      debugPrint("freeChildren is %d, preparing to reuse child for %s pid %d\n", freeChildren, processName, PIDs[counter]);
      //	  debugPrint("from parent: printing before writing to pipe \n");
      /* write message start */
      
      // close(newProcessToChild[0]);
      char tmpStr[62];
      sprintf(tmpStr, "%-20s#%20d!%20d", processName, PIDs[counter], processLifeSpan);
      write(newProcessToChild[1], tmpStr, 62);


      debugPrint("using children\n");
      freeChildren = freeChildren - 1;
    }
	
    else {
      pid = fork();
      if (pid == 0) {
      
	// child portion
	debugPrint("Spawned A child %d for process: %s, pid %d \n",getpid(),  processName, PIDs[counter]);
	dispatch(parentPID, processName, PIDs[counter], processLifeSpan);
	    
	while(keepLooping == 1) {
	  debugPrint("%d child : starting dispatch \n", getpid());
	  dispatch(parentPID, processName, -1, processLifeSpan);
	}
	
	
	free(PIDs);
	free(message);
	debugPrint("child dying....\n");
	exit(0);
	
      }
      else if (pid < 0) {
	fprintf(stderr,"error in forking. \n");
	exit(0);
      }
      else {
	numberOfChildren++;
	if (childPIDs){
	  addNode(childPIDs, " ", pid);
	}
	else {
	  childPIDs = init(" ", pid);
	}

	//printf("parent process pid=%d \n", getpid());
      }
	  
    } // spawning child end bracket

  } // for loop end bracket
  
  free(PIDs);
  free(message);
}

void dispatch(int parentPid, char * processName, int processPID, int processLifeSpan) {
  // reads if PID not provided
  // pipe and transfer info to funcion monitor
  //  int n = 0;
  /* read message start */

  if (processPID == -1 ) {
    char * pipeBuffer= malloc(62);
    char * originalPipeBufferPointer = pipeBuffer;
    char * newProcessName;
    // if we need to get process id
    // we look to getting it from the pipe
    //    debugPrint("from child: preparing to read messages \n");

    close(newProcessToChild[1]);
    read(newProcessToChild[0], pipeBuffer, 62);

    // after we've retrieved info from pipe
    // check if it is a shutdown message
    if (atoi(pipeBuffer) == -1) {
      debugPrint("I should close now\n");
      keepLooping = 0;
      free(pipeBuffer);
      return;
    }
    else {
      
      processLifeSpan = 0;
      int len = 0;
      processPID = 0;
      
      len = abs(strchr(pipeBuffer,'#') - pipeBuffer);
      newProcessName = malloc(len+1);
      memcpy(newProcessName, pipeBuffer, len);
      newProcessName[len] = '\0'; 
      pipeBuffer = strchr(pipeBuffer,'#');
      processPID = atoi(pipeBuffer+1);
      processLifeSpan = atoi(strchr(pipeBuffer,'!')+1);      
    }


    if (waitAndKill(processLifeSpan, processPID) > 0) {
      //    debugPrint("from child: sending log info  to pipe \n");
      /* write message start */
      close(killCountPipe[0]);
      char tmpStr[62];

      sprintf(tmpStr, "%-20s#%20d!%20d", newProcessName, processPID, processLifeSpan);
      //    debugPrint("child %d writing this to pipe: %s \n", getpid(), tmpStr);
      int w = write(killCountPipe[1], tmpStr, 62);
      while (w != 62) {
	w = write(killCountPipe[1], tmpStr, 62);
	debugPrint("%s\n",strerror(errno));
      }
      debugPrint("child %d w was %d \n",getpid(), w);

      // signal parent
      debugPrint("NOT SIGNALLING PARENT \n");
      //kill(parentPid,SIGUSR1);
    }

    free(newProcessName);
    free(originalPipeBufferPointer);
    //exit(0);
    
  } 
  else {


    if (waitAndKill(processLifeSpan, processPID) > 0) {
      //    debugPrint("from child: sending log info  to pipe \n");
      /* write message start */
      close(killCountPipe[0]);
      char tmpStr[62];

      sprintf(tmpStr, "%-20s#%20d!%20d", processName, processPID, processLifeSpan);
      //    debugPrint("child %d writing this to pipe: %s \n", getpid(), tmpStr);
      int w = write(killCountPipe[1], tmpStr, 62);
      while (w != 62) {
	w = write(killCountPipe[1], tmpStr, 62);
	debugPrint("%s\n",strerror(errno));
      }
      debugPrint("child %d w was %d \n",getpid(), w);

      // signal parent
      debugPrint("NOT SIGNALLING PARENT \n");
      //kill(parentPid,SIGUSR1);
    }

  }
  
  
  
}

int waitAndKill(int processLifeSpan, int PID) {
  char * inputBuffer = malloc(150);
  int killed = 0;
  // processLifeSpan is in seconds
  sleep(processLifeSpan);

  FILE * fp;

  if (countNumberOfPIDs(PID) == 1) {
    snprintf(inputBuffer, 140, "kill -9 %d", PID);
    //printf("%s \n", command);
    //    fp = popen(inputBuffer, "r");

    if (PID != -1 && (fp = popen(inputBuffer, "r")) != NULL) {
      killed = killed + 1;
    }

  } 


  free(inputBuffer);
  pclose(fp);
  return killed;
}

int countNumberOfPIDs(int PID) {
  FILE * getPIDfp;
  char * getPIDbuffer = malloc(100);
  char * getPIDcommand = malloc(150);
  int count = -1;
  snprintf(getPIDcommand, 140, "ps -e | grep '%d' -c", PID);
  getPIDfp = popen(getPIDcommand, "r");
  if (getPIDfp != NULL) {
    fgets(getPIDbuffer, 15, getPIDfp);    
    count = atoi(getPIDbuffer);
    //printf("%s process has PID %d \n", processName, PIDs[counter]);
  }

  pclose(getPIDfp);
  free(getPIDbuffer);
  free(getPIDcommand);
  return count;
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


int main(int argc, char * argv[]) {

  if (pipe(newProcessToChild) < 0) {
    fprintf(stderr,"pipe error");
    exit(0);
  }

  if (pipe(killCountPipe) < 0) {
    fprintf(stderr,"pipe error");
    exit(0);
  }

  fcntl(killCountPipe[0], F_SETFL, O_NONBLOCK);

  int s;
  struct sockaddr_in server;  
  struct hostent *host;
  printf("connecting to %s on port %d \n", argv[1], atoi(argv[2]));
  int MY_PORT = atoi(argv[2]);
	
  /* Put here the name of the sun on which the server is executed */
  host = gethostbyname (argv[1]);

  if (host == NULL) {
    perror ("Client: cannot get host description");
    exit (1);
  }
  char * pipeBuffer = malloc(62);
  while (1) {
    // use this loop to get process lists
    // 
    s = socket (AF_INET, SOCK_STREAM, 0);

    if (s < 0) {
      perror ("Client: cannot open socket");
      exit (1);
    }

    bzero (&server, sizeof (server));
    bcopy (host->h_addr, & (server.sin_addr), host->h_length);
    server.sin_family = host->h_addrtype;
    server.sin_port = htons (MY_PORT);

    if (connect (s, (struct sockaddr*) & server, sizeof (server))) {
      perror ("Client: cannot connect to server");
      exit (1);
    }
		
    read (s, pipeBuffer, 62);
    //    char * tmpBuffer = pipeBuffer;
  
    char * processName;
    int len = 0;
    int PID = 0;
    int processLifeSpan = 0;


    len = strchr(pipeBuffer,'#') - pipeBuffer;
    processName = malloc(len+1);
    strncpy(processName, pipeBuffer, len);
    processName[len] = '\0'; 

    processName[strlen(processName) - 1] = '\0';

  
    pipeBuffer = strchr(pipeBuffer,'#');
    PID = atoi(pipeBuffer+1);
    processLifeSpan = atoi(strchr(pipeBuffer,'!')+1);

    char *newP = trimwhitespace(processName);
    printf("results from updateKillCount pipe read\n");
    printf("name: %s \n", newP);
    printf("life span: %d \n", processLifeSpan);
    printf("pid: %d \n", PID);
    close (s);
    fprintf (stderr, "Process %d gets number %s\n", getpid (),
	     pipeBuffer);
    while (1) {
      monitorProcess(newP, processLifeSpan);
      sleep (5);
      
    }
  }
  
  return 0;
}




