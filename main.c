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

void getDate(char * dateVar);
void clearLogs();

int totalProcsKilled = 0;
static int * freeChildren;
char * fileName = NULL;

void writeToLogs(char * logLevel, char * message);
int countNumberOfPIDs(int PID);
void getPIDs(char * processName, int * PIDs, int numberOfPIDs);
int getNumberOfPIDsForProcess(char * processName);
int waitAndKill(char * processName, int processLifeSpan, int * PIDs, int numberOfPIDs);
void killOldProcNannies();
void monitor(char * processName, int processLifeSpan);
int updateKillCount(int n);
void sighup_handler(int signum);
void sigint_handler(int signum);
void readAndExecute();
void wrap_up();
void dispatch();
int newProcessToChild[2]; // p2c[0] gets written to on parent, p2c[1] gets read on child
int killCountPipe[2]; // killCountPipe[0] gets written to on child, killCountPipe[1] gets read on parent
int numberOfChildren = 0;

int main(int argc, char *argv[]) {

  freeChildren = mmap(NULL, sizeof * freeChildren, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
  *freeChildren = 0;
  signal(SIGHUP, sighup_handler);
  signal(SIGINT, sigint_handler);
  
  if (pipe(newProcessToChild) < 0) {
    printf("pipe error");
    exit(0);
  }

  if (pipe(killCountPipe) < 0) {
    printf("pipe error");
    exit(0);
  }

  fileName = argv[1];

  clearLogs();
  killOldProcNannies();


  readAndExecute();
  printf("parent in main, going to wrap_up\n");
  wrap_up();
  return 0;
}


void sighup_handler(int signum) {
  signal(SIGHUP, sighup_handler);

  writeToLogs("Info", "Caught SIGHUP. Configuration file 'nanny.config' re-read.\n");
  printf("Caught SIGHUP. Configuration file 'nanny.config' re-read.\n");

  readAndExecute();

  printf("beginning wrap_up \n");
  wrap_up();
}

void sigint_handler(int signum) {
  signal(SIGINT, sigint_handler);

  int i = 0;
  for (i = 0; i < numberOfChildren; i++) {
  
    printf("from parent: printing before writing to pipe \n");
    /* write message start */
    close(newProcessToChild[0]);
    char tmpStr2[12];
    strcpy(tmpStr2,"-1");
    //  sprintf(tmpStr2, "%s", processName);
    write(newProcessToChild[1], tmpStr2, strlen(tmpStr2) + 1);
    /* write message end */
    printf("from parent: done writing to pipe \n");

  }


  // update kill count before closing
  int n = -1;
  n = updateKillCount(5); // 5 is random value indicating size of what to read from pipe     
  
  while(n!=0) {
    n = updateKillCount(n);        
  }



  char * message;
  message = malloc(300);

  //printf("from parent %d \n", getpid());
  snprintf(message, 290, "Caught SIGINT. Exiting cleanly. %d process(es) killed. \n", totalProcsKilled);
  writeToLogs("Info", message);
  printf("%s", message);

  free(message);

  printf("According to us, we got a kill count of %d \n", totalProcsKilled);
  
  exit(0);


  
}


int updateKillCount(int n) {

  // reads pipe killCountPipe for message indicating the kills
  // process have committed and updates 
  // parent-controlled totalProcsKilled variable
  
  /* read message start */
  //printf("preparing to read messages \n");
  close(killCountPipe[1]);
  char pipeBuffer[17];
  n = read(killCountPipe[0], pipeBuffer, n);
  //  printf("%d is what we got and n = %d \n", atoi(pipeBuffer), n);
  /* read message end */
  //  printf("finished reading messages \n");

  if (n!=0) {
    // in this scope, we know that a child process just ended
    // so we can incremement freeChildren variable
    printf("%d is what we got and n = %d \n", atoi(pipeBuffer), n);

    totalProcsKilled += atoi(pipeBuffer);
    //freeChildren++;
  }
  
  return n;
}

void monitor(char * processName, int processLifeSpan) {
  // child portion of procnanny
  // monitors process "processName" and kills it
  // when processLifeSpan(seconds) is over   
  //
  // - generates its own variables and frees as needed
  // 


  // child process
  int numberOfPIDs = 0;
  int counter = 0;
  char * dateVar = malloc(30);
  char * message;
  char * startOfMessage;
  int * PIDs;

    
  // check to make sure process "processName" is still active 
  if (getNumberOfPIDsForProcess(processName)  < 1) {
    char * message;
    message = malloc(300);

    snprintf(message, 290, "No '%s' processes found. \n", processName);
    writeToLogs("Info", message);
      
    free(message);
  }


  numberOfPIDs = getNumberOfPIDsForProcess(processName);
  printf("we are %d \n", getpid());
  printf("for %d %s we have  the number of PIDs is equal to %d \n", getpid(), processName, numberOfPIDs);
  PIDs = malloc(numberOfPIDs + 100);
  message = malloc(300);
  startOfMessage = message;

  getPIDs(processName, PIDs, numberOfPIDs);

  // print out messages to logs regarding which
  // processes are being monitored
  for (counter = 0; counter < numberOfPIDs; counter = counter + 1) {

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
      
      

  int procsKilled = waitAndKill(processName, processLifeSpan, PIDs, numberOfPIDs);

  printf("from child: printing before writing to pipe \n");
  /* write message start */
  close(killCountPipe[0]);
  char tmpStr[5];
  sprintf(tmpStr, "%d", procsKilled);
  write(killCountPipe[1], tmpStr, 5);
  /* write message end */
  printf("from child: done writing to pipe \n");

  free(message);
  free(dateVar);
  free(PIDs);
  //  fclose(fp2);
  printf("incrementing freeChildren\n");
  *freeChildren = *freeChildren + 1;

    

}

void dispatch() {
  // reads pipe and transfer info to funcion monitor
  
  int n = 0;
  while(n == 0){
    /* read message start */
    printf("from child: preparing to read messages \n");

    close(newProcessToChild[1]);
    char pipeBuffer[100];
    n = read(newProcessToChild[0], pipeBuffer, 99);
    printf("from child: %s is what we got and n = %d \n", pipeBuffer, n);
    /* read message end */
    printf("from child: finished reading messages \n");

    if (strcmp(pipeBuffer, "-1") == 0) {
      printf("I should close now\n");

      exit(0);
      //break;

    }
    else {

      int processLifeSpan = 0;
      char * processName;
      char * tmpL = strchr(pipeBuffer, ' ');
      processLifeSpan = atoi(tmpL);
      strncpy(pipeBuffer, pipeBuffer, strlen(pipeBuffer) - strlen(tmpL));
      pipeBuffer[strlen(pipeBuffer) - strlen(tmpL)] = '\0'; // removes new pipeBuffer
      processName = pipeBuffer;

      monitor(processName, processLifeSpan);
    }
    
  }

  //exit(0);
  
}


void readAndExecute() {
  printf("starting readAndExecute\n");

  FILE * fp2;
  if (fileName == NULL) {
    exit(0);
  }
  fp2 = fopen(fileName,"r");
  if (fp2 == NULL) { 
    clearLogs();
    writeToLogs("Error", "Configuration file not found.\n");
    printf("Configuration file not found.\n");
    exit(0); 
  }


  char * line = NULL;
  size_t len = 0; 
  ssize_t read1;
  //int status = 0;
  int processLifeSpan = 0;
  char * processName = NULL;
  int pid = 0;

  char delimiter = ' ';

  while ((read1 = getline(&line, &len, fp2)) != -1) {
    printf("from parent: starting main while \n ");
    
    char * tmp = strchr(line, delimiter);
    processLifeSpan = atoi(tmp);
    strncpy(line, line, strlen(line) - strlen(tmp));
    line[strlen(line) - strlen(tmp)] = '\0'; // removes new line
    processName = line;


    printf("%s \n", processName);
    printf("%d \n", processLifeSpan);
    printf("freeChildren is %d \n", *freeChildren);
    
    if (*freeChildren > 0) {
      // write to pipe newProcessToChild
      printf("freeChildren is %d, preparing to reuse child", *freeChildren);
      printf("from parent: printing before writing to pipe \n");
      /* write message start */
      close(newProcessToChild[0]);
      char tmpStr[120];
      sprintf(tmpStr, "%s %d", processName, processLifeSpan);
      write(newProcessToChild[1], tmpStr, strlen(tmpStr) + 1);
      free(tmpStr);
      /* write message end */
      printf("from parent: done writing to pipe \n");

      printf("decing freeChildren\n");

      *freeChildren = *freeChildren - 1;
      continue;
    }

    pid = fork();
    if (pid == 0) {
      
      // child portion
      printf("Spawned A child for process: %s \n", processName);
      fclose(fp2);
      monitor(processName, processLifeSpan);
      while(1) {
	printf("staring dispatch \n");
	dispatch();

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

  }

  fclose(fp2);

  printf("done with readAndExecute \n");
}

void wrap_up() {
  
  // performs end of life operations
  
  printf("parent is wrapping up\n");
  

  printf("going into waiting\n");
  while(1) {
    sleep(5);
    
    // check for processes
    
  }
  /*
    Wed. 28
    parent stops at above statement waiting for children to exit.
   */
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

  // returns size of int array containging 
  // the PIDs.
  // Use the size to walk through the array

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

int waitAndKill(char * processName, int processLifeSpan, int * PIDs, int numberOfPIDs) {
  char * inputBuffer = malloc(150);
  char * startOfInputBuffer = inputBuffer;
  int killed = 0;
  // processLifeSpan is in seconds
  sleep(processLifeSpan);

  FILE * fp;
  int counter = 0;

  for (counter = 0; counter < numberOfPIDs; counter = counter + 1) {

    if (countNumberOfPIDs(PIDs[counter]) == 1) {
      snprintf(inputBuffer, 140, "kill -9 %d", PIDs[counter]);
      //printf("%s \n", command);
      //    fp = popen(inputBuffer, "r");

      if (PIDs[counter] != -1 && (fp = popen(inputBuffer, "r")) != NULL) {
	killed = killed + 1;

	// during first loop
	if (killed == 1) {
	  inputBuffer += sprintf(inputBuffer, "PID (%d", PIDs[counter]);
	}

	// default loop
	else {
	  inputBuffer += sprintf(inputBuffer, ", %d", PIDs[counter]);	  
	}


      } 
    }
    else {
      snprintf(inputBuffer, 290, "PID (%d) (%s) died before %d seconds. \n", PIDs[counter], processName, processLifeSpan);
      writeToLogs("Info", inputBuffer);
      
    }
    
  }

  if (killed > 0) {
    inputBuffer += sprintf(inputBuffer, ") (%s) killed after exceeding %d seconds. \n", processName, processLifeSpan);	  
    inputBuffer = startOfInputBuffer;
    writeToLogs("Action", inputBuffer);
    
  }



  while ((numberOfPIDs = getNumberOfPIDsForProcess(processName)) > 0) {
    getPIDs(processName, PIDs, numberOfPIDs);
    snprintf(inputBuffer, 140, "kill -9 %d", PIDs[0]);
    //printf("%s \n", command);
    //    fp = popen(inputBuffer, "r");

    if (PIDs[0] != -1 && (fp = popen(inputBuffer, "r")) != NULL) {
      //printf("process %s killed\n", processName);
      //      snprintf(message, 290, "PID (%d) (%s) killed after exceeding %d seconds. \n", PIDs[0], processName, processLifeSpan);
      //      writeToLogs("Action", message);
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



