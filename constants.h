//max num processes
#define MAX		18
//for message queue
#define MSGSZ	3
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
	int current_page[MAX][32];//pid and page number
	int valid;
	int dirty;
} frame;