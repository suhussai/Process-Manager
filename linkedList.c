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
#include "linkedList.h"



struct node {
  char * value;
  struct node * next;
};

struct node * init(char * headValue) {

  struct node * head = malloc(sizeof(struct node));
  //  char * headValue = malloc(strlen(value)*sizeof(value));
  head->value = malloc(strlen(headValue)*sizeof(headValue));
  strcpy(head->value, headValue);
  //  head->value = headValue; 
  head->next = NULL;

  return head;
  
}

int getSize(struct node * head) {
  struct node * currentNode = head;
  int size = 0;
  while (currentNode != NULL && currentNode->next != NULL) {
    currentNode = currentNode->next;
    size++;
  }

  return size + 1;
}


void addNode(struct node * head, char * value) {
  struct node * currentNode = head;
  while (currentNode != NULL && currentNode->next != NULL) {
    currentNode = currentNode->next;
  }

  // we are at the last node
  struct node * newNode = malloc(sizeof(struct node));
  //  char * newNodeValue = malloc(strlen(value)*sizeof(value));
  newNode->value = malloc(strlen(value)*sizeof(value));
  strcpy(newNode->value, value);
  //  newNode->value = newNodeValue; 
  currentNode->next = newNode;
  newNode->next = NULL;
  //free(newNodeValue);

}

void removeNode(struct node *head, char * valueToBeRemoved) {
  struct node * currentNode = head;
  struct node * previousNode = head;
  while (currentNode != NULL && currentNode->next != NULL &&
	 strcmp(currentNode->value, valueToBeRemoved) != 0 ) {
    previousNode = currentNode;
    currentNode = currentNode->next;
  }

  if (strcmp(currentNode->value, valueToBeRemoved) == 0) {
    previousNode->next = currentNode->next;    
    free(currentNode->value);
    free(currentNode);  
  }
  else {
    printf("Node not found.\n");
  }
  
}

int searchNodes(struct node * head, char * valueToSearch) {
  // returns location of nodes
  // if it exists, otherwise
  // it returns -1

  struct node * currentNode = head;
  int location = 0;
  while (currentNode != NULL && currentNode->next != NULL &&
	 strcmp(currentNode->value,valueToSearch) != 0) {
    printf("value is %s\n", currentNode->value);
    currentNode = currentNode->next;
    location++;
  }

  if (strcmp(currentNode->value,valueToSearch) == 0) {
    return location + 1;
  }
  else {
    return -1;
  }
  
}

void freeList(struct node * head) {
  struct node * currentNode = head;
  struct node * nextNode = head;

  printf("we in\n");

  while (currentNode != NULL) {
    nextNode = currentNode->next;
    free(currentNode->value);      
    free(currentNode);
    currentNode = nextNode;
  }
  
  
}

/* int main() { */

/*   printf("beginning \n"); */

/*   struct node * head = malloc(sizeof(struct node)); */
/*   head->next = NULL; */
/*   char * process = malloc(8);  */
/*   strcpy(process,"firefox"); */
/*   head->value = process; */
/*   printf("size before head: %d \n", getSize(head)); */
/*   head->value = process; */
/*   printf("size after head: %d \n", getSize(head)); */

/*   addNode(head, "emacs"); */
/*   printf("size after adding is: %d \n", getSize(head)); */


/*   addNode(head, "ping"); */
/*   printf("size after adding is: %d \n", getSize(head)); */

/*   addNode(head, "ping2"); */
/*   printf("size after adding is: %d \n", getSize(head)); */


/*   addNode(head, "ping3"); */
/*   printf("size after adding is: %d \n", getSize(head)); */

/*   printf("ping is located at location: %d", searchNodes(head, "ping")); */

/*   int i = 1; */

/*   struct node * cursorNode = head; */
/*   printf("\n\n\n"); */
/*   for (i = 1; i < getSize(head); i++) { */
/*     printf("value:%s\n", cursorNode->value); */
/*     cursorNode = cursorNode->next; */
/*   } */


/*   //  removeNode(head, "ping"); */
/*   //removeNode(head, "emacs"); */
/*   printf("size after removing is: %d \n", getSize(head)); */


/*   freeList(head); */
/*   printf("done \n"); */
/*   return 0; */
/* } */


