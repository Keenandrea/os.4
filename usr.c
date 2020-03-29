/* PREPROCESSOR DIRECTIVES ============================================= */
/* ===================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "shmem.h"
#include "queue.h"
/* END ================================================================= */


/* CONSTANTS =========================================================== */
/* ===================================================================== */
const int probthatprocterminates = 15;
const int probthatprocenervates  = 85;
/* END ================================================================= */


/* GLOBAL VARIABLES ==================================================== */
/* ===================================================================== */
shmem* smseg;
int sipcid;
int tousr;
int tooss;
int entire = 1;
int pcaps = 18;
/* END ================================================================= */


/* GLOBAL MESSAGE QUEUE ================================================ */
/* ===================================================================== */
struct 
{
	long msgtype;
	char message[100];
} msg;
/* END ================================================================= */


/* FUNCTION PROTOTYPES ================================================= */
/* ===================================================================== */
void sminit();
void msginit();
void clockinc(simclock *, int, int);
/* END ================================================================= */


/* MAIN ================================================================ */
/* ===================================================================== */

int main(int argc, char *argv[])
{

    sminit();

    msginit();

	simclock blockout; 

    int pid = getpid();

	int localpid = atoi(argv[0]);

    /* time(NULL) ensures a different val
       ue every second. (getpid() << 16))
       increases the odds that a differen
       t value computes for each process
       because process IDs typically are
       not re-used that often          */
    srand(time(NULL) ^ (getpid() << 16));

	while(1)
	{
		msgrcv(tousr, &msg, sizeof(msg), pid, 0);

		int termproc = (rand() % 100);
		int liveproc = (rand() % 100);	
		int decision = rand() % 2;

		/* if process decides to terminate */
		if(termproc <= probthatprocterminates)
		{
			msg.msgtype = pid;
			strcpy(msg.message, "EXPIRED");
			msgsnd(tooss, &msg, sizeof(msg), 0);

			char percent[20];
			int randtimesliceamount = rand() % 100;
			sprintf(percent, "%i", randtimesliceamount);

			strcpy(msg.message, percent);
			msgsnd(tooss, &msg, sizeof(msg), 0);

			return 0;
		}

		/* process decides not
		   to terminate and us
		   e entire timeslice*/
		if(decision == entire)
		{
				msg.msgtype = pid;
				strcpy(msg.message, "EXHAUSTED");	
				msgsnd(tooss, &msg, sizeof(msg), 0);

		} else {
				blockout.secs = smseg->smtime.secs;
				blockout.nans = smseg->smtime.nans;

				int r = (rand() % 3) + 1;
				int s = (rand() % 1000) + 1;
				int p = (rand() % 99) + 1;

				clockinc(&(blockout), r, s);
				clockinc(&(smseg->pctable[localpid].smblktime), r, s);

				msg.msgtype = pid;
				strcpy(msg.message, "SLICED");
				msgsnd(tooss, &msg, sizeof(msg), 0);

				char pconv[20];
				sprintf(pconv, "%i", p);

				strcpy(msg.message, pconv);
				msgsnd(tooss, &msg, sizeof(msg), 0);

				while(1)
				{
					if((smseg->smtime.secs > blockout.secs) || (smseg->smtime.secs == blockout.secs && smseg->smtime.nans >= blockout.nans))
					{
						break;
					}
				}

				msg.msgtype = pid;
				strcpy(msg.message, "FINALIZED");
				msgsnd(tooss, &msg, sizeof(msg), IPC_NOWAIT);
		} 
	}
	shmdt(smseg);
    return 0;
}
/* END ================================================================= */


/* ADDS TIME BASED ON SECONDS AND NANOSECONDS ========================== */
/* ===================================================================== */
void clockinc(simclock* khronos, int sec, int nan)
{
	khronos->secs = khronos->secs + sec;
	khronos->nans = khronos->nans + nan;
	while(khronos->nans >= 1000000000)
	{
		khronos->nans -= 1000000000;
		(khronos->secs)++;
	}
}
/* END ================================================================= */


/* INITIATES MESSAGES ================================================== */
/* ===================================================================== */
void msginit()
{
	key_t msgkey = ftok("msg1", 925);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	tousr = msgget(msgkey, 0600 | IPC_CREAT);
	if(tousr == -1)
	{
		perror("\noss: error: failed to create");
		exit(EXIT_FAILURE);
	}

	msgkey = ftok("msg2", 825);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	tooss = msgget(msgkey, 0600 | IPC_CREAT);
	if(tooss == -1)
	{
		perror("\noss: error: failed to create");
		exit(EXIT_FAILURE);
	}
}
/* END ================================================================= */


/* INITIATES SHARED MEMORY ============================================= */
/* ===================================================================== */
void sminit()
{
	key_t smkey = ftok("shmfile", 'a');
	if(smkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	sipcid = shmget(smkey, sizeof(shmem), 0600 | IPC_CREAT);
	if(sipcid == -1)
	{
		perror("\noss: error: failed to create shared memory");
		exit(EXIT_FAILURE);
	}

	smseg = (shmem*)shmat(sipcid,(void*)0, 0);

	if(smseg == (void*)-1)
	{
		perror("\noss: error: failed to attach shared memory");
		exit(EXIT_FAILURE);
	}
}
/* END ================================================================= */
