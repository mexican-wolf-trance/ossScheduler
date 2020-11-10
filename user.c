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

int main(int argc, char **argv)
{

	typedef struct Clock
	{
	        unsigned int sec;
	        unsigned int nsec;
		pid_t shmpid;
	} Clock;
	
	struct msgbuf
	{
	        long mtype;
	        int mtext;
	} message;
	
	typedef struct processControlTable
	{
	        unsigned long int cpuTime;
	        unsigned long int totalTime;
	        unsigned long int lastBurstTime;
		char priority;
	        pid_t childPID;
	} pct;
	
	int timeSet = 1, randombit, x, size, shmid, shmpctid, msgqid;
	long timeUsed = 0;
	struct Clock *sim_clock;
	time_t t;


	pct *pcTable;
	size = sizeof(pct) * maxProc;

        shmid = shmget(SEC_KEY, sizeof(Clock), 0644 | IPC_CREAT);
        if (shmid == -1)
        {
                perror("shmid get failed\n");
                return 1;
        }

        sim_clock = (Clock *) shmat(shmid, NULL, 0);
        if (sim_clock == (void *) -1)
        {
                perror("clock get failed\n");
                return 1;
        }

        shmpctid = shmget(PCT_KEY, size, 0644 | IPC_CREAT);
        if (shmpctid == -1)
        {
                perror("shmpctid get failed\n");
                return 1;
        }

        pcTable = (pct *) shmat(shmpctid, NULL, 0);
        if (pcTable == (void *) -1)
        {
                perror("pcTable get failed\n");
                return 1;
        }

        msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed\n");
                return 1;
        }
	
	x = atoi(argv[1])-1;	
	srand((int)time(&t) % getpid());
	randombit = rand() % 2;
	pcTable[x].childPID = getpid();
	printf("Child %ld has the index %d\n", (long) pcTable[x].childPID, x);
	printf("Now waiting for death...\n");	
	while(1)
	{
	        if(msgrcv(msgqid, &message, sizeof(message), x, IPC_NOWAIT) > 0)
		{
			printf("The incoming child message: %d and type: %ld\n", message.mtext, message.mtype);
			printf("Message received...%ld\n", (long) getpid());
			if(timeSet)
			{
				if(randombit)
	        			pcTable[x].lastBurstTime = message.mtext;
				else
					pcTable[x].lastBurstTime = (rand() % message.mtext);
				timeSet = 0;
			}
			printf("Time to live: %lu, time used: %ld\n", pcTable[x].lastBurstTime, timeUsed);
    			pcTable[x].totalTime += pcTable[x].lastBurstTime;
			if(timeUsed >= pcTable[x].lastBurstTime)
			{				
				printf("You found me! Time to die %ld\n", (long) pcTable[x].childPID);
				sim_clock->shmpid = pcTable[x].childPID;
				message.mtype = 18;
				message.mtext = x;
				printf("The outgoing child death message: %d and type: %ld\n", message.mtext, message.mtype);
				msgsnd(msgqid, &message, sizeof(message), 0);
				
				return 0;
			}
                        message.mtype = 18;
                        message.mtext = x;
			printf("The outgoing child lives message: %d and type: %ld\n", message.mtext, message.mtype);
                        msgsnd(msgqid, &message, sizeof(message), 0);
		}
		sleep(1);
		timeUsed += 500000;
		printf("Child %ld is still alive!\n", (long) getpid());
	}
	return 0;
}
