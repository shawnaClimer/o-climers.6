#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include "constants.h"

//for shared memory clock
static int *shared;

void sighandler(int sigid){
	printf("Caught signal %d\n", sigid);
	
	//cleanup shared memory
	detachshared();
	
	exit(sigid);
}
int detachshared(){//detach from shared memory clock
	if((shmdt(shared) == -1)){
		perror("failed to detach from shared memory");
		return -1;
	}else{
		return 1;
	}
}

int main(int argc, char **argv){
	//attach to clock shared memory
	key_t key;
	int shmid;
	//int *shared;
	int *clock;
	void *shmaddr = NULL;
	
	if((key = ftok("oss.c", 7)) == -1){
		perror("key error");
		return 1;
	} 
	//get the shared memory
	if((shmid = shmget(key, (sizeof(int) * 2), IPC_CREAT | 0666)) == -1){
		perror("failed to create shared memory");
		return 1;
	}
	//attach to shared memory
	if((shared = (int *)shmat(shmid, shmaddr, 0)) == (void *)-1){
		perror("failed to attach");
		if(shmctl(shmid, IPC_RMID, NULL) == -1){
			perror("failed to remove memory seg");
		}
		return 1;
	}
		
	clock = shared;
	
	//attach to message queue in shared memory
	int msqid;
	key_t msgkey;
	message_buf sbuf, rbuf;
	size_t buf_length = 0;
	
	if((msgkey = ftok("oss.c", 2)) == -1){
		perror("msgkey error");
		return 1;
	}
	if((msqid = msgget(msgkey, 0666)) < 0){
		perror("msgget from user");
		return 1;
	}
	int mypid = getpid();
	
	//initialize random number generator
	srand( time(NULL) );
	
	int terminate = 0;//flip to 1 when time to terminate
	int mem_ref = 0;
	int page;//request page
	int write;//0 for read, 1 for write
	
	while(terminate < 1){
		//choose page to request
		page = (rand() % 32);
		//page = 3200 / 1000;
		write = (rand() % 2);//1 for write, 0 for read
		//request page
		sbuf.mtype = 1;//message type 1
		sbuf.mtext[0] = mypid;
		sbuf.mtext[1] = page;
		sbuf.mtext[2] = write; //0 for read, 1 for write
		buf_length = sizeof(sbuf.mtext);
		if(msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0){
			printf("%d, %d\n", msqid, sbuf.mtype);
			perror("time msgsend");
			detachshared();
			return 1;
		}
		//wait for response 
		if(msgrcv(msqid, &rbuf, 0, mypid, 0) < 0){
			//printf("message not received.\n");
			//return 1;
		}else{
			mem_ref++;
		}
		
		
		if ((mem_ref % 1000) == 0){
			if (rand() % 100 < 50){
				terminate = 1;//time to terminate
				//send message to oss
				sbuf.mtype = 2;//message type 2 
				sbuf.mtext[0] = mypid;
				sbuf.mtext[1] = clock[0];
				sbuf.mtext[2] = clock[1];
				sbuf.mtext[3] = mem_ref;
				buf_length = sizeof(sbuf.mtext);
				if(msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0){
					printf("%d, %d\n", msqid, sbuf.mtype);
					perror("time msgsend");
					detachshared();
					return 1;
				}
			} 
		}
	}
	//printf("%d memory accesses\n", mem_ref);
			
	//code for freeing shared memory
	if(detachshared() == -1){
		return 1;
	}
		
	
	return 0;
}