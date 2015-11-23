#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */
/* This is a sample client program for the number server. The client and */
/* the server need not run on the same machine.				 */
/* --------------------------------------------------------------------- */

/* void monitorProcess(char * processName, int processLifeSpan) { */
/*   // takes processName and lifeSpan  */
/*   // converts processName to PIDs  */
/*   // and monitors each PID by either  */
/*   // spawning a child for that process or  */
/*   // or reusing a "unbusy" child depending */
/*   // on the global freeChildren variable. */
  

/*   debugPrint("startin monitor Process for %s \n", processName); */
/*   char * message; */
/*   char * startOfMessage; */
/*   int * PIDs; */
/*   int counter = 0; */
/*   int pid = 0; */
/*   int parentPID = getpid(); */
  
/*   int numberOfPIDs = 0; */
/*   // check to make sure process "processName" is still active  */
/*   if ((numberOfPIDs = getNumberOfPIDsForProcess(processName))  < 1) { */
/*     char * message; */
/*     message = malloc(300); */

/*     snprintf(message, 290, "No '%s' processes found. \n", processName); */
/*     writeToLogs("Info", message); */
      
/*     free(message); */
/*     return; */
/*   } */
      
/*   debugPrint("we are parent: %d \n", getpid()); */
/*   debugPrint("for %d %s we have  the number of PIDs is equal to %d \n", getpid(), processName, numberOfPIDs); */
/*   PIDs = malloc(numberOfPIDs + 100); */
/*   message = malloc(300); */
/*   startOfMessage = message; */

/*   getPIDs(processName, PIDs, numberOfPIDs); */

/*   // print out messages to logs regarding which */
/*   // processes are being monitored */
/*   for (counter = 0; counter < numberOfPIDs; counter = counter + 1) { */

/*     if (monitoredPIDs != NULL && searchNodes(monitoredPIDs, processName, PIDs[counter]) != -1) { */
/*       // only call monitor process if  */
/*       // process has more than 0 pids active */
/*       // and it is not in monitoredPIDs linked list */
/*       // ie it is not being monitored already */
/*       debugPrint("process %s with pid %d is already being monitored \n", processName, PIDs[counter]); */
/*       continue; */
/*     } */


/*     // during first loop */
/*     if (counter == 0) { */
/*       message += sprintf(message, "Initializing monitoring of process '%s' (%d", processName, PIDs[counter]); */
/*     } */
/*     // default loop */
/*     else { */
/*       message += sprintf(message, ", %d", PIDs[counter]); */
/*     } */

/*     // during last loop */
/*     if (counter + 1 == numberOfPIDs) { */
/*       message += sprintf(message, ") \n"); */
/*       message = startOfMessage; */
/*       writeToLogs("Info", message); */

/*     } */
/*   } */


/*   for (counter = 0; counter < numberOfPIDs; counter = counter + 1) { */

/*     if (monitoredPIDs != NULL && searchNodes(monitoredPIDs, processName, PIDs[counter]) != -1) { */
/*       // only call monitor process if  */
/*       // process has more than 0 pids active */
/*       // and it is not in monitoredPIDs linked list */
/*       // ie it is not being monitored already */
/*       debugPrint("process %s with pid %d is already being monitored \n", processName, PIDs[counter]); */
/*       continue; */
/*     } */


/*     debugPrint("adding pid %d to list \n", PIDs[counter]); */
/*     if (monitoredPIDs) { */
/*       debugPrint("ADDDING  pid %d to list! \n", PIDs[counter]); */
/*       addNode(monitoredPIDs, processName, PIDs[counter]); */
/*     } */
/*     else { */
/*       debugPrint("initialzing pid %d to list!\n", PIDs[counter]); */
/*       monitoredPIDs = init(processName, PIDs[counter]); */
/*     } */
/*     if (freeChildren > 0) { */
/*       // write to pipe newProcessToChild */
/*       debugPrint("freeChildren is %d, preparing to reuse child for %s pid %d\n", freeChildren, processName, PIDs[counter]); */
/*       //	  debugPrint("from parent: printing before writing to pipe \n"); */
/*       /\* write message start *\/ */
      
/*       // close(newProcessToChild[0]); */
/*       char tmpStr[62]; */
/*       sprintf(tmpStr, "%-20s#%20d!%20d", processName, PIDs[counter], processLifeSpan); */
/*       write(newProcessToChild[1], tmpStr, 62); */


/*       debugPrint("using children\n"); */
/*       freeChildren = freeChildren - 1; */
/*     } */
	
/*     else { */
/*       pid = fork(); */
/*       if (pid == 0) { */
      
/* 	// child portion */
/* 	debugPrint("Spawned A child %d for process: %s, pid %d \n",getpid(),  processName, PIDs[counter]); */
/* 	dispatch(parentPID, processName, PIDs[counter], processLifeSpan); */
	    
/* 	while(keepLooping == 1) { */
/* 	  debugPrint("%d child : starting dispatch \n", getpid()); */
/* 	  dispatch(parentPID, processName, -1, processLifeSpan); */
/* 	} */
	
	
/* 	free(PIDs); */
/* 	free(message); */
/* 	debugPrint("child dying....\n"); */
/* 	exit(0); */
	
/*       } */
/*       else if (pid < 0) { */
/* 	fprintf(stderr,"error in forking. \n"); */
/* 	exit(0); */
/*       } */
/*       else { */
/* 	numberOfChildren++; */
/* 	if (childPIDs){ */
/* 	  addNode(childPIDs, " ", pid); */
/* 	} */
/* 	else { */
/* 	  childPIDs = init(" ", pid); */
/* 	} */

/* 	//printf("parent process pid=%d \n", getpid()); */
/*       } */
	  
/*     } // spawning child end bracket */

/*   } // for loop end bracket */



/*   free(PIDs); */
/*   free(message); */
     

/* } */

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


int main(int argc, char * argv[]) 
{
	int	s, number;

	struct	sockaddr_in	server;

	struct	hostent		*host;
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
		printf("results from updateKillCount pipe read\n");
	        printf("name: %s \n", newP);
		printf("life span: %d \n", processLifeSpan);
	        printf("pid: %d \n", PID);

		close (s);
		fprintf (stderr, "Process %d gets number %s\n", getpid (),
			pipeBuffer);
		sleep (5);
	}
}




