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
    snprintf(getPIDcommand, 140, "ps -e | grep '%s' | awk '{print $1}' | tail -n %d | head -n 1", processName, numberOfPIDs - counter);
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
  snprintf(numberOfPIDScommand, 140, "ps -e | grep '%s' | awk '{print $1}' | wc -l", processName);
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


static int  * numberOfProcessesKilled;

int main(int argc, char *argv[]) {

  numberOfProcessesKilled = mmap(NULL, sizeof * numberOfProcessesKilled, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  
  *numberOfProcessesKilled = 0;
  FILE * fp2;
  fp2 = fopen(argv[1],"r");
  if (fp2 == NULL) { 
    clearLogs();
    writeToLogs("Error", "Configuration file not found.\n");
    printf("Configuration file not found.\n");
    exit(0); 
  }


  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  int count = 1;
  int status;
  int processLifeSpan = 0;
  int pid;
  clearLogs();
  killOldProcNannies();
  
  while ((read = getline(&line, &len, fp2)) != -1) {
    //printf("line = %s", line);
    if (count == 1) {
      // process life span
      processLifeSpan = atoi(line);
      count = count + 1;
      continue;
    }

    line[strlen(line) - 1] = '\0'; // remove new line character
    char * processName = line;

    // only proceed to forking a new process if we even need to 
    if (getNumberOfPIDsForProcess(processName)  < 1) {
      char * message;
      message = malloc(300);

      snprintf(message, 290, "No '%s' processes found. \n", processName);
      writeToLogs("Info", message);
      
      free(message);
      continue;
    }
    pid = fork();
    if (pid == 0) {
      int numberOfPIDs = 0;
      int counter = 0;
      char * dateVar = malloc(30);
      char * message;
      char * startOfMessage;
      int * PIDs;

      numberOfPIDs = getNumberOfPIDsForProcess(processName);
      //printf("for %d %s we have  the number of PIDs is equal to %d \n", getpid(), processName, numberOfPIDs);
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
      // increment global variable keeping track of number of processes killed
      *numberOfProcessesKilled = *numberOfProcessesKilled + procsKilled;

      
      free(message);
      free(dateVar);
      free(PIDs);
      fclose(fp2);

      exit(0);
      //break;
    }
    else if (pid == -1) {
      printf("error in forking. \n");
      exit(1);
      
    }    
    else {
      //printf("parent process pid=%d \n", getpid()); 
    }

  }


  // wait for children to finish
  while((pid = wait(&status)) != -1) { }
  char * message;
  message = malloc(300);

  //printf("from parent %d \n", getpid());
  //printf("total number of processes killed are %d\n", *numberOfProcessesKilled);
  snprintf(message, 290, "Exiting. %d process(es) killed. \n", *numberOfProcessesKilled);
  writeToLogs("Info", message);

  free(message);

  fclose(fp2);
  munmap(numberOfProcessesKilled, sizeof * numberOfProcessesKilled);
  return 0;
}

