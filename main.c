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
int DEBUGGING = 0;

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

int main(int argc, char *argv[]) {

  parentPid = getpid();
  debugPrint("Parent PID = %d\n", parentPid);    
  debugPrint("set handlers\n");
  if (pipe(newProcessToChild) < 0) {
    fprintf(stderr,"pipe error");
    exit(0);
  }

  if (pipe(killCountPipe) < 0) {
    fprintf(stderr,"pipe error");
    exit(0);
  }

  fcntl(killCountPipe[0], F_SETFL, O_NONBLOCK);

  fileName = argv[1];

  clearLogs();
  writeParentPIDToLogs();  
  killOldProcNannies();


  readAndExecute();
  debugPrint("parent in main, going to wrap_up\n");
  //  wrap_up();
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

void updateKillCount() {

  // reads pipe killCountPipe for message indicating the kills
  // process have committed and updates 
  // parent-controlled totalProcsKilled variable
  
  /* read message start */
  //  debugPrint("from parent preparing to log messages \n");


  //close(killCountPipe[1]);
  char * pipeBuffer= malloc(62);
  debugPrint("updating kill...\n");

  if (read(killCountPipe[0], pipeBuffer, 62) <= 0) {
    debugPrint("found nothing on killCount pipe, returning\n");
    free(pipeBuffer);
    return;
  }

  char * tmpBuffer = pipeBuffer;
  
  char * inputBuffer = malloc(150);
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
  debugPrint("results from updateKillCount pipe read\n");
  debugPrint("name: %s|| \n", newP);
  debugPrint("life span: %d \n", processLifeSpan);
  debugPrint("pid: %d \n", PID);


  if (searchNodes(monitoredPIDs, newP, PID) != -1) {
    debugPrint("NOT REALLYremoving pid that was killed.\n");
    //removeNode(monitoredPIDs, newP, PID);    
  }

  totalProcsKilled++;
  
  debugPrint("PID (%d) (%s) killed after exceeding %d seconds. \n", PID, processName, processLifeSpan);
  sprintf(inputBuffer, "PID (%d) (%s) killed after exceeding %d seconds. \n", PID, processName, processLifeSpan);
  writeToLogs("Action", inputBuffer);
  

  free(tmpBuffer);
  free(inputBuffer);
  free(processName);  
  freeChildren = freeChildren + 1;  
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
    char * message;
    message = malloc(300);

    snprintf(message, 290, "No '%s' processes found. \n", processName);
    writeToLogs("Info", message);
      
    free(message);
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
      writeToLogs("Info", message);

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



void readAndExecute() {
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
  if (fileName == NULL) {
    exit(0);
  }


  char * line = NULL;
  size_t len = 0; 
  ssize_t read1;
  //int status = 0;
  int processLifeSpan = 0;
  char * processName = NULL;

  char delimiter = ' ';

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
    
    debugPrint("reread = %d\n", rereadFromConfigFile);
    while ((read1 = getline(&line, &len, fp2)) != -1 && rereadFromConfigFile == 1) {
      debugPrint("from parent: starting main while \n ");

    
      char * tmp = strchr(line, delimiter);
      processLifeSpan = atoi(tmp);
      strncpy(line, line, strlen(line) - strlen(tmp));
      line[strlen(line) - strlen(tmp)] = '\0'; // removes new line
      processName = line;

      debugPrint("%s \n", processName);
      debugPrint("%d \n", processLifeSpan);
      debugPrint("freeChildren is %d \n", freeChildren);
      debugPrint("adding all  nodes \n");
      
      if (processList && searchNodes(processList, processName, processLifeSpan)) {
	debugPrint("%d adding node \n", getpid());
	addNode(processList, processName, processLifeSpan);
      }
      else {
	debugPrint("%d processListing node \n", getpid());
	processList = init(processName, processLifeSpan);
      }
      
      // convert processName into PIDs
      // pass PIDs, one by one, into a separate child
      monitorProcess(processName, processLifeSpan);
      //fclose(fp2);

    }// while loop for reading file

    rereadFromConfigFile = -1;
    sleep(5);

    updateKillCount();
    debugPrint("********************\n");

    debugPrint("number of freechildren is %d: \n", freeChildren);
    debugPrint("number of messages from children is %d: \n", messagesFromChildren);
    struct node * cursorNode;
    int i = 0;
    debugPrint("processes we monitored are: \n");
    cursorNode = monitoredPIDs;
    if (monitoredPIDs) {
      for (i = 0; i < getSize(monitoredPIDs); i++) {
	debugPrint("value: %s, key: %d\n", cursorNode->value, cursorNode->key);
	cursorNode = cursorNode->next;
      }
      
    }


    debugPrint("process we got are: \n");

    cursorNode = processList;

    for (i = 0; i < getSize(processList); i++) {
      debugPrint("calling monitorProcess \n");
      debugPrint("for %s, %d \n",cursorNode->value, cursorNode->key);

      if (getNumberOfPIDsForProcess(cursorNode->value) > 0) {
	monitorProcess(cursorNode->value, cursorNode->key);   	
      }
      else {
	debugPrint("No %s proc found.\n", cursorNode->value);
      }

      cursorNode = cursorNode->next;
    }



    
  }

  int i = 0;

  debugPrint("parent receieved sigint, signalling children...\n");

  struct node * currentNode = childPIDs;
  for (i = 0; i < numberOfChildren; i++) {
    debugPrint("sending sigint to %d\n",currentNode->key); 
    kill(SIGINT, currentNode->key);
    currentNode = currentNode->next;      
  }
      
  char * message;
  message = malloc(300);

  printf("Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
  snprintf(message, 290, "Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
  writeToLogs("Info", message);

  free(message);



  fclose(fp2);
  freeList(processList);
  freeList(monitoredPIDs);
  freeList(childPIDs);
  return;
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
  char * logMessage = malloc(400);
  logFile = fopen(getenv("PROCNANNYLOGS"), "a");    
    
  char * dateVar = malloc(50);
  getDate(dateVar);
  snprintf(logMessage, 390, "[%s] %s: %s", dateVar, logLevel, message);
  //fprintf(logFile, "[%s] %s: %s", dateVar, logLevel, message);
  fputs(logMessage, logFile);
  free(dateVar);
  free(logMessage);
  fclose(logFile);
  
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


void getCorrespondingProcessName(int PID, char * processName) {
  // populates processName with 
  // the process name that correspond 
  // to process id (PID)

  FILE * fp;
  char * nameOfPIDSbuffer = malloc(100);
  char * nameOfPIDScommand = malloc(150);

  //ps -e outputs
  //   PID TTY          TIME CMD
  //   values ....
  snprintf(nameOfPIDScommand, 140, "ps -e | grep '%d' | awk '{print $4}'", PID);
  //printf("%s \n", command);

  fp = popen(nameOfPIDScommand, "r");
  if (fp != NULL) {
    fgets(nameOfPIDSbuffer, 15, fp);
    processName = nameOfPIDSbuffer;
    //printf("%s number of PIDs are %d \n", processName, numberOfPIDs);
  }

  pclose(fp);
  free(nameOfPIDSbuffer);
  free(nameOfPIDScommand);
  
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


// kill previous procnannies
void killOldProcNannies()  {
  int numberOfProcNanniePIDs = getNumberOfPIDsForProcess("procnanny");
  
  if (numberOfProcNanniePIDs > 1) {
    char * inputBuffer = malloc(150);
    FILE * fp;
    //printf("found %d processes of procnanny running. Commencing deletion... \n", numberOfProcNanniePIDs);
    int * procNanniePIDs = malloc(numberOfProcNanniePIDs + 50);
    getPIDs("procnanny", procNanniePIDs, numberOfProcNanniePIDs);

    // remove current pid
    int currentProcPID = getpid();
    int tmpCounter = 0;
    for (tmpCounter = 0; tmpCounter < numberOfProcNanniePIDs; tmpCounter = tmpCounter + 1) {
      if (currentProcPID != procNanniePIDs[tmpCounter]) {
	snprintf(inputBuffer, 140, "kill -9 %d", procNanniePIDs[tmpCounter]);
	//printf("%s \n", inputBuffer);

	if ((fp = popen(inputBuffer, "r")) != NULL) {
	  snprintf(inputBuffer, 120, "An old procnanny (%d) was killed \n",  procNanniePIDs[tmpCounter]);
	  writeToLogs("Warning", inputBuffer);
	}

      }
    }


    free(inputBuffer);
    free(procNanniePIDs);
  }
  
}
 


void writeParentPIDToLogs()  {
  char * parentPIDMessage;
  parentPIDMessage = malloc(300);

  snprintf(parentPIDMessage, 290, "Parent process is PID %d. \n", getpid());
  writeToLogs("Info", parentPIDMessage);
      
  free(parentPIDMessage);
  return;
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
