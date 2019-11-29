/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

//adicionar /0 no final de mensagens do buffer
//adicionar /0 no final de texto guardado 
//por funcoes de tratamento de mensagens no fs
//mutex c_command??

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
#include <signal.h>

#include "fs.h"
#include "constants.h"
#include "lib/timer.h"
#include "sync.h"
#include "lib/hash.h"
#include "lib/inodes.h"
#include "getopt.h"

char* socketname = NULL;
char* global_outputFile = NULL;
pthread_mutex_t c_command;
pthread_t* workers;
tecnicofs* fs;
TIMER_T startTime, stopTime;

int numberBuckets = 0;
int num_commands_readed=0;
int input_processed=0;
int num_clients=0;
int sockfd;

sigset_t signal_mask;

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

char* applyCommand(char* line, int uid, fd *opened_files){
    printf("new command %s\n", line);
    int file_d, permissions, mode, rtn, len;
    char token, name[MAX_INPUT_SIZE], newname[MAX_INPUT_SIZE];
    char *buffer;
    char *msg;
    token= line[0];

    switch (token) {
        case 'c':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %d", &token, name, &permissions);
            mutex_unlock(&c_command);
            rtn=create(fs, name, permissions, uid);
            sprintf(msg, "%d", rtn);
            return msg;
            break;
        case 'd':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s", &token, name);
            
            mutex_unlock(&c_command);
            rtn=delete(fs, name, opened_files, uid);
            sprintf(msg, "%d", rtn);
            return msg;
            break;
        case 'r':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %s", &token, name, newname);
            mutex_unlock(&c_command);
            rtn=change_name(fs, name, newname, opened_files, uid);
            sprintf(msg, "%d", rtn);
            return msg;
            break;
        case 'o':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %s %d", &token, name, &mode);
            rtn=open_file(fs, name, uid, mode, opened_files);
            sprintf(msg, "%d", rtn);
            return msg;
            break;
        case 'x':
            msg=malloc(sizeof(char));
            sscanf(line, "%c %d", &token, &file_d);
            rtn=close_file(fs, file_d, uid, opened_files);
            sprintf(msg, "%d", rtn);
            return msg;
            break;
        case 'l':
            sscanf(line, "%c %d %d", &token, &file_d, &len);
            msg=read_file(fs, file_d, opened_files, len);
            return msg;
            break;
        case 'w':
            msg=malloc(sizeof(char));
            buffer=malloc(sizeof(char)*strlen(line));
            sscanf(line, "%c %d %s", &token, &file_d, buffer);
            rtn=write_file(fs, file_d, opened_files, buffer);
            sprintf(msg, "%d", rtn);
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
            strcat(buffer, "\0");
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
    int newsockfd, s;
    socklen_t clientlen, servlen;
    struct sockaddr_un serv_addr, client_addr;
    pthread_t thread_client;

    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);

    s=pthread_sigmask(SIG_UNBLOCK, &signal_mask, NULL);
    if(s!=0)
        printf("Error in assigning the main task to the signal\n");
    if(s==0)
        printf("fez signal\n");

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

void closesocket(int socket){
    close(socket);
}

void trata_sinal(int signal){
    printf("entrou\n");
    destroy_threads(stdout);
    closesocket(sockfd);
    exit(0);
} 

int main(int argc, char* argv[]) {
    parseArgs(argc, argv);


    fs = new_tecnicofs(numberBuckets);
    FILE * outputFp = openOutputFile();

    mutex_init(&c_command);
    inode_table_init();
    if(signal(SIGINT, trata_sinal)==SIG_ERR)
        printf("signal error\n");
    socket_init();

    print_tecnicofs_tree(outputFp, fs, numberBuckets);
    fflush(outputFp);
    fclose(outputFp);

    mutex_destroy(&c_command);
    inode_table_destroy();
    free_tecnicofs(fs, numberBuckets);
    exit(EXIT_SUCCESS);
}
