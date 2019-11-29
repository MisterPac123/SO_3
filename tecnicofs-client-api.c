#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "tecnicofs-client-api.h"

#define MAX_INPUT_SIZE 512



int sock;


int tfsMount(char * address){
	int dim_serv;
	struct sockaddr_un serv_addr;

	if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		perror("client: can't open stream socket");
	
	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, address);
	dim_serv = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
	

	if(connect(sock, (struct sockaddr *) &serv_addr, dim_serv) < 0)
		perror("client: can't connect to server");

	return 0;
}


int tfsUnmount(){
	if(close(sock) < 0)
		perror("client: can't close socket");

	return 0;
}


int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions){
	int tam_msg;
	char msg[MAX_INPUT_SIZE] = "c ";
	char delim[1] = " ";
	char permissions[3]; 
	
	strcat(msg,filename);
	strcat(msg,delim);	

	if (ownerPermissions == NONE)
		strcpy(permissions,"0");
	else if (ownerPermissions == WRITE)
		strcpy(permissions,"1");
	else if (ownerPermissions == READ)
		strcpy(permissions,"2");
	else if (ownerPermissions == RW)
		strcpy(permissions,"3");

	if (othersPermissions == NONE)
		strcat(permissions,"0");
	else if (othersPermissions == WRITE)
		strcat(permissions,"1");
	else if (othersPermissions == READ)
		strcat(permissions,"2");
	else if (othersPermissions == RW)
		strcat(permissions,"3");

	strcat(msg,permissions);
	tam_msg = strlen(msg);
			printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-4")==0)
		return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;

	return 0;
}


int tfsDelete(char *filename){
	int tam_msg;
	char msg[MAX_INPUT_SIZE] = "d ";

	strcat(msg,filename);
	
	tam_msg = strlen(msg);
		printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");
	printf("return %s\n", msg);
	if(strcmp(msg,"-5")==0)
		return TECNICOFS_ERROR_FILE_NOT_FOUND;
	if(strcmp(msg,"-6")==0)
		return TECNICOFS_ERROR_PERMISSION_DENIED;
	if(strcmp(msg,"-9")==0)
		return TECNICOFS_ERROR_FILE_IS_OPEN;

	return 0;
}


int tfsRename(char *filenameOld, char *filenameNew){
	int tam_msg;
	char msg[MAX_INPUT_SIZE] = "r ";
	char delim[1] = " ";

	strcat(msg,filenameOld);
	strcat(msg,delim);
	strcat(msg,filenameNew);
	
	tam_msg = strlen(msg);
			printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-4")==0)
		return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
	if(strcmp(msg,"-5")==0)
		return TECNICOFS_ERROR_FILE_NOT_FOUND;
	if(strcmp(msg,"-6")==0)
		return TECNICOFS_ERROR_PERMISSION_DENIED;

	return 0;
}


int tfsOpen(char *filename, permission mode){
	int tam_msg, fd;
	char msg[MAX_INPUT_SIZE] = "o ";

	strcat(msg,filename);

	if (mode == NONE)
		strcat(msg," 0");
	else if (mode == WRITE)
		strcat(msg," 1");
	else if (mode == READ)
		strcat(msg," 2");
	else if (mode == RW)
		strcat(msg," 3");

	tam_msg = strlen(msg);
	printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-5")==0)
		return TECNICOFS_ERROR_FILE_NOT_FOUND;
	else if(strcmp(msg,"-6")==0)
		return TECNICOFS_ERROR_PERMISSION_DENIED;
	else if(strcmp(msg,"-7")==0)
		return TECNICOFS_ERROR_MAXED_OPEN_FILES;

	fd=atoi(msg);
	
	return fd;
}


int tfsClose(int fd){
	int tam_msg;	
	char fd2[2];
	char msg[MAX_INPUT_SIZE] = "x ";

	sprintf(fd2,"%d",fd);
	strcat(msg, fd2);
	
	tam_msg = strlen(msg);
			printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-8")==0)
		return TECNICOFS_ERROR_FILE_NOT_OPEN;

	return 0;
}


int tfsRead(int fd, char *buffer, int len){
	int tam_msg, num_car;
	char msg[MAX_INPUT_SIZE] = "l ";
	char delim[2] = " ";
	char fd2[2], len2[2], num_car2[2];

	sprintf(fd2,"%d",fd);
	sprintf(len2,"%d",len);
	strcat(msg,fd2);
	strcat(msg,delim);
	strcat(msg,len2);

	tam_msg = strlen(msg);
			printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");
	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-8")==0)
		return TECNICOFS_ERROR_FILE_NOT_OPEN;
	if(strcmp(msg,"-10")==0)
		return TECNICOFS_ERROR_INVALID_MODE;


	sscanf(msg,"%s %s", num_car2, buffer);
	num_car = atoi(num_car2);

	return num_car;
}


int tfsWrite(int fd, char *buffer, int len){
	int tam_msg;
	char fd2[2];
	char delim[2] = " ";
	char msg[MAX_INPUT_SIZE] = "w ";

	sprintf(fd2,"%d",fd);
	strcat(msg,fd2);
	strcat(msg,delim);
	strcat(msg,buffer);


	tam_msg = strlen(msg);
			printf("msg:%s\n", msg);

	if (write(sock,msg,tam_msg) < 0)
		perror("client: write error on socket");

	bzero(msg, sizeof(msg));

	tam_msg = read(sock,msg,MAX_INPUT_SIZE+1);
	if(tam_msg<0)
		perror("client: read error on socket");

	if(strcmp(msg,"-8")==0)
		return TECNICOFS_ERROR_FILE_NOT_OPEN;
	if(strcmp(msg,"-10")==0)
		return TECNICOFS_ERROR_INVALID_MODE;

	return 0;
}



