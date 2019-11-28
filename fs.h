/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include "sync.h"

typedef struct bucket{
	node *bstRoot;
	int nextINumber;
	syncMech bstLock;
}bucket;

typedef struct tecnicofs {
    bucket **trees;
} tecnicofs;

int obtainNewInumber(bucket* bst);
tecnicofs* new_tecnicofs(int numbuckets);
bucket *new_bucket(tecnicofs* fs, int i);
void free_tecnicofs(tecnicofs* fs, int numbuckets);
void create(bucket* bckt, char *name, int inumber);
void delete(bucket* bckt, char *name);
int lookup(bucket* bckt, char *name);
void change_name(bucket* bckt1, bucket* bckt2, char *name, char *newName);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs, int numbuckets);

#endif /* FS_H */
