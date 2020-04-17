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

int sem_widelcy;    //semafory widelcy
int sem_glodomorow; //semafory glodomorow
int start;      //semafor poczatku uczty
int shmid[6];   //tablica id shared memory
int* bufshm[6]; //tablica wskaznikow na bufory shared memory
struct sembuf op;

void podnies(int semid, int semnum){
   op.sem_num = semnum;
   op.sem_op = 1;
   op.sem_flg = 0;
   semop(semid, &op, 1);
}

void opusc(int semid, int semnum){
   op.sem_num = semnum;
   op.sem_op = -1;
   op.sem_flg = 0;
   semop(semid, &op, 1);
}

int glodomor(int n){

    int prawy, lewy;
    op.sem_flg = 0;

    //losowanie czasu
    unsigned int seed, random_wait;
    seed = time(NULL) ^ getpid() ^ n;
    random_wait = rand_r(&seed)%500000; //losowe mikrosekundy, max pol sekundy

    //przypisanie widelcow z prawej i z lewej
    //przy czym ostatni (5-ty) glodomor ma zamienione numery widelcow
    //ta zamiana dla ostatniego pozwala na zapobieganie zakleszczeniom
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

    //poinformowanie przez semafor, ze glodomor jest gotowy
    opusc(start, 0);
    printf("Glodomor_%d\t przy stole\n", n);

    //sprawdzenie, czy pozostali sa gotowi
    op.sem_op = 0;
    op.sem_num = 0;
    semop(start, &op, 1);

    int zjadl=0; //priorytet
    //petla glowna
    while(zjadl < 5){
        //printf("Glodomor_%d\t przygotowuje sie\n", n);

        //rozwiazanie z hierarchia zasobow i priorytetami

        //podanie swojego priorytetu w shm
        opusc(sem_glodomorow, n);
        *bufshm[n] = zjadl;
        podnies(sem_glodomorow, n);

        //pobranie lewego widelca
        opusc(sem_widelcy, lewy);
        //printf("Glodomor_%d\t ma widelec %d\n", n, lewy);

        //sprawdzenie priorytetu lewego sasiada
        opusc(sem_glodomorow, lewy);

        if(*bufshm[lewy] == -1 || *bufshm[lewy] >= zjadl){
            //konkurent z lewej nie chce wcale lub zjadl wiecej lub tyle samo (nizszy priorytet)
            //czyli mozemy leciec dalej
            //zwalnia semafor sprawdzania priorytetu lewego sasiada
            podnies(sem_glodomorow, lewy);
        }
        else{
            //konkurent z lewej zjadl mniej (ma wyzszy priorytet)
            //obecny glodomor sie wycofuje

            //podanie swojego priorytetu w shm jako -1 (niezainteresowany)
            podnies(sem_glodomorow, n);
            *bufshm[n] = -1;
            opusc(sem_glodomorow, n);

            //zwalnia semafor sprawdzania priorytetu lewego sasiada
            podnies(sem_glodomorow, lewy);

            //oddaje lewy widelec
            podnies(sem_widelcy, lewy);

            //przechodzi do nastepnej iteracji petli
            continue;
        }

        //pobranie prawego widelca
        opusc(sem_widelcy, prawy);
        //printf("Glodomor_%d\t ma widelec %d\n", n, prawy);

        //sprawdzenie priorytetu prawego sasiada
        opusc(sem_glodomorow, prawy);

        if(*bufshm[prawy] == -1 || *bufshm[prawy] >= zjadl){
            //konkurent z prawej nie chce wcale lub zjadl wiecej lub tyle samo (nizszy priorytet)
            //czyli mozemy leciec dalej
            //zwalnia semafor sprawdzania priorytetu prawego sasiada
            podnies(sem_glodomorow, prawy);
        }
        else{
            //konkurent z prawej zjadl mniej (ma wyzszy priorytet)
            //obecny glodomor sie wycofuje

            //podanie swojego priorytetu w shm jako -1 (niezainteresowany)
            opusc(sem_glodomorow, n);
            *bufshm[n] = -1;
            podnies(sem_glodomorow, n);

            //zwalnia semafor sprawdzania priorytetu prawego sasiada
            podnies(sem_glodomorow, prawy);

            //oddaje prawy widelec
            podnies(sem_widelcy, prawy);

            //oddaje lewy widelec
            podnies(sem_widelcy, lewy);

            //przechodzi do nastepnej iteracji petli
            continue;
        }
        //widelce i priorytety ok - mozna jesc

        //zjadanie w wylosowanym czasie
        usleep(random_wait);
        zjadl++;
        printf("Glodomor_%d\t zjadl porcje nr %d\n", n, zjadl);

        //podanie swojego priorytetu w shm jako -1 (niezainteresowany)
        opusc(sem_glodomorow, n);
        *bufshm[n] = -1;
        podnies(sem_glodomorow, n);

        //odlozenie prawego widelca
        podnies(sem_widelcy, prawy);

        //odlozenie lewego widelca
        podnies(sem_widelcy, lewy);
    }

    printf("Glodomor_%d\t skonczyl - zjadl %d porcji\n", n, zjadl);

    exit(n);
}

int main(){
    int i, pid, tmp, status;
    pid_t glod[6]; //tablica PID-ow potomkow

    //inicjalizacja semaforow widelcy
    //6 sztuk
    //0 nieuzywany, 1-5 semafory widelcow
    sem_widelcy = semget(IPC_PRIVATE, 6, IPC_CREAT|0600);
    for(i=1; i<=5; i++){
        semctl(sem_widelcy, i, SETVAL, 1);
    }
    //inicjalizacja semafora startu
    start = semget(IPC_PRIVATE, 1, IPC_CREAT|0600);
    semctl(start, 0, SETVAL, 5);

    //inicjalizacja semaforow glodomorow (priorytety)
    //0 nieuzywany, 1-5 semafory glodomorow do sprawdzania priorytetow
    sem_glodomorow = semget(IPC_PRIVATE, 6, IPC_CREAT|0600);
    for(i=1; i<=5; i++){
        semctl(sem_glodomorow, i, SETVAL, 1);
    }

    //tworze 5 segmentow pamieci wspoldzielonej do priorytetow
    for(i=1; i<=5; i++){
        shmid[i] = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|0600);
        bufshm[i] = (int*)shmat(shmid[i], NULL, 0);
        *bufshm[i] = -1; //poczatkowo zaden glodomor nie jest chetny
    }

    //generacja procesow glodomorow
    for(i=1; i<=5; i++){
        pid = fork();
        if(pid == 0){   //potomek
            tmp = glodomor(i);  //potomek uruchamia funkcje glodomora
            exit(tmp);          //a gdy ja zakonczy, to sie zakancza
        }
        else{   //macierzysty
            glod[i] = pid;
        }
    }

    //oczekiwanie na zakonczenie wszystkich glodomorow
    for(i=1; i<=5; i++){
        waitpid(glod[i], &status, 0);
    }

    //usuniece semaforow
    semctl(sem_widelcy, 0, IPC_RMID, 0);
    semctl(sem_glodomorow, 0, IPC_RMID, 0);
    semctl(start, 0, IPC_RMID, 0);

    return 0;
}
