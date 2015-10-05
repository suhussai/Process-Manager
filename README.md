# procnanny
Assignment1 CMPUT379: C program to fork processes to monitor user specified processes  

Name: ***REMOVED***

Student Number: ***REMOVED***

Unix ID: ***REMOVED***

Lecture Section: ***REMOVED***

Instructor's Name: ***REMOVED***

Lab Section: ***REMOVED***

TA's Name: ***REMOVED***

procnanny --- 

C program to monitor processes. Requires a configuration file, with the first line
indicating the amount of time. in seconds, the program must allow a process to run
before killing it. Each line after the first line lists the processes that must be monitored.

How to Run --- 

To compile the file:

$ make

To Run the program:

1. set the env variable 
$ export PROCNANNYLOGS="logsProc"

2. run the program while passing the configuration file as the argument
$ ./procnanny /full/path/to/configurationFile



