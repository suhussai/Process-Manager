#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_


struct node;

struct node * init(char *);
int getSize(struct node * );
void addNode(struct node *, char * );
void removeNode(struct node *, char *);
int searchNodes(struct node *, char *);
void freeList(struct node *);

#endif
