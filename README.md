# procnanny
Assignment1 CMPUT_379: C program to fork processes to monitor user specified processes  

procnanny --- 

C program to monitor processes. Requires a configuration file, listing the 
processes to monitor and the number of seconds before executing the process.
The procnanny.server communicates with clients updating them on which processes
to monitor and for how long. The procnanny.server is responsible for informing 
the procnanny.clients and the procnanny.clients are responsible for executing
the prorams specified by the procnanny.server.
How to Run --- 

To compile the file:

$ make

To Run the program:

1. set the env variable 
$ export PROCNANNYLOGS="logsProc"
2. set the other env variable
$ export PROCNANNYSERVERINFO='logsProcServerInfo'

To start the server
3. make s 

To start a client
4. make c

OR

./procnanny.server /path/to/configFile
./procnanny.client hostNameOfProcnannyServer portNumberOfProcnannyServer
Makefile can be updated for different circumstances.



