/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */
#include "constants.h"
#include "fs.h"
#include "lib/bst.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sync.h"
#include "lib/inodes.h"
#include "lib/hash.h"

int numberBuckets;


int obtainNewInumber(bucket* bst) {
	int newInumber = ++(bst->nextINumber);
	return newInumber;
}

tecnicofs* new_tecnicofs(int numbuckets){
	numberBuckets=numbuckets;
	tecnicofs* fs = malloc(sizeof(tecnicofs));
	bucket* bkct;
	if (!fs) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}
	fs->trees=malloc(sizeof(bucket)*numbuckets);
	if(!fs->trees){
		perror("failed to allocate hash bst's");
		exit(EXIT_FAILURE);
	}
	for(int i=0; i<numbuckets; i++){
		bkct= new_bucket(fs, i);
		bkct->nextINumber = 0;
		bkct->bstRoot = NULL;
		sync_init(&(bkct->bstLock));
	}
	return fs;
}

bucket *new_bucket(tecnicofs* fs, int i){
	fs->trees[i]=malloc(sizeof(bucket));
	if(!fs->trees[i]){
		perror("failed to allocate bst");
		exit(EXIT_FAILURE);
	}
	return fs->trees[i];
}

void free_tecnicofs(tecnicofs* fs, int numbuckets){
	bucket* bucket_aux;
	for(int i = 0; i<numbuckets; i++){
		bucket_aux=fs->trees[i];
		free_tree(bucket_aux->bstRoot);
		sync_destroy(&(bucket_aux->bstLock));
	}
	free(fs);
}

int create(tecnicofs* fs, char *name, int permissions, int uid){
	int iNumber, position;
	permission othersPerm, ownerPerm; 

	position=hash(name, numberBuckets);
	bucket* bckt=fs->trees[position];

    othersPerm=permissions % 10;
    ownerPerm=permissions / 10;
    //verifica se ficheiro existe
    if(lookup(bckt, name)!=-1)
        return -4;

    if((iNumber = inode_create(uid, ownerPerm, othersPerm))==-1)
        return -11;

    sync_wrlock(&(bckt->bstLock));
	bckt->bstRoot = insert(fs->trees[position]->bstRoot, name, iNumber);
	sync_unlock(&(bckt->bstLock));
	return 0;
}

int delete(tecnicofs* fs, char *name, fd *opened_files, int uid){
	int i, iNumber, position, len=0;
	permission othersPerm, ownerPerm; 
    uid_t owner;

    char *buffer=malloc(sizeof(char)*len);
	position=hash(name, numberBuckets);
	bucket* bckt=fs->trees[position];

    iNumber = lookup(bckt, name);
    if(iNumber==-1)
        return -5;
    
    for(i=0; i<MAX_OPENED_FILES; i++){
        if(iNumber==opened_files[i].iNumber)
            return -9;
    }
  
	if(inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len)==-1)
        return -11;

    if(owner!=uid){
        return -6;
    }
    
    if(inode_delete(iNumber)==-1){
        return -11;
    }

	sync_wrlock(&(bckt->bstLock));
	bckt->bstRoot = remove_item(bckt->bstRoot, name);
	sync_unlock(&(bckt->bstLock));
	return 0;
}

int lookup(bucket* bckt, char *name){
	sync_rdlock(&(bckt->bstLock));
	int inumber = -1;
	node* searchNode = search(bckt->bstRoot, name);
	if ( searchNode ) {
		inumber = searchNode->inumber;
	}
	sync_unlock(&(bckt->bstLock));
	return inumber;
}

int change_name(tecnicofs* fs, char *name, char *newname, fd* opened_files, int uid){ 
	int i, flag=1, position, position2, iNumber, len=0;
	permission othersPerm, ownerPerm; 
    char *buffer=malloc(sizeof(char)*len);
    uid_t owner;
	//adicional sleep dentro dos trylocks
    position=hash(name, numberBuckets);
    position2=hash(newname, numberBuckets);
    if(lookup(fs->trees[position2], newname)!=-1)
        return -4;

    if((iNumber = lookup(fs->trees[position], name))==-1)
        return -5;

    for(i=0; i<MAX_OPENED_FILES; i++)
        if(iNumber==opened_files[i].iNumber)
	        return -9;

    if(inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len)==-1)
        return -11;

    if(owner!=uid)
        return -6;
    
    if(inode_delete(iNumber)==-1)
        return -11;

    if(inode_create(uid, ownerPerm, othersPerm)==-1)
        return -11;

    bucket* bckt1=fs->trees[position];
    bucket* bckt2=fs->trees[position2];

	while(1){
		if(sync_trylock(&(bckt1->bstLock))==0){
			if((&(bckt1->bstLock))==(&(bckt2->bstLock)))
				flag=0;
			if(sync_trylock(&(bckt2->bstLock))==0 || flag==0){
				node* searchNode = search(bckt1->bstRoot, name);
				node* searchNode2 = search(bckt2->bstRoot, newname);
				if ( searchNode && !searchNode2) {
					int inumber = searchNode->inumber;
					bckt1->bstRoot = remove_item(bckt1->bstRoot, name);
					bckt2->bstRoot = insert(bckt2->bstRoot, newname, inumber);
				}
				sync_unlock(&(bckt1->bstLock));
				if (flag==1)
					sync_unlock(&(bckt2->bstLock));
				break;
			}
			sync_unlock(&(bckt1->bstLock));
		}
	}
	return 0;
}

int open_file(tecnicofs* fs, char* name, int uid, int mode, fd* opened_files){
	int position, i, len=0, iNumber;
	permission othersPerm, ownerPerm; 
    uid_t owner;
	char* buffer = malloc(sizeof(char)*len);
	position=hash(name, numberBuckets);
    if((iNumber = lookup(fs->trees[position], name))==-1)
        return -5;

    if(inode_get(iNumber, &owner, &ownerPerm, &othersPerm, buffer, len)==-1)
        return -11;   

    if((owner==uid && (ownerPerm==3 || ownerPerm==mode)) || (owner!=uid &&(othersPerm==3 || ownerPerm==mode))){
        for(i=0; i<MAX_OPENED_FILES; i++)
            if(opened_files[i].iNumber==-1){
                opened_files[i].iNumber=iNumber;
                opened_files[i].mode=mode;
                return i;
            }
        return -7;
    }
    return -6;
}

int close_file(tecnicofs* fs, int file_d, int uid, fd *opened_files){
    if(opened_files[file_d].iNumber!=-1){
	    opened_files[file_d].iNumber=-1;
	    opened_files[file_d].mode=0;
	    return 0;
    }
    return -8;
}

char* read_file(tecnicofs* fs, int file_d, fd *opened_files, int len){
	int iNumber;
	char* msg, *buffer;
	permission othersPerm, ownerPerm; 
    uid_t owner;
	if(opened_files[file_d].iNumber!=-1){
        if(opened_files[file_d].mode==3 || opened_files[file_d].mode==2){
            iNumber=opened_files[file_d].iNumber;
            buffer=malloc(sizeof(char)*len);
            len = inode_get(iNumber, &owner,  &ownerPerm,  &othersPerm, buffer, len);
            if(len==-1){
                msg=malloc(sizeof(char));
                sprintf(msg, "%d", -11);
            }
            else{
	            msg=malloc(sizeof(char) *(len+1));
	            sprintf(msg, "%d ", len);
	            strcat(msg, buffer);
	        }
        }
        else{
            msg=malloc(sizeof(char));
            sprintf(msg, "%d", -10);
            return msg;
        }
    }
    else{
    	msg=malloc(sizeof(char));
    	sprintf(msg, "%d", -8);
    	}
    return msg;

}

int write_file(tecnicofs* fs, int file_d, fd *opened_files, char* buffer){
	int iNumber, len;

	if(opened_files[file_d].iNumber!=-1){
        if(opened_files[file_d].mode==3 || opened_files[file_d].mode==1){
            iNumber=opened_files[file_d].iNumber;
            len=strlen(buffer);
            if(inode_set(iNumber, buffer, len)==0)
                return 0;
            else
                return -11;     
        }
        else
            return -10;
    }
    return -8;
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs, int numbuckets){
	bucket* bucket_aux;
	for(int i = 0; i<numbuckets; i++){
			bucket_aux=fs->trees[i];
			print_tree(fp, bucket_aux->bstRoot);
	}
}
