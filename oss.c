/* PREPROCESSOR DIRECTIVES ============================================= */
/* ===================================================================== */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/msg.h>

#include "queue.h"
#include "shmem.h"
/* END ================================================================= */


/* CONSTANTS =========================================================== */
/* ===================================================================== */
const int maxtimebetweennewprocsnans = 500000;
const int maxtimebetweennewprocssecs = 1;
const int basetimequantum = 10;
const int maxrandinterval = 1000;
const int weightedrealtime = 20;
/* ===================================================================== */


/* GLOBAL VARIABLES ==================================================== */
/* ===================================================================== */
struct sigaction satime;
struct sigaction sactrl;
shmem* smseg;
int sipcid = 0;
int pcap = 18;
FILE* outlog;
int tousr = 0;
int tooss = 0;
/* END ================================================================= */


/* GLOBAL MESSAGE QUEUE ================================================ */
/* ===================================================================== */
typedef struct 
{
	long msgtype;
	char message[100];
} msg;
/* END ================================================================= */


/* FUNCTION PROTOTYPES ================================================= */
/* ===================================================================== */
void optset(int, char **);
void helpme();
static int satimer();
void killctrl(int, siginfo_t *, void *);
void killtime(int, siginfo_t *, void *);
void sminit();
void msginit();
void clockinit();
void pscheduler();
void timeinc(simclock *, int);
int findaseat();
int bitvector(int);
void overlay(int);
void clockinc(simclock *, int, int);
int uspsdispatch(int, msg *);
/* END ================================================================= */


/* MAIN ================================================================ */
/* ===================================================================== */
int main(int argc, char *argv[])
{
    optset(argc, argv);

	satime.sa_sigaction = killtime;
	sigemptyset(&satime.sa_mask);
	satime.sa_flags = 0;
	sigaction(SIGALRM, &satime, NULL);

    sactrl.sa_sigaction = killctrl;
	sigemptyset(&sactrl.sa_mask);
	sactrl.sa_flags = 0;
	sigaction(SIGINT, &sactrl, NULL);

	outlog = fopen("log.txt", "w");
	if(outlog == NULL)
	{
		perror("\noss: error: failed to open output file");
		exit(EXIT_FAILURE);
	}

	sminit();

	msginit();

	clockinit();

	pscheduler();
	
	moppingup();

return 0;
}
/* END ================================================================= */

						/* guard against a
					   	   long log file*/
						// if(lcount < 10000)
						// {
						// 	fprintf(outlog, "\n[oss]: [blocked] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
						// 	lcount++;
						// }
					    // else if(lcount > 10000 && logext == 0) 
						// {
						// 	fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
						// 	logext = 1;
						// }
						// else if(logext == 1 && logwrn == 0)
						// {
						// 	fprintf(stderr, "\n[oss]: warning: log write terminated. closing file stream\n");
						// 	logwrn = 1;
						// }
						// else if(logext == 1 && logwrn == 1)
						// {
						// 	if(fclosed == 0)
						// 	{
						// 		fclose(logout);
						// 		fclosed = 1;
						// 	}							
						// }


/* PROCESS SCHEDULER =================================================== */
/* ===================================================================== */
void pscheduler()
{
    int status;
	int msglen;
    int acount = 0; //activeProcs
    int ecount = 0; //exitCount
	int lcount = 0;
	int runnin = 0;
	int logext = 0;
	int active = 0; //activeProcIndex
    int execap = 100; //remainingExecs

	/* if starting with no proces
	   ses in the system, defined
	   time in the future where a
	   process will be launched */
	simclock initlaunch = {0, 0}; //idleTime
	/* generate a new time upon w
	   hich a new process will wa
	   it until it is launched */ 
	simclock nextlaunched = {0, 0}; //nextexec
	simclock cputimetotal = {0, 0};
	simclock systimetotal = {0, 0};
	simclock blktimetotal = {0, 0};
	simclock waittimetotal = {0, 0};


	struct Queue* q0 = queueinit(pcap);
	struct Queue* q1 = queueinit(pcap);
	struct Queue* q2 = queueinit(pcap);
	struct Queue* q3 = queueinit(pcap);
	struct Queue* bqueue = queueinit(pcap);

	int q0servicetime = basetimequantum * 1000000;
	int q1servicetime = q0servicetime * 2;
	int q2servicetime = q1servicetime * 2;
	int q3servicetime = q2servicetime * 2;

	srand(time(0));

	while(1)
	{
		/* advance clock by 1.xx seconds in each iteration of loop   */
		int overhead = (1000000000 + (rand() % (maxrandinterval + 1)));
		timeinc(&(smseg->simtime), overhead);

		/* generate random number between 0 & 2 */
		int generate = (rand() % (2000000000 + 1));

		if(runnin == 0)
		{
			timeinc(&(initlaunch), generate);
			timeinc(&(smseg->simtime), generate);
		}

		pid_t pid;

		if(acount < pcap && execap > 0)
		{
			/* DEBUG the following conditional */
			if((smseg->smtime.secs >= nextlaunched.secs) && (smseg->smtime.nans >= nextlaunched.nans))
			{
				nextlaunched.secs = smseg->smtime.secs;
				nextlaunched.nans = smseg->smtime.nans;
				
				int simsecs = (rand() % (maxtimebetweennewprocssecs + 1));
				int simnans = (rand() % (maxtimebetweennewprocsnans + 1));

				clockinc(&(nextlaunched), simsecs, simnans);

				pid = fork();
				if(pid < 0)
				{
					perror("\noss: fork failed");
					exit(EXIT_FAILURE);
				}
				if(pid == 0)
				{
					overlay(pid);
				}

				execap--;
				acount++;

				int seat = findaseat(); 
				if(seat > -1)
				{
					smseg->pctable[seat].pids = pid;
					smseg->pctable[seat].fpid = seat;
					/* constant for percentage of time a process is launched as real-time or 
					   user process. heavily weighted to generating mainly user processes */
					smseg->pctable[seat].pclass = ((rand() % 100) < weightedrealtime) ? 1 : 0;

					smseg->pctable[seat].smcputime.secs = 0;
					smseg->pctable[seat].smcputime.nans = 0;
					smseg->pctable[seat].smblktime.secs = 0;
					smseg->pctable[seat].smblktime.nans = 0;

					smseg->pctable[seat].smsystime.secs = smseg->smtime.secs;
					smseg->pctable[seat].smsystime.nans = smseg->smtime.nans;

					/* follow if process is real-time */
					if(smseg->pctable[seat].pclass == 1)
					{
						smseg->pctable[seat].priority = 0;
						enqueue(q0, smseg->pctable[seat].fpid);
						/* guard against a
						   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [generating process] -> [pid: %i] [local pid: %i] [queue: 0] [time: %is:%ins]", smseg->pctable[seat].pid, smseg->pctable[seat].fpid, smseg->smtime.secs, smseg->smtime.nans);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}
					}
					/* follow if process is user-time */
					if(smseg->pctable[seat].pclass == 0)
					{
						smseg->pctable[seat].priority = 1;
						enqueue(q1, smseg->pctable[seat].fpid);
						/* guard against a
						   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [generating process] -> [pid: %i] [local pid: %i] [queue: 1] [time: %is:%ins]", smseg->pctable[seat].pid, smseg->pctable[seat].fpid, smseg->smtime.secs, smseg->smtime.nans);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}
					}
					
				} else {
					kill(pid, SIGTERM);
				}
			}
		}


		if(runnin == 1)
		{
			if((msglen = msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pid, 0)) > -1)
			{
				if(strcmp(msg.message, "EXPIRED") == 0)
				{
					msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pid, 0);

					int percent = atoi(msg.message);
					int servicetime;

					if(smseg->pctable[active].priority == 0)
					{
						servicetime = (int)((double)q0servicetime * ((double)percent / (double)100));
						/* guard against a
					       long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [terminated] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to terminate: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}
						
						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);
					}

					else if(smseg->pctable[active].priority == 1)
					{
						servicetime = (int)((double)q1servicetime * ((double)percent / (double)100));

						/* guard against a
					       long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [terminated] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to terminate: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}
						
						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);
					}

					else if(smseg->pctable[active].priority == 2)
					{
						servicetime = (int)((double)q2servicetime * ((double)percent / (double)100));

						/* guard against a
					       long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [terminated] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to terminate: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
						f	printf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}
						
						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);						
					}

					else if(smseg->pctable[active].priority == 3)
					{
						servicetime = (int)((double)q3servicetime * ((double)percent / (double)100));
						
						/* guard against a
					   	   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [terminated] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to terminate: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);						
					}		

				runnin = 0;
				}

				if(strcmp(msg.message, "EXHAUSTED") == 0)
				{
					if(smseg->pctable[active].priority == 0)
					{
						enqueue(q0, smseg->pctable[bitvector(msg.msgtype)].fpid);
						smseg->pctable[bitvector(msg.msgtype)].priority = 0;

						timeinc(&(smseg->smtime), q0servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved between q(0) -> q(0)] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q0servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), q0servicetime);
					}

					else if(smseg->pctable[active].priority == 1)
					{
						enqueue(q1, smseg->pctable[bitvector(msg.msgtype)].fpid);
						smseg->pctable[bitvector(msg.msgtype)].priority = 2;

						timeinc(&(smseg->smtime), q1servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved between q(1) -> q(2)] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q1servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), q1servicetime);		
					}

					else if(smseg->pctable[active].priority == 2)
					{
						enqueue(q2, smseg->pctable[bitvector(msg.msgtype)].fpid);
						smseg->pctable[bitvector(msg.msgtype)].priority = 3;

						timeinc(&(smseg->smtime), q2servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved between q(2) -> q(3)] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q2servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), q2servicetime);				
					}

					else if(smseg->pctable[active].priority == 3)
					{
						enqueue(q3, smseg->pctable[bitvector(msg.msgtype)].fpid);
						smseg->pctable[bitvector(msg.msgtype)].priority = 3;

						timeinc(&(smseg->smtime), q3servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved between q(3) -> q(3)] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q3servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), q3servicetime);				
					}
				
				runnin = 0;
				}

				if(strcmp(msg.message, "SLICED") == 0)
				{
					msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pid, 0);

					int percent = atoi(msg.message);
					int servicetime;

					if(smseg->pctable[active].priority == 0)
					{
						servicetime = (int)((double)q0servicetime * ((double)percent / (double)100));

						/* guard against a
					   	   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [blocked] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);						
					}

					else if(smseg->pctable[active].priority == 1)
					{
						servicetime = (int)((double)q1servicetime * ((double)percent / (double)100));
						
						/* guard against a
					   	   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [blocked] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);							
					}

					else if(smseg->pctable[active].priority == 2)
					{
						servicetime = (int)((double)q2servicetime * ((double)percent / (double)100));

						/* guard against a
					   	   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [blocked] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);							
					}

					else if(smseg->pctable[active].priority == 3)
					{
						servicetime = (int)((double)q3servicetime * ((double)percent / (double)100));
						
						/* guard against a
					   	   long log file*/
						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [blocked] -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
							lcount++;
						} else {
							fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
							logext = 1;
						}

						timeinc(&(smseg->pctable[active].smcputime), servicetime);
						timeinc(&(smseg->smtime), servicetime);							
					}

					runnin = 0;
					enqueue(bqueue, smseg->pctable[bitvector(msg.msgtype)].fpid);
				}
			}

		}


		if(isempty(bqueue) == 0)
		{
			// if(runnin == 0)
			// {
			// 	timeinc(&(initlaunch), generate);
			// 	timeinc(&(smseg->simtime), generate);
			// }

			int entry;
			for(entry = 0; entry < getsize(bqueue); entry++)
			{
				int blockedid = dequeue(bqueue);
				if((msglen = msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[blockedid].pid, IPC_NOWAIT) > -1) && strcmp(msg.message, "FINALIZED") == 0)
				{
					if(smseg->pctable[blockedid].pclass == 1)
					{
						enqueue(q0, smseg->pctable[blockedid].fpid);
						smseg->pctable[blockedid].priority = 0;
					} 
					
					else if(smseg->pctable[blockedid].pclass == 0) 
					{
						enqueue(q1, smseg->pctable[blockedid].fpid);
						smseg->pctable[blockedid].priority = 1;
					}

					/* guard against a
					   long log file*/
					if(lcount < 10000)
					{
						fprintf(outlog, "\n[oss]: [unblocked] -> [pid: %i] [local pid: %i] [time: %is:%ins]", smseg->pctable[blockedid].pid, smseg->pctable[blockedid].fpid, smseg->smtime.secs, smseg->smtime.nans);
						lcount++;
					} else {
						fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
						logext = 1;
					}

					int schsyscost = ((rand() % 9900) + 100);
					timeinc(&(smseg->simtime), schsyscost);

					/* guard against a
			   		   long log file*/
					if(lcount < 10000)
					{
						fprintf(outlog, "\n[oss]: [scheduler] -> [pid: %i] [local pid: %i] [total unblock time: %ins]", smseg->pctable[blockedid].pid, smseg->pctable[blockedid].fpid, schsyscost);
						lcount++;
					} else {
						fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
						logext = 1;
					}

				} else {
					enqueue(bqueue, smseg->pctable[blockedid].fpid);
				}
			}
		}


		/*  level-five conditional enters if any queue (q0 to q3) is holding and no process is running */
		if(runnin == 0 && (isempty(q0) == 0 || isempty(q1) == 0 || isempty(q2) == 0 || isempty(q3) == 0))
		{
			int qnum;
			/* perhaps in q0? */
			if(isempty(q0) == 0)
			{
				active = dequeue(q0);
				msg.msgtype = smseg->pctable[active].pid;
				strcpy(msg.message, "");
				if((uspsdispatch(tousr, &msg, sizeof(msg))) == -1)
				{
					perror("\noss: error: [queue 0] failed to dispatch");
					exit(EXIT_FAILURE);
				}
				qnum = 0;
			}
			/* maybe we're in q1?  */
			else if(isempty(q1) == 0)
			{
				active = dequeue(q1);
				msg.msgtype = smseg->pctable[active].pid;
				strcpy(msg.message, "");
				if((uspsdispatch(tousr, &msg, sizeof(msg))) == -1)
				{
					perror("\noss: error: [queue 1] failed to dispatch");
					exit(EXIT_FAILURE);
				}
				qnum = 1;
			}
			/* maybe we're in q2?  */
			else if(isempty(q2) == 0)
			{
				active = dequeue(q2);
				msg.msgtype = smseg->pctable[active].pid;
				strcpy(msg.message, "");
				if((uspsdispatch(tousr, &msg, sizeof(msg))) == -1)
				{
					perror("\noss: error: [queue 2] failed to dispatch");
					exit(EXIT_FAILURE);
				}
				qnum = 2;				
			}
			/* maybe we're in q3?  */
			else if(isempty(q3) == 0)
			{
				active = dequeue(q3);
				msg.msgtype = smseg->pctable[active].pid;
				strcpy(msg.message, "");
				if((uspsdispatch(tousr, &msg, sizeof(msg))) == -1)
				{
					perror("\noss: error: [queue 3] failed to dispatch");
					exit(EXIT_FAILURE);
				}
				qnum = 3;				
			}
			/* guard against a
			   long log file*/
			if(lcount < 10000)
			{
				fprintf(outlog, "\n[oss]: [dispatching process] -> [pid: %i] [local pid: %i] [queue: 1] [time: %is:%ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans);
				lcount++;
			} else {
				fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
				logext = 1;
			}

			int schsyscost = ((rand() % 9900) + 100);
			timeinc(&(smseg->simtime), schsyscost);

			/* guard against a
			   long log file*/
			if(lcount < 10000)
			{
				fprintf(outlog, "\n[oss]: [scheduler] -> [pid: %i] [local pid: %i] [total dispatch time: %ins]", smseg->pctable[active].pid, smseg->pctable[active].fpid, schsyscost);
				lcount++;
			} else {
				fprintf(stderr, "\n[oss]: warning: log write limit exceeded. terminating write\n");
				logext = 1;
			}

			runnin = 1;
		}

		if((pid = waitpid((pid_t)-1, &status, 0)) > 0)
		{
			if(WIFEXITED(status))
			{
				if(WEXITSTATUS(status) == 42)
				{
					ecount++;
					acount--;

					int pos = bitvector(pid);
					
				}
			}
		}





		if(ecount >= 100 && execap <= 0)
		{
			break;
		}
	}


}
/* END ================================================================= */


/* SEND MESSAGE ======================================================== */
/* ===================================================================== */
int uspsdispatch(int id, msg* buf, int size)
{
	int result;
	if((result = msgsnd(id, buf, size, 0)) == -1)
	{
		return(-1);
	}
	return(result);
}
/* END ================================================================= */


/* INITIALIZE PCB FOR PROCESS ========================================== */
/* ===================================================================== */
int findaseat()
{
	int searcher;
	for(searcher = 0; searcher < pcap; searcher++)
	{
		if(smseg->pctable[searcher].pids == -1)
		{
			return searcher; 
		}
	}
	return -1;
}
/* END ================================================================= */


/* INITIALIZE PCB FOR PROCESS ========================================== */
/* ===================================================================== */
int bitvector(int pid)
{
	int searcher;
	for(searcher = 0; searcher < pcap; searcher++)
	{
		if(smseg->pctable[searcher].pids == pid)
		{
			return searcher; 
		}
	}
	return -1;
}
/* END ================================================================= */


/* BITMADE JANITOR ===================================================== */
/* ===================================================================== */
void moppingup()
{
	fclose(outlog);
	shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);
}
/* END ================================================================= */


/* OVERLAYS PROGRAM IMAGE WITH EXECV =================================== */
/* ===================================================================== */
void overlay(int cpid)
{
	char proc[20]; 
					
	snprintf(proc, sizeof(proc), "%i", cpid);	
				
	char* fargs[] = {"./usr", proc, NULL};
	execv(fargs[0], fargs);

	/* oss will not reach here unerred */	
	perror("\noss: error: exec failure");
	exit(EXIT_FAILURE);	
}
/* END ================================================================= */


/* ADDS TIME BASED ON SECONDS AND NANOSECONDS ========================== */
/* ===================================================================== */
void clockinc(simclock* khronos, int sec, int nan)
{
	khronos->secs = khronos->secs + sec;
	timeinc(khronos, nan);
}
/* END ================================================================= */


/* ADDS TIME BASED ON DURATION ========================================= */
/* ===================================================================== */
void timeinc(simclock* khronos, int duration)
{
	int temp = khronos->nans + duration;
	while(temp >= 1000000000)
	{
		temp -= 1000000000;
		(khronos->secs)++;
	}
	khronos->nans = temp;
}
/* END ================================================================= */


/* INITIATES CLOCK ===================================================== */
/* ===================================================================== */
void clockinit()
{
	smseg->simtime.secs = 0;
	smseg->simtime.nans = 0;
}
/* END ================================================================= */


/* INITIATES MESSAGES ================================================== */
/* ===================================================================== */
void msginit()
{
	key_t msgkey = ftok("msg1", 825);
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

	msgkey = ftok("msg2", 725);
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


/* HANDLES SIGNALS ===================================================== */
/* ===================================================================== */
void killtime(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgtime[] = "\noss: exit: computation exceeded 3 seconds\n";
	int msglentime = sizeof(msgtime);

	write(STDERR_FILENO, msgtime, msglentime);

	int i;
	for(i = 0; i < pcap; i++)
	{
		if(smseg->pctable[i].pids != 0)
		{
			if(kill(smseg->pctable[i].pids, SIGTERM) == -1)
			{
				perror("\noss: error: ");			
			}
		}
	}

	fclose(outlog);
	shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	exit(EXIT_SUCCESS);			
}
/* END ================================================================= */


/* HANDLES CTRL-C SIGNAL =============================================== */
/* ===================================================================== */
void killctrl(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgctrl[] = "\noss: exit: received ctrl-c interrupt signal\n";
	int msglenctrl = sizeof(msgctrl);

	write(STDERR_FILENO, msgctrl, msglenctrl);

	int i;
	for(i = 0; i < pcap; i++)
	{
		if(smseg->pctable[i].pids != 0)
		{
			if(kill(smseg->pctable[i].pids, SIGTERM) == -1)
			{
				perror("\noss: error: ");
			}
		}
	}

	fclose(outlog);
	shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	exit(EXIT_SUCCESS);			
}
/* END ================================================================= */


/* SETS TIMER ========================================================== */
/* ===================================================================== */
static int satimer()
{
	struct itimerval t;
	t.it_value.tv_sec = 3;
	t.it_value.tv_usec = 0;
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	
	return(setitimer(ITIMER_REAL, &t, NULL));
}
/* END ================================================================= */


/* SETS OPTIONS ======================================================== */
/* ===================================================================== */
void optset(int argc, char *argv[])
{
	int choice;
	while((choice = getopt(argc, argv, "hn:")) != -1)
	{
		switch(choice)
		{
			case 'h':
				helpme();
				exit(EXIT_SUCCESS);
			case 'n':
				pcap = atoi(optarg);
				if(pcap > 18)
				{
					pcap = 18;
				}
				if(pcap <= 0)
				{
					pcap = 18;
				}
				break;
			case '?':
				fprintf(stderr, "\noss: error: invalid argument\n");
				exit(EXIT_FAILURE);				
		}
	}
}
/* END ================================================================= */


/* SETS HELP ========================================================== */
/* ===================================================================== */
void helpme()
{
	printf("\n|HELP|MENU|\n\n");
    printf("\t-h : display help menu\n");
	printf("\t-n : specify number of processes\n")
}
/* END ================================================================= */
