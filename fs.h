/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include "sync.h"
#include "constants.h"

typedef struct bucket{
	node *bstRoot;
	int nextINumber;
	syncMech bstLock;
}bucket;

typedef struct tecnicofs {
    bucket **trees;
} tecnicofs;

typedef struct fd{
	int iNumber;
	int mode;
}fd;

int obtainNewInumber(bucket* bst);
tecnicofs* new_tecnicofs(int numbuckets);
bucket *new_bucket(tecnicofs* fs, int i);
void free_tecnicofs(tecnicofs* fs, int numbuckets);
int create(tecnicofs* fs, char *name, int permissions, int uid);
int delete(tecnicofs* fs, char *name, fd* opened_files, int uid);
int lookup(bucket* bckt, char *name);
int change_name(tecnicofs* fs, char *name, char *newName, fd* opened_files, int uid);
int open_file(tecnicofs* fs, char* name, int uid, int mode, fd* opened_files);
int close_file(tecnicofs* fs, int filed, int uid, fd* opened_files);
char* read_file(tecnicofs* fs, int file_d, fd *opened_files, int len);
int write_file(tecnicofs* fs, int file_d, fd *opened_files, char* buffer);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs, int numbuckets);

#endif /* FS_H */
