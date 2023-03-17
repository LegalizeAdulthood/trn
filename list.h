/* list.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

struct listnode {
    LISTNODE* next;
    /*LISTNODE* mid;*/
    long low;
    long high;
    char* data_high;
    char data[1];  /* this is actually longer */
};

struct list {
    LISTNODE* first;
    LISTNODE* recent;
    void (*init_node)(LIST*,LISTNODE*);
    long low;
    long high;
    int item_size;
    int items_per_node;
    int flags;
};

#define LF_ZERO_MEM	0x0001
#define LF_SPARSE	0x0002

void list_init(void);
LIST *new_list(long, long, int, int, int, void (*)(LIST *, LISTNODE *));
char *listnum2listitem(LIST *, long);
long listitem2listnum(LIST *, char *);
bool walk_list(LIST *, bool (*)(char *, int), int);
long existing_listnum(LIST *, long, int);
char *next_listitem(LIST *, char *);
char *prev_listitem(LIST *, char *);
void delete_list(LIST *);
