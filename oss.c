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
#include <errno.h>
#include "constants.h"

//for shared memory clock
static int *shared;
static int shmid;

//for pids
static pid_t *pidptr;

//for message queue
static int msqid;

void sighandler(int sigid){
	printf("Caught signal %d\n", sigid);
	//send kill message to children
	//access pids[] to kill each child
	 int i;
	for ( i = 0; i < MAX; i++){
		if (pidptr[i] != 1){
			kill(pidptr[i], SIGQUIT);
		}
	} 
	
	//cleanup shared memory
	cleanup();
	
	exit(sigid);
}

int cleanup(){
	int i;
	for ( i = 0; i < MAX; i++){
		if (pidptr[i] != 1){
			kill(pidptr[i], SIGQUIT);
		}
	}
	
	detachshared();
	removeshared();
	deletequeue();
	return 0;
}

int deletequeue(){
	//delete message queue
	struct msqid_ds *buf;
	if(msgctl(msqid, IPC_RMID, buf) == -1){
		perror("msgctl: remove queue failed.");
		return -1;
	}
}

int detachshared(){
	//detach from shared memory clock and resource descriptors
	if((shmdt(shared) == -1) || (shmdt(block) == -1)){
		perror("failed to detach from shared memory");
		return -1;
	}
	
	
}
int removeshared(){
	//remove shared memory clock and resource descriptors
	if((shmctl(shmid, IPC_RMID, NULL) == -1) || (shmctl(rdmid, IPC_RMID, NULL) == -1)){
		perror("failed to delete shared memory");
		return -1;
	}
	
}

int main(int argc, char **argv){
	
	//getopt
	extern char *optarg;
	extern int optind;
	int c, err = 0;
	int hflag=0, sflag=0, lflag=0, tflag=0, vflag=0;
	static char usage[] = "usage: %s -h \n-v \n-l filename \n-i y \n-t z\n";
	
	char *filename, *x, *z;
	
	while((c = getopt(argc, argv, "hs:l:i:t:")) != -1)
		switch (c) {
			case 'h':
				hflag = 1;
				break;
			case 's':
				sflag = 1;
				x = optarg;//max number of slave processes
				break;
			case 'l':
				lflag = 1;
				filename = optarg;//log file 
				break;
			case 'v':
				vflag = 1;//verbose on
				break;
			case 't':
				tflag = 1;
				z = optarg;//time until master terminates
				break;
			case '?':
				err = 1;
				break;
		}
		
	if(err){
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	//help
	if(hflag){
		puts("-h for help\n-l to name log file\n-s for number of slaves\n-i for number of increments per slave\n-t time for master termination\n-v verbose on\n");
	}
	//set default filename for log
	if(lflag == 0){
		filename = "test.out";
	}
	puts(filename);
	//number of slaves
	int numSlaves = 10; 
	if(sflag){//change numSlaves
		numSlaves = atoi(x);
	}
	
	//time in seconds for master to terminate
	int endTime = 2;
	if(tflag){//change endTime
		endTime = atoi(z);
	}
	
	//create message queue in shared memory
	key_t msgkey;
	message_buf sbuf, rbuf;
	size_t buf_length = 0;
	
	if((msgkey = ftok("oss.c", 2)) == -1){
		perror("msgkey error");
		return 1;
	}
	if((msqid = msgget(msgkey, IPC_CREAT | 0666)) < 0){
		perror("msgget from oss");
		return 1;
	}
	
	//create clock in shared memory
	key_t key;
	//int shmid;
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
	//delete after detach
	//shmctl(shmid, IPC_RMID, 0);

	clock = shared;
	clock[0] = 0;//initialize "clock" to zero
	clock[1] = 0;
	
	//create start time
	struct timespec start, now;
	clockid_t clockid;//clockid for timer
	clockid = CLOCK_REALTIME;
	long starttime, nowtime;
	if(clock_gettime(clockid, &start) == 0){
		starttime = start.tv_sec;
	}
	if(clock_gettime(clockid, &now) == 0){
		nowtime = now.tv_sec;
	}
	
	int totalProcesses = 0;//keep count of total processes created
	int currentnum = 0;//keep count of current processes in system
	
	//for forking children
	pid_t pids[MAX];//pid_t *pidptr points to this
	pidptr = pids;
	//initialize pids[]
	//printf("initializing pids[]\n");
	int i;
	for(i = 0; i < MAX; i++){
		pids[i] = 1;
	}
	//pid
	pid_t pid;
	
	int childsec, childns;//for time sent by child
	int status;//for wait(&status)
	int sendnext = 1;//send next process message to run
	int loglength = 0;//for log file
	
	//initialize random number generator
	srand( time(NULL) );
	
	//interval between forking children
	int timetofork = rand() % 500000000;//500 milliseconds
	int currentns, prevns, prevsec = 0;
	
	//put message type 1 (critical section token) into message queue
	sbuf.mtype = 1;
	//send message
	if(msgsnd(msqid, &sbuf, 0, IPC_NOWAIT) < 0) {
		printf("%d, %d\n", msqid, sbuf.mtype);
		perror("msgsnd");
		//cleanup();
		return 1;
	}else{
		//printf("critical section token available\n");
	}
	
	while(totalProcesses < 100 && clock[0] < 20 && (nowtime - starttime) < endTime){
		//signal handler
		signal(SIGINT, sighandler);
		
		errno = 0;
		//check for critical section token in message queue
		if(msgrcv(msqid, &rbuf, 0, 1, MSG_NOERROR | IPC_NOWAIT) < 0){
			if(errno != ENOMSG){
				perror("msgrcv in oss");
				//cleanup();
				return 1;
			}
			
		}else{
			//printf("critical section token received\n");
			clock[1] += rand() % 1000000;
			if(clock[1] > 1000000000){
				clock[0] += 1;
				clock[1] -= 1000000000;
			}
		
			//put critical section token back into message queue	
			sbuf.mtype = 1;
			//send message
			if(msgsnd(msqid, &sbuf, 0, IPC_NOWAIT) < 0) {
				printf("%d, %d, %d, %d\n", msqid, sbuf.mtype);
				perror("msgsnd critical section token");
				//cleanup();
				return 1;
			}else{
				//printf("critical section token available\n");
			}
		}		
		//fork children
		currentsec = clock[0];
		currentns = clock[1];
		//if time to fork new process && current number of processes < max number
		if(((((currentsec * 1000000000) + currentns) - ((prevsec * 1000000000) + prevns)) >= timetofork) && (currentnum < numSlaves)){
			prevns = currentns;
			prevsec = currentsec;
			//find empty pids[]
			for(i = 0; i < numSlaves; i++){
				if(pids[i] == 1){
					break;
				}
			}
			pids[i] = fork();
			if(pids[i] == -1){
				perror("Failed to fork");
				//cleanup();
				return 1;
			}
			if(pids[i] == 0){
				execl("user", "user", NULL);
				perror("Child failed to exec user");
				//cleanup();
				return 1;
			}
			totalProcesses++;//add to total processes	
			currentnum++;//add to current number of processes
		}
		
		
		
		//get current time
		if(clock_gettime(clockid, &now) == 0){
			nowtime = now.tv_sec;
		}
	}//end of while loop
	//terminate any leftover children
	for (i = 0; i < numSlaves; i++){
		if (pids[i] != 1){
			kill(pids[i], SIGQUIT);
		}
	}
	printf("%d total processes started\n", totalProcesses);
	
	cleanup();
	return 0;
}
	
		
		