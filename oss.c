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
int tousr;
int tooss;
int bitvector[18];
int trackbitvectorpid = 0;
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
void optset(int, char **);
void helpme();
static int satimer();
void killctrl(int, siginfo_t *, void *);
void killtime(int, siginfo_t *, void *);
void sminit();
void msginit();
void pcbinit();
void moppingup();
void clockinit();
void pscheduler();
int findaseat();
void overlay(int);
void clockinc(simclock *, int, int);
void waitstats(simclock *, simclock *);
void ltoi(simclock *, long);
void avgtimecalc(simclock *, int);
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

	pcbinit();

	clockinit();

	pscheduler();
	
	moppingup();

return 0;
}
/* END ================================================================= */


/* PROCESS SCHEDULER =================================================== */
/* ===================================================================== */
void pscheduler()
{
    int status;
	int msglen;
    int acount = 0;
    int ecount = 0;
	int lcount = 0;
	int runnin = 0;
	int logext = 0;
	int active = 0;
    int execap = 100;

	pid_t pid;

	/* if starting with no proces
	   ses in the system, defined
	   time in the future where a
	   process will be launched */
	simclock initlaunch = {0, 0};
	/* generate a new time upon w
	   hich a new process will wa
	   it until it is launched */ 
	simclock nextlaunched = {0, 0};
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

	int q1aging = q0servicetime;
	int q2aging = q1servicetime;
	int q3aging = q3servicetime;

	srand(time(0));

	while(ecount < 100)
	{
		/* advance clock by 1.xx seconds in each iteration of loop   */
		int overhead = (1000000000 + (rand() % (maxrandinterval + 1)));
		clockinc(&(smseg->smtime), 0, overhead);

		if(runnin == 0)
		{
			int generate = (rand() % (2000000000 + 1));
			clockinc(&(nextlaunched), 0, generate);
			clockinc(&(initlaunch), 0, generate);
		}

			if((acount < pcap) && (execap > 0) && (smseg->smtime.secs >= nextlaunched.secs) && (smseg->smtime.nans >= nextlaunched.nans))
			{

				/* following block simulates the creati
				   on of user processes at random inter
				   vals based on random secs and nans*/
				nextlaunched.secs = smseg->smtime.secs;
				nextlaunched.nans = smseg->smtime.nans;
				int simsecs = (rand() % (maxtimebetweennewprocssecs + 1));
				int simnans = (rand() % (maxtimebetweennewprocsnans + 1));
				clockinc(&(nextlaunched), simsecs, simnans);

				/* allocate and initial
				   ize process control
				   block for process */
				int seat = findaseat(); 
				if(seat > -1)
				{
					pid = fork();
					if(pid < 0)
					{
						perror("\noss: fork failed");
						exit(EXIT_FAILURE);
					}
					if(pid == 0)
					{
						overlay(seat);
					}

					execap--;
					acount++;

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
							fprintf(outlog, "\n[oss]: [generating process]  -> [pid: %i] [local pid: %i] [queue: 0] [time: %is:%ins]", smseg->pctable[seat].pids, smseg->pctable[seat].fpid, smseg->smtime.secs, smseg->smtime.nans);
							lcount++;
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
							fprintf(outlog, "\n[oss]: [generating process]  -> [pid: %i] [local pid: %i] [queue: 1] [time: %is:%ins]", smseg->pctable[seat].pids, smseg->pctable[seat].fpid, smseg->smtime.secs, smseg->smtime.nans);
							lcount++;
						}
					}	
				}
			}


		/*  level-five conditional enters if any queue (q0 to q3) is holding and no process is running */
		if((isempty(q0) == 0 || isempty(q1) == 0 || isempty(q2) == 0 || isempty(q3) == 0) && runnin == 0)
		{
			int qnum;
			strcpy(msg.message, "");
			if(isempty(q0) == 0)
			{
				qnum = 0;
				active = dequeue(q0);
				msg.msgtype = smseg->pctable[active].pids;
				msgsnd(tousr, &msg, sizeof(msg), 0);
			}

			else if(isempty(q1) == 0)
			{
				qnum = 1;
				active = dequeue(q1);
				msg.msgtype = smseg->pctable[active].pids;
				msgsnd(tousr, &msg, sizeof(msg), 0);
			}

			else if(isempty(q2) == 0)
			{
				qnum = 2;
				active = dequeue(q2);
				msg.msgtype = smseg->pctable[active].pids;
				msgsnd(tousr, &msg, sizeof(msg), 0);				
			}

			else if(isempty(q3) == 0)
			{
				qnum = 3;
				active = dequeue(q3);
				msg.msgtype = smseg->pctable[active].pids;
				msgsnd(tousr, &msg, sizeof(msg), 0);
			}
			/* guard against a
			   long log file*/
			if(lcount < 10000)
			{
				fprintf(outlog, "\n[oss]: [dispatching process] -> [pid: %i] [local pid: %i] [queue: %i] [time: %is:%ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, qnum, smseg->smtime.secs, smseg->smtime.nans);
				lcount++;
			}

			int simcost = ((rand() % 9900) + 100);
			clockinc(&(smseg->smtime), 0, simcost);

			/* guard against a
			   long log file*/
			if(lcount < 10000)
			{
				fprintf(outlog, "\n[oss]: [scheduler]           -> [pid: %i] [local pid: %i] [total dispatch time: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, simcost);
				lcount++;
			}

			runnin = 1;
		}


		if(isempty(bqueue) == 0)
		{
			int entry;
			for(entry = 0; entry < getsize(bqueue); entry++)
			{
				int blockedid = dequeue(bqueue);
				if((msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[blockedid].pids, IPC_NOWAIT) > -1) && strcmp(msg.message, "FINALIZED") == 0)
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

					int simcost = ((rand() % 99900) + 1000);
					clockinc(&(smseg->smtime), 0, simcost);

					if(lcount < 10000)
					{
						fprintf(outlog, "\n[oss]: [unblocked]           -> [pid: %i] [local pid: %i] [time: %is:%ins]", smseg->pctable[blockedid].pids, smseg->pctable[blockedid].fpid, smseg->smtime.secs, smseg->smtime.nans);
						lcount++;
					}

				} else {
					enqueue(bqueue, blockedid);
				}
			}
		}


		if(runnin == 1)
		{
			runnin = 0;
			if((msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pids, 0)) > -1)
			{
				if(strcmp(msg.message, "EXPIRED") == 0)
				{
					while(waitpid(smseg->pctable[active].pids, NULL, 0) > 0);
					msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pids, 0);

					smseg->pctable[active].smsystime = smseg->smtime;
					smseg->pctable[active].smcputime.nans = smseg->smtime.nans - smseg->pctable[active].smcputime.nans;

					int percent = atoi(msg.message);
					int servicetime;

					if(smseg->pctable[active].priority == 0)
					{
						servicetime = (int)((double)q0servicetime * ((double)percent / (double)100));
					}

					else if(smseg->pctable[active].priority == 1)
					{
						servicetime = (int)((double)q1servicetime * ((double)percent / (double)100));
					}

					else if(smseg->pctable[active].priority == 2)
					{
						servicetime = (int)((double)q2servicetime * ((double)percent / (double)100));					
					}

					else if(smseg->pctable[active].priority == 3)
					{
						servicetime = (int)((double)q3servicetime * ((double)percent / (double)100));					
					}	

					acount--;
					ecount++;

					clockinc(&(smseg->smtime), 0, servicetime);
					clockinc(&(smseg->pctable[active].smcputime), 0, servicetime);

					bitvector[active] = 0;

					/* guard against a
					   long log file*/
					if(lcount < 10000)
					{
						fprintf(outlog, "\n[oss]: [terminated]          -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to terminate: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
						lcount++;
					}	

					clockinc(&cputimetotal, smseg->pctable[active].smcputime.secs, smseg->pctable[active].smcputime.nans);
					clockinc(&blktimetotal, smseg->pctable[active].smblktime.secs, smseg->pctable[active].smblktime.nans);

					int secswait = smseg->pctable[active].smsystime.secs;
					int nanswait = smseg->pctable[active].smsystime.nans;

					secswait -= smseg->pctable[active].smcputime.secs;
					nanswait -= smseg->pctable[active].smcputime.nans;

					secswait -= smseg->pctable[active].smblktime.secs;
					nanswait -= smseg->pctable[active].smblktime.nans;

					clockinc(&waittimetotal, secswait, nanswait);
				}


				if((strcmp(msg.message, "EXHAUSTED") == 0))
				{
					int priorpriority;
					if(smseg->pctable[active].priority == 0)
					{
						priorpriority = smseg->pctable[active].priority;
						smseg->pctable[active].priority = 0;
						enqueue(q0, smseg->pctable[active].fpid);

						clockinc(&(smseg->smtime), 0, q0servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved q(0) -> q(0)]  -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q0servicetime);
							lcount++;
						}

						clockinc(&(smseg->pctable[active].smcputime), 0, q0servicetime);
					}

					else if(smseg->pctable[active].priority == 1)
					{
						priorpriority = smseg->pctable[active].priority;
						smseg->pctable[active].priority = 2;
						enqueue(q2, smseg->pctable[active].fpid);

						clockinc(&(smseg->smtime), 0, q1servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved q(1) -> q(2)]  -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q1servicetime);
							lcount++;
						}

						clockinc(&(smseg->pctable[active].smcputime), 0, q1servicetime);		
					}

					else if(smseg->pctable[active].priority == 2)
					{
						priorpriority = smseg->pctable[active].priority;
						smseg->pctable[active].priority = 3;
						enqueue(q3, smseg->pctable[active].fpid);

						clockinc(&(smseg->smtime), 0, q2servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved q(2) -> q(3)]  -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q2servicetime);
							lcount++;
						}

						clockinc(&(smseg->pctable[active].smcputime), 0, q2servicetime);				
					}

					else if(smseg->pctable[active].priority == 3)
					{
						priorpriority = smseg->pctable[active].priority;
						smseg->pctable[active].priority = 3;
						enqueue(q3, smseg->pctable[active].fpid);

						clockinc(&(smseg->smtime), 0, q3servicetime);

						if(lcount < 10000)
						{
							fprintf(outlog, "\n[oss]: [moved q(3) -> q(3)]  -> [pid: %i] [local pid: %i] [time: %is:%ins] [time to move: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, q3servicetime);
							lcount++;
						}

						clockinc(&(smseg->pctable[active].smcputime), 0, q3servicetime);				
					}
				}


				if(strcmp(msg.message, "SLICED") == 0)
				{
					msgrcv(tooss, &msg, sizeof(msg), smseg->pctable[active].pids, 0);

					int percent = atoi(msg.message);
					int servicetime;

					if(smseg->pctable[active].priority == 0)
					{
						servicetime = (int)((double)q0servicetime * ((double)percent / (double)100));				
					}

					else if(smseg->pctable[active].priority == 1)
					{
						servicetime = (int)((double)q1servicetime * ((double)percent / (double)100));						
					}

					else if(smseg->pctable[active].priority == 2)
					{
						servicetime = (int)((double)q2servicetime * ((double)percent / (double)100));						
					}

					else if(smseg->pctable[active].priority == 3)
					{
						servicetime = (int)((double)q3servicetime * ((double)percent / (double)100));
					}
	
					clockinc(&(smseg->smtime), 0, servicetime);	
					clockinc(&(smseg->pctable[active].smcputime), 0, servicetime);

					/* guard against a
					   long log file*/
					if(lcount < 10000)
					{
						fprintf(outlog, "\n[oss]: [blocked]           -> [pid: %i] [local pid: %i] [time: %is:%ins] [time as blocked: %ins]", smseg->pctable[active].pids, smseg->pctable[active].fpid, smseg->smtime.secs, smseg->smtime.nans, servicetime);
						lcount++;
					}	

					enqueue(bqueue, smseg->pctable[active].fpid);
				}
			}

		}

		if(ecount >= 100 && execap <= 0)
		{
			systimetotal.secs = smseg->smtime.secs;
			systimetotal.nans = smseg->smtime.nans;
			break;
		}
	}

	pid_t wpid;
	while((wpid = wait(&status)) > 0);

	int pcount = ecount;
	avgtimecalc(&(waittimetotal), pcount);
	avgtimecalc(&(systimetotal), pcount);
	avgtimecalc(&(cputimetotal), pcount);
	avgtimecalc(&(blktimetotal), pcount);
	avgtimecalc(&(initlaunch), pcount);

	/* guard against a
	   long log file*/
	if(lcount < 10001)
	{
		fprintf(outlog, "\n\n[report averages]:\t[average wait time]: [%is : %ins]\t[average cpu utilization]: [%is : %ins]\t[average time in blocked queue]: [%is : %ins]\t[idle cpu time]: [%is : %ins]\n", waittimetotal.secs, waittimetotal.nans, systimetotal.secs, systimetotal.nans, blktimetotal.secs, blktimetotal.nans, initlaunch.secs, initlaunch.nans);
	} 
	if(lcount < 1000) 
	{
		fprintf(stderr, "\n[oss]: warning: log write limit exceeded. write has terminated\n");
	}
}
/* END ================================================================= */


/* INITIALIZE PCB FOR PROCESS ========================================== */
/* ===================================================================== */
int findaseat()
{
	int searcher;
	for(searcher = 0; searcher < pcap; searcher++)
	{
		if(bitvector[searcher] == 0)
		{
			bitvector[searcher] = 1;
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
	shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);
	fclose(outlog);
}
/* END ================================================================= */


/* OVERLAYS PROGRAM IMAGE WITH EXECV =================================== */
/* ===================================================================== */
void overlay(int seat)
{
	char proc[20]; 
					
	sprintf(proc, "%i", seat);	
				
	char* fargs[] = {"./usr", proc, NULL};
	execv(fargs[0], fargs);

	/* oss will not reach here unerred */	
	perror("\noss: error: exec failure");
	exit(EXIT_FAILURE);	
}
/* END ================================================================= */


/* CALCULATES THE AVERAGE TIMES ======================================== */
/* ===================================================================== */
void avgtimecalc(simclock * timetotal, int processcount)
{
	long avgtime = (((long)(timetotal->secs) * (long)1000000000) + (long)(timetotal->nans)) / processcount;

	simclock avgstats = {0, 0};
	ltoi(&avgstats, avgtime);

	timetotal->secs = avgstats.secs;
	timetotal->nans = avgstats.nans;
}
/* END ================================================================= */


/* SOLVES INT LIMIT ==================================================== */
/* ===================================================================== */
void ltoi(simclock * timestat, long dtemp)
{
	long longnans = timestat->nans + dtemp;
	while(longnans >= 1000000000)
	{
		longnans -= 1000000000;
		(timestat->secs)++;
	}

	timestat->nans = (int)longnans;
}
/* END ================================================================= */


/* CALCULATES WAIT TIME ================================================ */
/* ===================================================================== */
void waitstats(simclock * minuend, simclock * subtrahend)
{
	long mtemp = (minuend->secs * 1000000000) + (minuend->nans);
	long stemp = (subtrahend->secs * 1000000000) + (subtrahend->nans);
	long dtemp = abs(mtemp - stemp);

	simclock timestats;
	timestats.secs = 0;
	timestats.nans = 0;

	ltoi(&timestats, dtemp);

	minuend->secs = timestats.secs;
	minuend->nans = timestats.nans;
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


/* INITIATES PROCESS BLOCK FOR EASY TRACKING =========================== */
/* ===================================================================== */
void pcbinit()
{
	int setit;
	for(setit = 0; setit < pcap; setit++)
	{
		bitvector[setit] = 0;
	}
}
/* END ================================================================= */


/* INITIATES CLOCK ===================================================== */
/* ===================================================================== */
void clockinit()
{
	smseg->smtime.secs = 0;
	smseg->smtime.nans = 0;
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
	printf("\t-n : specify number of processes\n");
}
/* END ================================================================= */
