/*
Author: Adam Handke
Task: Dining philosophers problem with priorities
Solved using System V IPC (semaphors and shared memory)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

int sem_widelcy;    //fork semaphores
int sem_glodomorow; //philospher semaphores
int start;      //dining start semaphore
int shmid[6];   //shared memory id table
int* bufshm[6]; //table of pointers to shm buffers
struct sembuf op;

//semaphore up
void podnies(int semid, int semnum){
   op.sem_num = semnum;
   op.sem_op = 1;
   op.sem_flg = 0;
   semop(semid, &op, 1);
}

//semaphore down
void opusc(int semid, int semnum){
   op.sem_num = semnum;
   op.sem_op = -1;
   op.sem_flg = 0;
   semop(semid, &op, 1);
}

//philospher function
int glodomor(int n){

    int prawy, lewy;
    op.sem_flg = 0;

    //random time
    unsigned int seed, random_wait;
    seed = time(NULL) ^ getpid() ^ n;
    random_wait = rand_r(&seed)%500000;

    //giving forks to right and left
    //the last philospher (5th) has reversed fork numbers
    //this prevents deadlocks
    if(n == 1){
        prawy = 1;
        lewy = 5;
    }
    else if (n == 5){
        //zamienione!
        prawy = 4;
        lewy = 5;
    }
    else{
        prawy = n;
        lewy = n-1;
    }

    //informing through a semaphore that a philosopher is ready
    opusc(start, 0);
    printf("Philosopher_%d\t at the table\n", n);

    //checking if the rest is ready
    op.sem_op = 0;
    op.sem_num = 0;
    semop(start, &op, 1);

    int zjadl=0; //priority
    //main loop
    while(zjadl < 5){
        //printf("Glodomor_%d\t przygotowuje sie\n", n);

        //solution with priorities and resource hiarchy

        //giving a priority with shm
        opusc(sem_glodomorow, n);
        *bufshm[n] = zjadl;
        podnies(sem_glodomorow, n);

        //taking the left fork
        opusc(sem_widelcy, lewy);
        //printf("Glodomor_%d\t ma widelec %d\n", n, lewy);

        //checking the left neighbour's priority
        opusc(sem_glodomorow, lewy);

        if(*bufshm[lewy] == -1 || *bufshm[lewy] >= zjadl){
            //the left neighbour doesn't want to eat or has eaten more or the same (lower priority)
            //so we can continue
            podnies(sem_glodomorow, lewy);
        }
        else{
            //the left neighbour has eaten less (higher priority)
            //current philosopher stops

            //stating this philosophers priority in shm as -1 (uninteresed)
            podnies(sem_glodomorow, n);
            *bufshm[n] = -1;
            opusc(sem_glodomorow, n);

            //freeing the left negihbour's priority semaphore
            podnies(sem_glodomorow, lewy);

            //letting go of left fork
            podnies(sem_widelcy, lewy);

            continue;
        }

        //taking the right fork
        opusc(sem_widelcy, prawy);
        //printf("Glodomor_%d\t ma widelec %d\n", n, prawy);

        //checking the right neighbour's priority
        opusc(sem_glodomorow, prawy);

        if(*bufshm[prawy] == -1 || *bufshm[prawy] >= zjadl){
            //the right neighbour doesn't want to eat or has eaten more or the same (lower priority)
            //so we can continue
            podnies(sem_glodomorow, prawy);
        }
        else{
            //the right neighbour has eaten less (higher priority)
            //current philosopher stops

           //stating this philosophers priority in shm as -1 (uninteresed)
            opusc(sem_glodomorow, n);
            *bufshm[n] = -1;
            podnies(sem_glodomorow, n);

            //freeing the left negihbour's priority semaphore
            podnies(sem_glodomorow, prawy);

            //letting go of right fork
            podnies(sem_widelcy, prawy);

            //letting go of left fork
            podnies(sem_widelcy, lewy);

            continue;
        }
        //forks and priorities are ok - our philosopher can eat!

        //eating in random time
        usleep(random_wait);
        zjadl++;
        printf("Philosopher_%d\t has eaten portion number %d\n", n, zjadl);

        //stating this philosophers priority in shm as -1 (uninteresed)
        opusc(sem_glodomorow, n);
        *bufshm[n] = -1;
        podnies(sem_glodomorow, n);

        //letting go of right fork
        podnies(sem_widelcy, prawy);

        //letting go of left fork
        podnies(sem_widelcy, lewy);
    }

    printf("Philosopher_%d\t has finished eating %d protions\n", n, zjadl);

    exit(n);
}

int main(){
    int i, pid, tmp, status;
    pid_t glod[6]; //PID table of children

    //initializing 6 fork semaphores
    //0 is unused, 1-5 are used
    sem_widelcy = semget(IPC_PRIVATE, 6, IPC_CREAT|0600);
    for(i=1; i<=5; i++){
        semctl(sem_widelcy, i, SETVAL, 1);
    }
    //initializing start semaphore
    start = semget(IPC_PRIVATE, 1, IPC_CREAT|0600);
    semctl(start, 0, SETVAL, 5);

    //initializing philosopher priority semaphores
    //0 is unused, 1-5 are used
    sem_glodomorow = semget(IPC_PRIVATE, 6, IPC_CREAT|0600);
    for(i=1; i<=5; i++){
        semctl(sem_glodomorow, i, SETVAL, 1);
    }

    //creating 5 shared memory segments for priorities
    for(i=1; i<=5; i++){
        shmid[i] = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|0600);
        bufshm[i] = (int*)shmat(shmid[i], NULL, 0);
        *bufshm[i] = -1; //initially all philosophers do not want to eat
    }

    //generating philosopher processes
    for(i=1; i<=5; i++){
        pid = fork();
        if(pid == 0){   //child
            tmp = glodomor(i);  //child runs philosopher function
            exit(tmp);          //when its finished, then it finishes
        }
        else{   //father
            glod[i] = pid;
        }
    }

    //waiting for the completion of all philosophers
    for(i=1; i<=5; i++){
        waitpid(glod[i], &status, 0);
    }

    //removing semaphores
    semctl(sem_widelcy, 0, IPC_RMID, 0);
    semctl(sem_glodomorow, 0, IPC_RMID, 0);
    semctl(start, 0, IPC_RMID, 0);

    return 0;
}
