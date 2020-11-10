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
#define CHD_KEY 0x9876
#define maxProc 18

typedef struct Clock
{
	int sec;
	long long nsec;
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

int size, shmid, shmpctid, msgqid;
struct Clock *sim_clock;

pct *pcTable;
size = sizeof(pct) * maxProc;

void sigint(int sig)
{
	write(1, "\nInterrupt!\n", 12);

        shmdt(pcTable);
        shmctl(shmpctid, IPC_RMID, NULL);

        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);

        if (msgctl(msgqid, IPC_RMID, NULL) == -1)
        {
                fprintf(stderr, "Message queue could not be deleted\n");
                exit(EXIT_FAILURE);
        }


	kill(0, SIGQUIT);
	exit(0);
}

int main()
{
        signal(SIGALRM, sigint);
        signal(SIGINT, sigint);

	int isScheduled, maxTime = 3, i, j = maxProc-1, k;
	int A[maxProc], B[maxProc], C[maxProc], bitarray[maxProc];
	unsigned long maxTimeBtwnNewProcNS, maxTimeBtwnNewProcSEC, maxTimeBtwnProc;
	char *exec[] = {"./user", NULL, NULL, NULL}, count[3];
	pid_t child = 0;
	FILE *fp;
	time_t t;

	srand((int)time(&t) % getpid());

        alarm(20);

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
                perror("clock get failed\n");
                return 1;
        }

        msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed\n");
                return 1;
        }

	for(i = 0; i < 18; i++)
		bitarray[i] = 0;
        for(i = 0; i < 18; i++)
                A[i] = 0;
        for(i = 0; i < 18; i++)
                B[i] = 0;
        for(i = 0; i < 18; i++)
                C[i] = 0;

	sim_clock->shmpid = 0;
	message.mtype = 18;
	message.mtext = 1;
	msgsnd(msgqid, &message, sizeof(message), 0);
	maxTimeBtwnProc = 0;
	printf("Max Time Between New Process: %li\n", maxTimeBtwnProc);

	while(1)
	{
		if(msgrcv(msgqid, &message, sizeof(message), 18, IPC_NOWAIT) > 0)
		{
                        if(sim_clock->shmpid)
                        {
                                printf("Child death zone\nMtext: %d\n", message.mtext);
                                for(i = 0; i < maxProc; i++)
                                {
                                        if(pcTable[i].childPID == sim_clock->shmpid)
                                        {
                                                printf("Child %ld died\n", (long) pcTable[i].childPID);
                                                printf("Total time: %lu\n", pcTable[i].totalTime);
                                                printf("Last burst: %lu\n", pcTable[i].lastBurstTime);
                                                break;
                                        }
                                }
                                sim_clock->shmpid = 0;
                                bitarray[message.mtext] = 0;
                                sim_clock->nsec += 500000000;
                        }

                        if(sim_clock->nsec >= 1000000000)
                        {
                                sim_clock->sec++;
                                sim_clock->nsec = 0;
                        }
        	        printf("\nCurrent time: %llu sec, %llu nsec\n", sim_clock->sec, sim_clock->nsec);
	                printf("Or %llu nsecs\n", (sim_clock->sec*1000000000 + sim_clock->nsec));
			printf("Max Time Between New Process: %li\n", maxTimeBtwnProc);

			printf("The incoming parent message: %d and type: %ld\n", message.mtext, message.mtype);
			sim_clock->nsec += 500000000;
			if((sim_clock->sec*1000000000 + sim_clock->nsec) >= maxTimeBtwnProc)
			{	
				for(i = 0; i < maxProc; i++)
				{
					if(bitarray[i] == 0)
					{
						printf("Child born\n");
						bitarray[i] = 1;
						snprintf(count, 3, "%d", i+1);
						exec[1] = count;
						exec[2] = NULL;
						A[j] = i+1;
						j--;
						if (j == 0)
							j = maxProc;
						if((child = fork()) == 0)
						{
							execvp(exec[0], exec);
							perror("Failed to exec");
							return 0;
						}
						if (child < 0)
						{
							perror("Failed to fork");
							return 0;
						}
						break;
					}
				}
			        maxTimeBtwnNewProcSEC = rand() % 3;
			        maxTimeBtwnNewProcNS = rand() % 1000000000;
				maxTimeBtwnProc = ((maxTimeBtwnNewProcSEC*1000000000 + maxTimeBtwnNewProcNS) + (sim_clock->sec*1000000000 + sim_clock->nsec));       

			}
/*			
			if(sim_clock->shmpid)
			{
				printf("Child death zone\nMtext: %d\n", message.mtext);
				for(i = 0; i < maxProc; i++)
				{
					if(pcTable[i].childPID == sim_clock->shmpid)
					{
						printf("Child %ld died\n", (long) pcTable[i].childPID);
						printf("Total time: %lu\n", pcTable[i].totalTime);
						printf("Last burst: %lu\n", pcTable[i].lastBurstTime);
						break;
					}
				}
				sim_clock->shmpid = 0;
				bitarray[message.mtext] = 0;
				sim_clock->nsec += 500000000;
			}
*/
		}
		printf("Checking the scheduler\n");
		for(i = maxProc-1; i > 0; i--)
		{
			if(A[i])
			{
				message.mtype = A[i];
				for(k = maxProc-1; k > 0; k--)
				{
					if(B[k] == 0)
					{
						B[k] = A[i];
						break;
					}
				}
				A[i] = 0;
				message.mtext = 1000000;
				msgsnd(msgqid, &message, sizeof(message), 0);
				isScheduled = 1;
				printf("Process %ld in queue A is scheduled\n", message.mtype);
				printf("The outgoing parent A queue message: %d and type: %ld\n", message.mtext, message.mtype);
				sim_clock->nsec += 50000000;
				sleep(1);
				break;
			}
		}
		if(isScheduled)
		{
			isScheduled = 0;
			continue;
		}
		for(i = maxProc-1; i > 0; i--)
       	        {
       	       		if(B[i])
               		{
               		        message.mtype = B[i];
               		        for(k = maxProc-1; k > 0; k--)
              	        	{
              		       		        if(C[k] == 0)
                       	      			{
                       	      		        	C[k] = B[i];
                       	              			break;
                       	       			}
               			}
              			B[i] = 0;
              			message.mtext = 2000000;
              			msgsnd(msgqid, &message, sizeof(message), 0);
				isScheduled = 1;
		                printf("Process %ld in queue B is scheduled\n", message.mtype);
				printf("The outgoing parent B queue message: %d and type: %ld\n", message.mtext, message.mtype);
				sim_clock->nsec += 50000000;
				sleep(1);
               			break;
               		}
		}
               	if(isScheduled)
               	{
               	        isScheduled = 0;
               	        continue;
               	}
		for(i = maxProc-1; i > 0; i--)
               	{
               	        if(C[i])
               	        {
               			message.mtype = C[i];
				C[i] = 0;
               			message.mtext = 4000000;
               			msgsnd(msgqid, &message, sizeof(message), 0);
                                printf("Process %ld in queue C is scheduled\n", message.mtype);
				printf("The outgoing parent C queue message: %d and type: %ld\n", message.mtext, message.mtype);
				sim_clock->nsec += 50000000;
				sleep(1);
               			break;
               		}
               	}
		printf("Active process indices:\n");
		for(i = 0; i < maxProc; i++)
		{
			if(bitarray[i] == 1)
			{
				printf("%d ", i);
				message.mtext = 4000000;
				message.mtype = i+1;
				msgsnd(msgqid, &message, sizeof(message), 0);
			}
		}
		printf("\n");
		sleep(1);	
	}
	
	if (child > 0)
		while(wait(NULL) > 0);

	for(i = 0; i < maxProc; i++)
		printf("ChildPID = %ld\n", (long) pcTable[i].childPID);

	printf("Finished\n");

        shmdt(pcTable);
        shmctl(shmpctid, IPC_RMID, NULL);

        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);

	if (msgctl(msgqid, IPC_RMID, NULL) == -1)
	{
		fprintf(stderr, "Message queue could not be deleted\n");
		exit(EXIT_FAILURE);
	}

        kill(0, SIGQUIT);
        return 0;
}		
