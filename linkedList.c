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



struct node;
struct node * init(char * headValue, int headKey) {

  struct node * head = malloc(sizeof(struct node));
  //  char * headValue = malloc(strlen(value)*sizeof(value));
  head->value = malloc(strlen(headValue)*sizeof(headValue));
  strcpy(head->value, headValue);
  head->key = headKey;
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


void addNode(struct node * head, char * valueToAdd, int keyToAdd) {
  struct node * currentNode = head;
  while (currentNode != NULL && currentNode->next != NULL) {
    currentNode = currentNode->next;
  }

  // we are at the last node
  struct node * newNode = malloc(sizeof(struct node));
  //  char * newNodeValue = malloc(strlen(value)*sizeof(value));
  newNode->value = malloc(strlen(valueToAdd)*sizeof(valueToAdd));
  strcpy(newNode->value, valueToAdd);
  newNode->key = keyToAdd;
  //  newNode->value = newNodeValue; 
  currentNode->next = newNode;
  newNode->next = NULL;
  //free(newNodeValue);

}

void removeNode(struct node *head, char * valueToBeRemoved, int keyToBeRemoved) {
  struct node * currentNode = head;
  struct node * previousNode = head;
  while (currentNode != NULL && currentNode->next != NULL &&
	 strcmp(currentNode->value, valueToBeRemoved) != 0 &&
	 currentNode->key != keyToBeRemoved) {
    previousNode = currentNode;
    currentNode = currentNode->next;
  }

  if (strcmp(currentNode->value, valueToBeRemoved) == 0 &&
      currentNode->key == keyToBeRemoved) {
    previousNode->next = currentNode->next;    
    free(currentNode->value);
    free(currentNode);  
  }
  else {
    printf("Node not found.\n");
  }
  
}

int searchNodes(struct node * head, char * valueToSearch, int keyToSearch) {
  // returns location of nodes
  // if it exists, otherwise
  // it returns -1

  struct node * currentNode = head;
  int location = 0;
  while (currentNode != NULL && currentNode->next != NULL && (strcmp(currentNode->value,valueToSearch) != 0 || currentNode->key != keyToSearch)) {
    //printf("value is %s\n", currentNode->value);
    currentNode = currentNode->next;
    location++;
  }

  if (strcmp(currentNode->value,valueToSearch) == 0 &&
      currentNode->key == keyToSearch) {
    return location + 1;
  }
  else {
    return -1;
  }
  
}

void freeList(struct node * head) {
  struct node * currentNode = head;
  struct node * nextNode = head;

  while (currentNode != NULL) {
    nextNode = currentNode->next;
    free(currentNode->value);      
    free(currentNode);
    currentNode = nextNode;
  }
  
  
}

/* int main() { */

/*   printf("beginning \n"); */

/*   struct node * head = init("firefox", 0); */
/*   struct node * otherHead =  init("lame", 2123120); */

/*   addNode(head, "emacs", 2); */
/*   printf("size after adding is: %d \n", getSize(head)); */


/*   addNode(otherHead, "emacs", 2123); */
/*   printf("size after adding is: %d \n", getSize(otherHead)); */

/*   addNode(head, "ping", 3); */
/*   printf("size after adding is: %d \n", getSize(head)); */

/*   addNode(head, "ping2", 4); */
/*   printf("size after adding is: %d \n", getSize(head)); */


/*   addNode(head, "ping3", 5); */
/*   printf("size after adding is: %d \n", getSize(head)); */
  
/*   printf("ping is located at location: %d", searchNodes(head, "ping", 3)); */

/*   int i = 1; */

/*   struct node * cursorNode = head; */
/*   printf("\n\n\n"); */
/*   for (i = 0; i < getSize(head); i++) { */
/*     printf("value:%s, key=%d\n", cursorNode->value, cursorNode->key); */
/*     cursorNode = cursorNode->next; */
/*   } */

/*   cursorNode = otherHead; */
/*   printf("\n\n\n"); */
/*   for (i = 0; i < getSize(otherHead); i++) { */
/*     printf("value:%s, key=%d\n", cursorNode->value, cursorNode->key); */
/*     cursorNode = cursorNode->next; */
/*   } */



/*   //  removeNode(head, "ping"); */
/*   removeNode(head, "emacs", 2); */
/*   printf("size after removing is: %d \n", getSize(head)); */


/*   freeList(head); */
/*   printf("done \n"); */
/*   return 0; */
/* } */


