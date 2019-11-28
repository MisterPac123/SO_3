/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#include "fs.h"
#include "lib/bst.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sync.h"



int obtainNewInumber(bucket* bst) {
	int newInumber = ++(bst->nextINumber);
	return newInumber;
}

tecnicofs* new_tecnicofs(int numbuckets){
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

void create(bucket* bckt, char *name, int inumber){
	sync_wrlock(&(bckt->bstLock));
	bckt->bstRoot = insert(bckt->bstRoot, name, inumber);
	sync_unlock(&(bckt->bstLock));
}

void delete(bucket* bckt, char *name){
	sync_wrlock(&(bckt->bstLock));
	bckt->bstRoot = remove_item(bckt->bstRoot, name);
	sync_unlock(&(bckt->bstLock));
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

void change_name(bucket* bckt1, bucket* bckt2, char *name, char *newName){ 
	int flag=1;
	while(1){
		if(sync_trylock(&(bckt1->bstLock))==0){
			if((&(bckt1->bstLock))==(&(bckt2->bstLock)))
				flag=0;
			if(sync_trylock(&(bckt2->bstLock))==0 || flag==0){
				node* searchNode = search(bckt1->bstRoot, name);
				node* searchNode2 = search(bckt2->bstRoot, newName);
				if ( searchNode && !searchNode2) {
					int inumber = searchNode->inumber;
					bckt1->bstRoot = remove_item(bckt1->bstRoot, name);
					bckt2->bstRoot = insert(bckt2->bstRoot, newName, inumber);
				}
				sync_unlock(&(bckt1->bstLock));
				if (flag==1)
					sync_unlock(&(bckt2->bstLock));
				break;
			}
			sync_unlock(&(bckt1->bstLock));
		}
	}
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs, int numbuckets){
	bucket* bucket_aux;
	for(int i = 0; i<numbuckets; i++){
			bucket_aux=fs->trees[i];
			print_tree(fp, bucket_aux->bstRoot);
	}
}
