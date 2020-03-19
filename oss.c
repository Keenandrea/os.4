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
void overlay(int, int);
void clockinc(simclock *, int, int);
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


/* PROCESS SCHEDULER =================================================== */
/* ===================================================================== */
void pscheduler()
{
    int status;
    int acount = 0; //activeProcs
    int ecount = 0; //exitCount
	int runnin = 0;
    int execap = 100; //remainingExecs

	/* if starting with no proces
	   ses in the system, defined
	   time in the future where a
	   process will be launched */
	simclock initlaunch = {0, 0};
	/* generate a new time upon w
	   hich a new process will wa
	   it until it is launched */ 
	simclock nextlaunch = {0, 0};

	//struct Queue* rqueue = queueinit(pcap);
	struct Queue* q1 = queueinit(pcap);
	struct Queue* q2 = queueinit(pcap);
	struct Queue* q3 = queueinit(pcap);
	struct Queue* bqueue = queueinit(pcap);

	int rservicetime = basetimequantum * 1000000;
	int q1servicetime = rservicetime * 2;
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
		}

		pid_t pid;

		if(acount < pcap && execap > 0)
		{
			/* DEBUG the following conditional */
			if((smseg->smtime.secs >= nextlaunch.secs) && (smseg->smtime.nans >= nextlaunch.nans))
			{
				nextlaunch.secs = smseg->smtime.secs;
				nextlaunch.nans = smseg->smtime.nans;
				
				int simsecs = (rand() % (maxtimebetweennewprocssecs + 1));
				int simnans = (rand() % (maxtimebetweennewprocsnans + 1));

				clockinc(&(nextlaunch), simsecs, simnans);

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
void overlay(int seat)
{
	char proc[20]; 
					
	snprintf(proc, sizeof(proc), "%i", seat);	
				
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
