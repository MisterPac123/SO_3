/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

//adicionar /0 no final de mensagens do buffer
//adicionar /0 no final de texto guardado 
//por funcoes de tratamento de mensagens no fs

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "fs.h"
#include "constants.h"
#include "lib/timer.h"
#include "sync.h"
#include "lib/hash.h"
#include "lib/inodes.h"
#include "getopt.h"

char* socketname = NULL;
char* global_outputFile = NULL;
pthread_mutex_t p_command, c_command;
pthread_t* workers;
sem_t sem_p, sem_c;
tecnicofs* fs;
TIMER_T startTime, stopTime;

int numberBuckets = 0;
int num_commands_readed=0;
int input_processed=0;
int num_clients=0;
int sockfd;

static void displayUsage (const char* appName){
    printf("Usage: %s input_filepath output_filepath threads_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }

    socketname = argv[1];
    global_outputFile = argv[2];
    numberBuckets = 1;
}

void errorParse(int lineNumber){
    fprintf(stderr, "Error: line %d invalid\n", lineNumber);
    exit(EXIT_FAILURE);
}

FILE * openOutputFile() {
    FILE *fp;
    fp = fopen(global_outputFile, "w");
    if (fp == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    return fp;
}

char* applyCommand(char* line, int uid, fd opened_files[]){
    printf("new command %s\n", line);
    int i, fd, position, position2, iNumber, permissions, len=0, mode;
    uid_t owner;
    permission othersPerm, ownerPerm; 
    char token, name[MAX_INPUT_SIZE], newname[MAX_INPUT_SIZE];
    char *buffer;
    char *msg;
    token= line[0];

    switch (token) {
        case 'c':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %d", &token, name, &permissions);

            position=hash(name, numberBuckets);
            othersPerm=permissions % 10;
            ownerPerm=permissions / 10;
            printf("name: %s position:%d\n", name, position);
            //verifica se ficheiro existe
            if(lookup(fs->trees[position], name)!=-1){
                sprintf(msg, "%d", -4);
                return msg;
            }

            iNumber = inode_create(uid, ownerPerm, othersPerm);
            printf("inumber:%d\n", iNumber);
            mutex_unlock(&c_command);
            create(fs->trees[position], name, iNumber);
            printf("criei\n");
            sprintf(msg, "%d", 0);
            return msg;
            break;
        case 'd':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s", &token, name);
            position=hash(name, numberBuckets);
            iNumber = lookup(fs->trees[position], name);
            //verifica se ficheiro existe
            if(iNumber==-1){
                sprintf(msg, "%d", -5);
                return msg;
            }
            //verifica que nao esta aberto
            for(i=0; i<MAX_OPENED_FILES; i++)
                if(iNumber==opened_files[i].iNumber){
                    printf("fd:%d\n", i);
                    sprintf(msg, "%d", -9);
                    return msg;
                }
            
            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber, &owner, &ownerPerm, &othersPerm, buffer, len);
            //verifica se utilizador e o dono
            if(owner!=uid){
                sprintf(msg, "%d", -6);
                return msg;
            }

            inode_delete(iNumber);
            mutex_unlock(&c_command);
            delete(fs->trees[position], name);
            sprintf(msg, "%d", 0);
            return msg;
            break;
        case 'r':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %s", &token, name, newname);
            position=hash(name, numberBuckets);
            position2=hash(newname, numberBuckets);
            if(lookup(fs->trees[position2], newname)!=-1){
                sprintf(msg, "%d", -4);
                return msg;
            }
            
            if((iNumber = lookup(fs->trees[position], name))==-1){
                sprintf(msg, "%d", -5);
                return msg;
            }

            for(i=0; i<MAX_OPENED_FILES; i++)
                if(iNumber==opened_files[i].iNumber){
                    sprintf(msg, "%d", -9);
                    return msg;
                }

            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len);
            if(owner!=uid){
                sprintf(msg, "%d", -6);
                return msg;
            }
            
            inode_delete(iNumber);
            inode_create(uid, ownerPerm, othersPerm);

            mutex_unlock(&c_command);
            change_name(fs->trees[position], fs->trees[position2], name, newname);
            sprintf(msg, "%d", 0);
            return msg;
            break;
        case 'o':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %d", &token, name, &mode);
            position=hash(name, numberBuckets);
            if((iNumber = lookup(fs->trees[position], name))==-1){
                sprintf(msg, "%d", -5);
                return msg;
            }

            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len);
            if((owner==uid && (ownerPerm==3 || ownerPerm==mode)) || (owner!=uid &&(othersPerm==3 || ownerPerm==mode))){
                for(i=0; i<MAX_OPENED_FILES; i++)
                    if(opened_files[i].iNumber==-1){
                        opened_files[i].iNumber=iNumber;
                        opened_files[i].mode=mode;
                        printf("inumber %d mode %d of fd %d \n", opened_files[i].iNumber, opened_files[i].mode,i);
                        sprintf(msg, "%d", i);
                        return msg;
                    }
                sprintf(msg, "%d", -7);
                return msg;
            }
            sprintf(msg, "%d", -6);
            return msg;
            break;
        case 'x':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %d", &token, &fd);
            if(opened_files[fd].iNumber!=-1){
                opened_files[fd].iNumber=-1;
                opened_files[fd].mode=0;
                sprintf(msg, "%d", 0);
                return msg;
                }
            sprintf(msg, "%d", -8);
            return msg;
            break;
        case 'l':
            sscanf(line, "%c %d %d", &token, &fd, &len);
            if(opened_files[fd].iNumber!=-1){
                if(opened_files[fd].mode==3 || opened_files[fd].mode==2){
                    iNumber=opened_files[fd].iNumber;
                    buffer=malloc(sizeof(char)*len);
                    len = inode_get(iNumber, &owner,  &ownerPerm,  &othersPerm, buffer, len);
                    msg=malloc(sizeof(char) *(len+1));
                    sprintf(msg, "%d ", len);
                    printf("%s\n", buffer);
                    strcat(msg, buffer);
                    printf("msg:%s\n", msg);
                    return msg;
                }
                else{
                    msg=malloc(sizeof(char));
                    sprintf(msg, "%d", -10);
                    return msg;
                }
            }
            msg=malloc(sizeof(char));
            sprintf(msg, "%d", -8);
            return msg;
            break;
        case 'w':
            buffer=malloc(sizeof(char)*strlen(line));
            msg=malloc(sizeof(char));
            sscanf(line, "%c %d %s", &token, &fd, buffer);
            if(opened_files[fd].iNumber!=-1){
                if(opened_files[fd].mode==3 || opened_files[fd].mode==1){
                    iNumber=opened_files[fd].iNumber;
                    len=strlen(buffer);
                    if(inode_set(iNumber, buffer, len)==0){
                        printf("escrevi %s\n", buffer);
                        sprintf(msg, "%d", 0);
                        return msg;
                    }
                    else{
                        sprintf(msg, "%d", -11);
                        return msg;
                    }
                }
                else{
                    sprintf(msg, "%d", -10);
                    return msg;
                }
            }
            sprintf(msg, "%d", -8);
            return msg;
            break;

        default: { /* error */
            mutex_unlock(&c_command);
            fprintf(stderr, "Error: commands to apply\n");
        }
    }
    return 0;
}

void *processClient(void *socket){
    printf("new socket created\n");
    fd opened_files[MAX_OPENED_FILES];
    int sock = *(int*)socket;
    char *buffer, client_message[MAX_INPUT_SIZE];
    //inicializacao de array de fds
    for(int i=0; i<MAX_OPENED_FILES; i++){
        opened_files[i].iNumber=-1;
        opened_files[i].mode=0;
    }
    socklen_t ucred_size=sizeof(struct ucred);
    struct ucred ucred;
    getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_size);    
    //ciclo que recebe e envia comados ao cliente
    while(1){
        if(read(sock, client_message, MAX_INPUT_SIZE)>0){
            buffer=applyCommand(client_message, ucred.uid, opened_files);
            printf("buffer:%s\n", buffer);
            write(sock, buffer, strlen(buffer));
            bzero(buffer, strlen(buffer));
            bzero(client_message, strlen(client_message));
        }
        
    }
    return 0;
}

void destroy_threads(FILE* timeFp){
    for(int i = 0; i < num_clients; i++) 
        if(pthread_join(workers[i], NULL)){
            perror("Can't join thread");
            exit(EXIT_FAILURE);
        }
    TIMER_READ(stopTime);
    fprintf(timeFp, "TecnicoFS completed in %.4f seconds.\n", TIMER_DIFF_SECONDS(startTime, stopTime));
    free(workers);
}

void socket_init(){
    int newsockfd;
    socklen_t clientlen, servlen;
    struct sockaddr_un serv_addr, client_addr;
    pthread_t thread_client;

    workers = (pthread_t*) malloc(sizeof(pthread_t));

    if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) <0)
        perror("server:can't open stream socket");

    unlink(socketname);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family=AF_UNIX;
    strcpy(serv_addr.sun_path, socketname);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
   
    if (bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        perror("server, can't bind local address");
    listen(sockfd, MAX_CLIENTS);
   
    while(1){
        clientlen = sizeof(client_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &clientlen);
        if (newsockfd < 0)
            perror("server:accept erro");
        int err = pthread_create(&thread_client, NULL, processClient, (void*) &newsockfd);
        
        if (err != 0){
            perror("Can't start thread");
            exit(EXIT_FAILURE);
        }
        printf("thread created\n");
        workers[num_clients]=thread_client;
        num_clients++;
        workers = (pthread_t*) realloc(workers, num_clients* sizeof(pthread_t));
    }
}

int main(int argc, char* argv[]) {
    parseArgs(argc, argv);


    fs = new_tecnicofs(numberBuckets);
    FILE * outputFp = openOutputFile();

    mutex_init(&p_command);
    mutex_init(&c_command);
    inode_table_init();
    socket_init();
    destroy_threads(stdout);


    print_tecnicofs_tree(outputFp, fs, numberBuckets);
    fflush(outputFp);
    fclose(outputFp);

    mutex_destroy(&p_command);
    mutex_destroy(&c_command);
    inode_table_destroy();
    free_tecnicofs(fs, numberBuckets);
    exit(EXIT_SUCCESS);
}
