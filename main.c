/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

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

int applyCommand(char* line, int uid, int opened_files[]){
    printf("new command %s\n", line);
    int i, fd, position, position2, iNumber, permissions, len=0, mode;
    uid_t owner;
    permission othersPerm, ownerPerm; 
    char token, name[MAX_INPUT_SIZE], newname[MAX_INPUT_SIZE];
    char *buffer;
    token= line[0];

    switch (token) {
        case 'c':
            sscanf(line, "%c %s %d", &token, name, &permissions);

            position=hash(name, numberBuckets);
            othersPerm=permissions % 10;
            ownerPerm=permissions / 10;
            printf("name: %s position:%d\n", name, position);
            int a=lookup(fs->trees[position], name);
            printf("%d\n", a);
            if(a!=-1){
                return -4;
            }

            iNumber = inode_create(uid, ownerPerm, othersPerm);
            printf("inumber:%d\n", iNumber);
            mutex_unlock(&c_command);
            semaphore_post(&sem_p);
            create(fs->trees[position], name, iNumber);
            printf("criei\n");
            break;
        case 'd':
            sscanf(line, "%c %s", &token, name);
            position=hash(name, numberBuckets);
            iNumber = lookup(fs->trees[position], name);
            if(iNumber==0)
                return -5;
            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber, &owner, &ownerPerm, &othersPerm, buffer, len);
            if(owner!=uid)
                return -6;

            inode_delete(iNumber);
            mutex_unlock(&c_command);
            semaphore_post(&sem_p);
            delete(fs->trees[position], name);
            break;
        case 'r':
            sscanf(line, "%c %s %s", &token, name, newname);
            position=hash(name, numberBuckets);
            position2=hash(newname, numberBuckets);
            if(lookup(fs->trees[position2], newname)!=0)
                return -4;
            
            if((iNumber = lookup(fs->trees[position], name))==0)
                return -5;
            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len);
            if(owner!=uid)
                return -6;
            
            inode_delete(iNumber);
            inode_create(uid, ownerPerm, othersPerm);

            mutex_unlock(&c_command);
            semaphore_post(&sem_p);
            change_name(fs->trees[position], fs->trees[position2], name, newname);
            break;
        case 'o':
            sscanf(line, "%c %s %d", &token, name, &mode);
            position=hash(name, numberBuckets);
            if((iNumber = lookup(fs->trees[position], name))==0)
                return -5;

            buffer = malloc(sizeof(char)*len);
            inode_get(iNumber , &owner,  &ownerPerm,  &othersPerm, buffer, len);
            if((owner==uid && (ownerPerm==3 || ownerPerm==mode)) || (owner!=uid &&(othersPerm==3 || ownerPerm==mode))){
                for(i=0; i<MAX_OPENED_FILES; i++)
                    if(opened_files[i]==-1){
                        opened_files[i]=iNumber;
                        return iNumber;
                    }
                return -7;
            }
            return -6;
            break;
        case 'x':
            sscanf(line, "%c %d", &token, &fd);
            for(i=0; i<MAX_OPENED_FILES; i++)
                if(opened_files[i]==fd){
                    opened_files[i]=-1;
                    return 0;
                }
            return -8;
            break;
        case 'l':
            sscanf(line, "%c %d %d", &token, &fd, &len);
            //verificações
            //return inode_get(iNumber, &owner,  &ownerPerm,  &othersPerm, buffer, len);
            break;
        case 'w':
            //verificar permiçoes -6
            sscanf(line, "%c %d %d", &token, &fd, &len);

            //inode_set(iNumber, &owner, &ownerPerm, &othersPerm, buffer, len);

        default: { /* error */
            mutex_unlock(&c_command);
            semaphore_post(&sem_p);
            fprintf(stderr, "Error: commands to apply\n");
        }
    }
    return 0;
}

void *processClient(void *socket){
    printf("new socket created\n");
    int opened_files[MAX_OPENED_FILES], rtn_apply_command;
    int sock = *(int*)socket;
    char *buffer, client_message[MAX_INPUT_SIZE];
    //inicializacao de array de fds
    for(int i=0; i<MAX_OPENED_FILES; i++)
        opened_files[i]=-1;
    //struct ucred ucred;
    //getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &ucred, &sizeof(struct ucred));    
    //ciclo que recebe e envia comados ao cliente
    while(1){
        if(read(sock, client_message, MAX_INPUT_SIZE)>0){
            rtn_apply_command=applyCommand(client_message, /*ucred.uid*/1, opened_files);
            buffer=malloc(sizeof(rtn_apply_command)+1);
            sprintf(buffer, "%d", rtn_apply_command);
            printf("buffer:%s\n", buffer);
            write(sock, buffer, strlen(buffer));
            bzero(buffer, strlen(buffer));
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
    semaphore_init(&sem_p, MAX_COMMANDS);
    semaphore_init(&sem_c, 0);
    inode_table_init();
    socket_init();
    destroy_threads(stdout);


    print_tecnicofs_tree(outputFp, fs, numberBuckets);
    fflush(outputFp);
    fclose(outputFp);

    mutex_destroy(&p_command);
    mutex_destroy(&c_command);
    semaphore_destroy(&sem_p);
    semaphore_destroy(&sem_c);
    inode_table_destroy();
    free_tecnicofs(fs, numberBuckets);
    exit(EXIT_SUCCESS);
}
