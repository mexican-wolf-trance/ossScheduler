#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define SEC_KEY 0x1234
#define MSG_KEY 0x2345
#define PCT_KEY 0x3456
#define maxProc 18

typedef struct Clock
{
	unsigned int sec;
	unsigned int nsec;
} Clock;

struct msgbuf
{
        long mtype;
        char mtext[100];
} message;

typedef struct processControlTable
{
	unsigned int cpuTime;
	unsigned int totalTime;
	int lastBurstTime;
	pid_t childPID;
} pct;

int shmid, shmpctid, msgqid;
struct Clock *sim_clock;

pct pcTable[maxProc];
pct *pctptr;

void sigint(int sig)
{
	write(1, "Interrupt!", 12);

        shmdt(pctptr);
        shmctl(shmpctid, IPC_RMID, NULL);

        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);

	kill(0, SIGQUIT);
	exit(0);
}


int main()
{
        signal(SIGALRM, sigint);
        signal(SIGINT, sigint);

	int maxTimeBtwnNewProcNS, maxTimeBtwnNewProcSEC, maxTime = 3, counter = 0;
	char *exec[] = {"./user", NULL};
	pid_t child = 0;
	FILE *fp;

	pctptr = NULL;
	pctptr = pcTable;

        alarm(maxTime);

        shmid = shmget(SEC_KEY, sizeof(Clock), 0644 | IPC_CREAT);
        if (shmid == -1)
        {
                perror("shmid get failed");
                return 1;
        }

        sim_clock = (Clock *) shmat(shmid, NULL, 0);
        if (sim_clock == (void *) -1)
        {
                perror("clock get failed");
                return 1;
        }

        shmpctid = shmget(PCT_KEY, sizeof(pcTable), 0644 | IPC_CREAT);
        if (shmpctid == -1)
        {
                perror("shmpctid get failed");
                return 1;
        }

        pctptr = (pct *) shmat(shmpctid, NULL, 0);
        if (pctptr == (void *) -1)
        {
                perror("clock get failed");
                return 1;
        }


        msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed");
                return 1;
        }

	while(1)
	{
		if(counter == maxProc)
			break;
		counter++;
	}

	printf("Finished\n");

        shmdt(pctptr);
        shmctl(shmpctid, IPC_RMID, NULL);

        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);

        kill(0, SIGQUIT);
        return 0;
}		
