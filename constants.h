//max num processes
#define MAX		18
//for message queue
#define MSGSZ	4
typedef struct msgbuf {
	long mtype;
	int mtext[MSGSZ];
} message_buf;

//for page tables
typedef struct pagetable {
	int page_frame [32];//-1 if not in frame
} page_table;

//for frame
typedef struct frametable {
	int current_pid;//pid
	int current_page;//page number
	char valid;//'F' free, 'V' valid, 'U' occupied
	int dirty;//0 was a read, 1 was a write
	int timestamp[2];//time stamp
} frame;