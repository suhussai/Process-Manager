#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "memwatch.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "linkedList.h"


void getDate(char * dateVar);
void clearLogs();

int totalProcsKilled = 0;
int freeChildren = 0;
char * fileName = NULL;
int keepLooping = 1; // 1 = true, 0 = false
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
int newProcessToChild[2]; // p2c[0] gets written to on parent, p2c[1] gets read on child
int killCountPipe[2]; // killCountPipe[0] gets written to on child, killCountPipe[1] gets read on parent
int numberOfChildren = 0;
struct sigaction sa;
int rereadFromConfigFile = 1;
int parentPid = 0;
struct node * processList;
struct node * monitoredPIDs;
int messagesFromChildren = 0;

void monitorProcess(char * processName, int processLifeSpan) {
  // takes processName and lifeSpan 
  // converts processName to PIDs 
  // and monitors each PID by either 
  // spawning a child for that process or 
  // or reusing a "unbusy" child depending
  // on the global freeChildren variable.
  

  printf("startin monitor Process for %s \n", processName);
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
      
  printf("we are parent: %d \n", getpid());
  printf("for %d %s we have  the number of PIDs is equal to %d \n", getpid(), processName, numberOfPIDs);
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
      printf("process %s with pid %d is already being monitored \n", processName, PIDs[counter]);
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
      printf("process %s with pid %d is already being monitored \n", processName, PIDs[counter]);
      continue;
    }


    printf("adding pid %d to list \n", PIDs[counter]);
    if (monitoredPIDs) {
      printf("ADDDING  pid %d to list! \n", PIDs[counter]);
      addNode(monitoredPIDs, processName, PIDs[counter]);
    }
    else {
      printf("initialzing pid %d to list!\n", PIDs[counter]);
      monitoredPIDs = init(processName, PIDs[counter]);
    }
    if (freeChildren > 0) {
      // write to pipe newProcessToChild
      printf("freeChildren is %d, preparing to reuse child for %s pid %d\n", freeChildren, processName, PIDs[counter]);
      //	  printf("from parent: printing before writing to pipe \n");
      /* write message start */
      
      // close(newProcessToChild[0]);
      char tmpStr[62];
      sprintf(tmpStr, "%-20s#%20d!%20d", processName, PIDs[counter], processLifeSpan);
      write(newProcessToChild[1], tmpStr, 62);


      printf("using children\n");
      freeChildren = freeChildren - 1;
    }
	
    else {
      pid = fork();
      if (pid == 0) {
      
	// child portion
	printf("Spawned A child %d for process: %s, pid %d \n",getpid(),  processName, PIDs[counter]);
	dispatch(parentPID, processName, PIDs[counter], processLifeSpan);
	    
	while(keepLooping == 1) {
	  printf("%d child : starting dispatch \n", getpid());
	  dispatch(parentPID, processName, -1, processLifeSpan);
	}

	    
      }
      else if (pid == -1) {
	printf("error in forking. \n");
	exit(1);
      }
      else {
	numberOfChildren++;
	//printf("parent process pid=%d \n", getpid());
      }
	  
    } // spawning child end bracket

  } // for loop end bracket



  free(PIDs);
  free(message);
     

}

int main(int argc, char *argv[]) {

  parentPid = getpid();
  printf("!!!!!!!Parent PID = %d\n", parentPid);
  sa.sa_handler = sig_handler;
  // http://www.linuxjournal.com/files/linuxjournal.com/linuxjournal/articles/063/6391/6391l3.html
  // http://stackoverflow.com/questions/6343871/about-the-delivery-of-standard-signals

  sa.sa_flags = SA_SIGINFO;
  int errorSig = sigaction(SIGHUP, &sa, NULL);
  if (errorSig == -1) {
    printf("error in seting signal");
    exit(0);
  }

  printf("set handlers\n");
  
  if (pipe(newProcessToChild) < 0) {
    printf("pipe error");
    exit(0);
  }

  if (pipe(killCountPipe) < 0) {
    printf("pipe error");
    exit(0);
  }

  fcntl(killCountPipe[0], F_SETFL, O_NONBLOCK);

  fileName = argv[1];

  clearLogs();
  killOldProcNannies();


  readAndExecute();
  printf("parent in main, going to wrap_up\n");
  //  wrap_up();
  return 0;
}


void sig_handler(int signum) {
  int i = 0;

  switch(signum) {
    
  case 1:
    writeToLogs("Info", "Caught SIGHUP. Configuration file 'nanny.config' re-read.\n");
    printf("\n\n\nCaught SIGHUP. Configuration file 'nanny.config' re-read.\n\n\n");
    rereadFromConfigFile = 1;
    break;
  case 2:
    for (i = 0; i < numberOfChildren; i++) {
  
      //      printf("from parent: printing before writing to pipe \n");
      /* write message start */
      //close(newProcessToChild[0]);
      char tmpStr2[2];
      strcpy(tmpStr2,"-1");
      tmpStr2[2] = '\0';
      //  sprintf(tmpStr2, "%s", processName);
      write(newProcessToChild[1], tmpStr2, 62);
      /* write message end */
      //      printf("from parent: done writing to pipe \n");
      
    }
    
    char * message;
    message = malloc(300);

    //printf("from parent %d \n", getpid());
    snprintf(message, 290, "Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
    writeToLogs("Info", message);
    printf("%s", message);

    free(message);

    printf("According to us, we got a kill count of %d \n", totalProcsKilled);

    keepLooping = 0;

    break;
  case 10:
    printf("caught sigusr1\n");
    messagesFromChildren = messagesFromChildren + 1;
    //updateKillCount();        
    break;
  default:
    printf("idk\n");
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
  //  printf("from parent preparing to log messages \n");


  //close(killCountPipe[1]);
  char * pipeBuffer= malloc(62);
  printf("updating kill...\n");

  if (read(killCountPipe[0], pipeBuffer, 62) <= 0) {
    printf("found nothing on killCount pipe, returning\n");
    free(pipeBuffer);
    return;
  }

  char * tmpBuffer = pipeBuffer;
  
  char * inputBuffer = malloc(150);
  char * processName;
  int len = 0;
  int PID = 0;
  int processLifeSpan = 0;


 
  //  int n = 0;
  
  /* if (read(killCountPipe[0], pipeBuffer, 63) <= 0) { */
  /*   printf("found nothing on killCount pipe, returning\n"); */
  /*   return; */
  /* } */
  
  /* while(n == 0) { */
  /*   n = read(killCountPipe[0], pipeBuffer, 63);     */
  /* } */
  
  printf("results of updating kill: %s\n", pipeBuffer);
  //  printf("parent: %s is what we got and n = %d \n", pipeBuffer, n);


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
  printf("name: %s|| \n", newP);
  printf("life span: %d \n", processLifeSpan);
  printf("pid: %d \n", PID);



  if (searchNodes(monitoredPIDs, newP, PID) != -1) {
    printf("NOT REALLYremoving pid that was killed.\n");
    //removeNode(monitoredPIDs, newP, PID);    
  }

  totalProcsKilled++;
  
  printf("PID (%d) (%s) killed after exceeding %d seconds. \n", PID, processName, processLifeSpan);
  sprintf(inputBuffer, "PID (%d) (%s) killed after exceeding %d seconds. \n", PID, processName, processLifeSpan);
  writeToLogs("Action", inputBuffer);
  

  free(tmpBuffer);
  free(inputBuffer);
  //free(pipeBuffer);
  free(processName);
  //  printf("!!!!!!!!!!!!!!!!incrementing freeChildren\n");  
  //printf("%d freeChildren is %d\n", getpid(), freeChildren);
  
  freeChildren = freeChildren + 1;
  //printf("%d before freeChildren is %d\n", getpid(), freeChildren);
  
}

void dispatch(int parentPid, char * processName, int processPID, int processLifeSpan) {
  // reads if PID not provided
  // pipe and transfer info to funcion monitor
  //  int n = 0;
  /* read message start */
  char * pipeBuffer= malloc(62);
  char * newProcessName;
  if (processPID == -1 ) {
    // if we need to get process id
    // we look to getting it from the pipe
    //    printf("from child: preparing to read messages \n");

    close(newProcessToChild[1]);
    read(newProcessToChild[0], pipeBuffer, 62);
    //    printf("from child: %s is what we got and n = %d \n", pipeBuffer, n);
    /* read message end */
    //    printf("from child: finished reading messages \n");

    // after we've retrieved info from pipe
    // check if it is a shutdown message
    if (atoi(pipeBuffer) == -1) {
      printf("I should close now\n");
      keepLooping = 0;
      free(pipeBuffer);
      return;
    }
    else {
      
      processLifeSpan = 0;
      int len = 0;
      processPID = 0;

      len = strchr(pipeBuffer,'#') - pipeBuffer;
      newProcessName = malloc(len+1);
      memcpy(newProcessName, pipeBuffer, len);
      newProcessName[len] = '\0'; 
      pipeBuffer = strchr(pipeBuffer,'#');
      processPID = atoi(pipeBuffer+1);
      processLifeSpan = atoi(strchr(pipeBuffer,'!')+1);      
    }
    
  } 
  else {
    newProcessName = malloc(strlen(processName));
    strcpy(newProcessName, processName);
    //newProcessName = processName;
  }

  // default action
  // either we've retrieved processName, processPID, processLifeSpan
  // from the pipe or
  // we were given it in the argument of the function when it was called

  if (waitAndKill(processLifeSpan, processPID) > 0) {
    //    printf("from child: sending log info  to pipe \n");
    /* write message start */
    close(killCountPipe[0]);
    char tmpStr[62];

    sprintf(tmpStr, "%-20s#%20d!%20d", newProcessName, processPID, processLifeSpan);
    printf("child %d writing this to pipe: %s \n", getpid(), tmpStr);
    int w = write(killCountPipe[1], tmpStr, 62);
    while (w != 62) {
      w = write(killCountPipe[1], tmpStr, 62);
      printf("%s\n",strerror(errno));
    }
    printf("child %d w was %d \n",getpid(), w);

    // signal parent
    printf("NOT SIGNALLING PARENT \n");
    //kill(parentPid,SIGUSR1);
  }
  
  
  free(newProcessName);
  free(pipeBuffer);
  //exit(0);
  
}


void readAndExecute() {
  printf("starting readAndExecute\n");

  sa.sa_handler = sig_handler;
  int errorSig = sigaction(SIGHUP, &sa, NULL);
  errorSig += sigaction(SIGINT, &sa, NULL);
  errorSig += sigaction(SIGUSR1, &sa, NULL);
  if (errorSig < 0) {
    printf("error in setting signal");
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
  //  int pid = 0;


  // child process
  //  int counter = 0;

  char delimiter = ' ';

  while(keepLooping == 1) {


    if (rereadFromConfigFile == 1) {
      fp2 = fopen(fileName,"r");
      if (fp2 == NULL) { 
	clearLogs();
	writeToLogs("Error", "Configuration file not found.\n");
	printf("Configuration file not found.\n");
	exit(0); 
      }
    }
    
    printf("reread = %d\n", rereadFromConfigFile);
    while ((read1 = getline(&line, &len, fp2)) != -1 && rereadFromConfigFile == 1) {
      printf("from parent: starting main while \n ");
    
      char * tmp = strchr(line, delimiter);
      processLifeSpan = atoi(tmp);
      strncpy(line, line, strlen(line) - strlen(tmp));
      line[strlen(line) - strlen(tmp)] = '\0'; // removes new line
      processName = line;

      printf("%s \n", processName);
      printf("%d \n", processLifeSpan);
      printf("freeChildren is %d \n", freeChildren);
      printf("adding all  nodes \n");
      
      if (processList && searchNodes(processList, processName, processLifeSpan)) {
	printf("%d adding node \n", getpid());
	addNode(processList, processName, processLifeSpan);
      }
      else {
	printf("%d processListing node \n", getpid());
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

    while (messagesFromChildren > 0) {
      printf("found a message from a child\n");
      updateKillCount();
      messagesFromChildren = messagesFromChildren - 1;
    }
    //sleep(10);
    //kill(getpid(), SIGHUP);
    //check for processes 
    printf("********************\n");

    printf("number of freechildren is %d: \n", freeChildren);
    printf("number of messages from children is %d: \n", messagesFromChildren);
    struct node * cursorNode;
    int i = 0;
    printf("processes we monitored are: \n");
    cursorNode = monitoredPIDs;
    if (monitoredPIDs) {
      for (i = 0; i < getSize(monitoredPIDs); i++) {
	printf("value: %s, key: %d\n", cursorNode->value, cursorNode->key);
	cursorNode = cursorNode->next;
      }
      
    }


    printf("process we got are: \n");

    cursorNode = processList;

    for (i = 0; i < getSize(processList); i++) {
      printf("calling monitorProcess \n");
      printf("for %s, %d \n",cursorNode->value, cursorNode->key);

      if (getNumberOfPIDsForProcess(cursorNode->value) > 0) {
	monitorProcess(cursorNode->value, cursorNode->key);   	
      }
      else {
	printf("No %s proc found.\n", cursorNode->value);
      }

      cursorNode = cursorNode->next;
    }



    
  }

  fclose(fp2);
  freeList(processList);
  freeList(monitoredPIDs);
  printf("done with readAndExecute \n");


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
    printf("PROCNANNYLOGS variable not set\n");
    exit(0);
  }
  
  if ((logFile = fopen(getenv("PROCNANNYLOGS"), "w+")) == NULL) {
    printf("Error in opening the PROCNANNYLOGS file \n");
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
    if (getPIDfp != NULL) {
      fgets(getPIDbuffer, 15, getPIDfp);
      PIDs[counter] = atoi(getPIDbuffer);
      //printf("%s process has PID %d \n", processName, PIDs[counter]);
    }
    
  }

  pclose(getPIDfp);
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
 


