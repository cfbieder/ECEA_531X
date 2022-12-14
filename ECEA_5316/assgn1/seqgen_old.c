// Sam Siewert, December 2017
//
// Updated June 2020 for signal driven example
//
// Sequencer Generic
//
// The purpose of this code is to provide an example for how to best
// sequence a set of periodic services for problems similar to and including
// the final project in real-time systems.
//
// For example: Service_1 for camera frame aquisition
//              Service_2 for image analysis and timestamping
//              Service_3 for image processing (difference images)
//              Service_4 for save time-stamped image to file service
//              Service_5 for save processed image to file service
//              Service_6 for send image to remote server to save copy
//              Service_7 for elapsed time in syslog each minute for debug
//

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (1)
#define TRUE (1)
#define FALSE (0)

#define FIB_TEST_CYCLES (100)
#define FIB_ITER (100)

//****************************************
//Modify to only run 3 processes
//****************************************
#define NUM_THREADS (3+1)

int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE, abortS4=FALSE, abortS5=FALSE, abortS6=FALSE, abortS7=FALSE;
sem_t semS1, semS2, semS3, semS4, semS5, semS6, semS7;
struct timeval start_time_val;

typedef struct
{
    int threadIdx;
    int event_time;
    char *service_name;
    unsigned long long sequencePeriods;
} threadParams_t;


void *Sequencer(void *threadp);


void *Service_1(void *threadp);
void *Service_2(void *threadp);
void *Service_3(void *threadp);

double getTimeMsec(void);
void print_scheduler(void);


int main(void)
{
    struct timeval current_time_val;
    int i, rc, scope;
    cpu_set_t threadcpu;
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;

    printf("Starting Sequencer Demo\n");

    //clear system log
    system("echo > /dev/null | sudo tee /var/log/syslog");
    //log username
    system("logger [COURSE:1][ASSIGNMENT:3]: `uname -a`");


    gettimeofday(&start_time_val, (struct timezone *)0);
    gettimeofday(&current_time_val, (struct timezone *)0);
    //syslog(LOG_CRIT, "Sequencer @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

    CPU_ZERO(&allcpuset);

    for(i=0; i < NUM_CPU_CORES; i++)
        CPU_SET(i, &allcpuset);

    printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));


    // initialize the sequencer semaphores
    //
    if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
    if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }
    if (sem_init (&semS3, 0, 0)) { printf ("Failed to initialize S3 semaphore\n"); exit (-1); }

    mainpid=getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");
    print_scheduler();


    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    for(i=0; i < NUM_THREADS; i++)
    {

      CPU_ZERO(&threadcpu);
      CPU_SET(3, &threadcpu);

      rc=pthread_attr_init(&rt_sched_attr[i]);
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
      //rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

      threadParams[i].threadIdx=i;
    }

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:
    //

    // Servcie_1 = RT_MAX-1	@ 3 Hz
    //
    rt_param[1].sched_priority=rt_max_prio-1;
    threadParams[1].event_time = 10;
    threadParams[1].service_name = "Seq 1";
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);
    rc=pthread_create(&threads[1],               // pointer to thread descriptor
                      &rt_sched_attr[1],         // use specific attributes
                      //(void *)0,               // default attributes
                      Service_1,                 // thread function entry point
                      (void *)&(threadParams[1]) // parameters to pass in
                     );
    if(rc < 0)
        perror("pthread_create for service 1");
    else
        printf("pthread_create successful for service 1\n");


    // Service_2 = RT_MAX-2	@ 1 Hz
    //
    rt_param[2].sched_priority=rt_max_prio-2;
    threadParams[2].event_time = 20;
    threadParams[2].service_name = "Seq 2";
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]);
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Service_2, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for service 2");
    else
        printf("pthread_create successful for service 2\n");




    // Service_3 = RT_MAX-3	@ 0.5 Hz
    //
    rt_param[3].sched_priority=rt_max_prio-3;
    pthread_attr_setschedparam(&rt_sched_attr[3], &rt_param[3]);
    rc=pthread_create(&threads[3], &rt_sched_attr[3], Service_3, (void *)&(threadParams[3]));
    if(rc < 0)
        perror("pthread_create for service 3");
    else
        printf("pthread_create successful for service 3\n");



    // Wait for service threads to initialize and await relese by sequencer.
    //
    // Note that the sleep is not necessary of RT service threads are created wtih
    // correct POSIX SCHED_FIFO priorities compared to non-RT priority of this main
    // program.
    //
    // usleep(1000000);

    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    threadParams[0].sequencePeriods=900;

    // Sequencer = RT_MAX	@ 30 Hz
    //
    rt_param[0].sched_priority=rt_max_prio;
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
    rc=pthread_create(&threads[0], &rt_sched_attr[0], Sequencer, (void *)&(threadParams[0]));
    if(rc < 0)
        perror("pthread_create for sequencer service 0");
    else
        printf("pthread_create successful for sequeencer service 0\n");


   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);

    printf("\nTEST COMPLETE\n");
    return (1);
}


void *Sequencer(void *threadp)
{
    struct timeval current_time_val;
    //********************************
    // MOdified delay time to 20 msec
    //********************************

    //NOTE: timespec in seconds and nanoseconds
    struct timespec delay_time = {0,10000000}; // delay for 10 msec, 100 Hz
    struct timespec remaining_time;
    //double current_time;
    double residual;
    int rc, delay_cnt=0;
    unsigned long long seqCnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    //syslog(LOG_CRIT, "Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    do
    {
        delay_cnt=0; residual=0.0;

        //gettimeofday(&current_time_val, (struct timezone *)0);
        //syslog(LOG_CRIT, "Sequencer thread prior to delay @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
        do
        {
            rc=nanosleep(&delay_time, &remaining_time);

            if(rc == EINTR)
            {
                residual = remaining_time.tv_sec + ((double)remaining_time.tv_nsec / (double)NANOSEC_PER_SEC);

                if(residual > 0.0) printf("residual=%lf, sec=%d, nsec=%d\n", residual, (int)remaining_time.tv_sec, (int)remaining_time.tv_nsec);

                delay_cnt++;
            }
            else if(rc < 0)
            {
                perror("Sequencer nanosleep");
                exit(-1);
            }

        } while((residual > 0.0) && (delay_cnt < 100));


        seqCnt++;
        //gettimeofday(&current_time_val, (struct timezone *)0);
        ///printf("Sequencer cycle %llu @ sec=%d, msec=%d\n", seqCnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);


        if(delay_cnt > 1) printf("Sequencer looping delay %d\n", delay_cnt);


        // Release each service at a sub-rate of the generic sequencer rate
        //***************************************
        //Modifed interval calls below
        //***************************************

        // Servcie_1 = RT_MAX-1	@ every 20 msce
        if((seqCnt % 2) == 0) sem_post(&semS1);

        // Service_2 = RT_MAX-2	@ every 100 msec
        if((seqCnt % 10) == 0) sem_post(&semS2);

        // Service_3 = RT_MAX-3	@ every 150 msec
        if((seqCnt % 15) == 0) sem_post(&semS3);


        //gettimeofday(&current_time_val, (struct timezone *)0);
        //syslog(LOG_CRIT, "Sequencer release all sub-services @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    } while(!abortTest && (seqCnt < threadParams->sequencePeriods));

    sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);

    abortS1=TRUE; abortS2=TRUE; abortS3=TRUE;


    pthread_exit((void *)0);
}



/*****************************************************************************
* Fib function
******************************************************************************/
void FIB_TEST(int seqCnt, int iterCnt)
{
   for(int idx=0; idx < iterCnt; idx++)
   {
      int fib0=0;
      int fib1=1;
      int jdx=1;
      int fib = fib0 + fib1;
      while(jdx < seqCnt)
      {
         fib0 = fib1;
         fib1 = fib;
         fib = fib0 + fib1;
         jdx++;
      }
   }
 }

 /*****************************************************************************
 * Service 1
 ******************************************************************************/

 void *Service_1(void *threadp )
 {
     struct timeval current_time_val;
     int start_time, end_time;
     int event_time,run_time;
     char *ser_name;



     //double current_time;
     unsigned long long S1Cnt=0;
     threadParams_t *threadParams = (threadParams_t *)threadp;
     event_time  = threadParams->event_time;
     ser_name = threadParams->service_name;

     gettimeofday(&current_time_val, (struct timezone *)0);
     //syslog(LOG_CRIT, "%s thread @ sec=%d, msec=%d\n", ser_name,(int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
     printf("%s thread @ sec=%d, msec=%d\n", ser_name, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);



     while(!abortS1)
     {
         sem_wait(&semS1);
         S1Cnt++;

         gettimeofday(&current_time_val, (struct timezone *)0);
         start_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
         printf("# Service %s Start %llu @ sec=%d, msec=%d\n",ser_name, S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
         run_time = (int)getTimeMsec()+event_time;


         while ((int)getTimeMsec() < run_time) {
           FIB_TEST(FIB_ITER, FIB_TEST_CYCLES);
         }
         gettimeofday(&current_time_val, (struct timezone *)0);
         printf("# Service %s Finish %llu @ sec=%d, msec=%d\n", ser_name, S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
         end_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
         printf("# Service %s Run time %llu @, msec=%d\n",ser_name, S1Cnt,end_time-start_time);
         //syslog(LOG_CRIT, "# Service S1 %llu @ sec=%d, msec=%d\n", S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
     }

     pthread_exit((void *)0);
 }



void *Service_1_old(void *threadp)
{
    struct timeval current_time_val;
    double event_time;
    int run_time=10.0; //set run time for 10 milliseconds
    int start_time, end_time;


    //double current_time;
    unsigned long long S1Cnt=0;
    //threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "S1 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("S1 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);



    while(!abortS1)
    {
        sem_wait(&semS1);
        S1Cnt++;

        gettimeofday(&current_time_val, (struct timezone *)0);
        start_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
        printf("# Service S1 Start %llu @ sec=%d, msec=%d\n", S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
        event_time = (int)getTimeMsec()+run_time;


        while ((int)getTimeMsec() < event_time) {
          FIB_TEST(FIB_ITER, FIB_TEST_CYCLES);
        }
        gettimeofday(&current_time_val, (struct timezone *)0);
        printf("# Service S1 Finish %llu @ sec=%d, msec=%d\n", S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
        end_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
        printf("# Service S1 Run time %llu @, msec=%d\n",S1Cnt,end_time-start_time);
        //syslog(LOG_CRIT, "# Service S1 %llu @ sec=%d, msec=%d\n", S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    }

    pthread_exit((void *)0);
}

/*****************************************************************************
* Service 1
******************************************************************************/


void *Service_2(void *threadp )
{
    struct timeval current_time_val;
    int start_time, end_time;
    int event_time,run_time;
    char *ser_name;



    //double current_time;
    unsigned long long S2Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    event_time  = threadParams->event_time;
    ser_name = threadParams->service_name;

    gettimeofday(&current_time_val, (struct timezone *)0);
    //syslog(LOG_CRIT, "%s thread @ sec=%d, msec=%d\n", ser_name,(int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("%s thread @ sec=%d, msec=%d\n", ser_name, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);



    while(!abortS2)
    {
        sem_wait(&semS2);
        S2Cnt++;

        gettimeofday(&current_time_val, (struct timezone *)0);
        start_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
        printf("# Service %s Start %llu @ sec=%d, msec=%d\n",ser_name, S2Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
        run_time = (int)getTimeMsec()+event_time;


        while ((int)getTimeMsec() < run_time) {
          FIB_TEST(FIB_ITER, FIB_TEST_CYCLES);
        }
        gettimeofday(&current_time_val, (struct timezone *)0);
        printf("# Service %s Finish %llu @ sec=%d, msec=%d\n", ser_name, S2Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
        end_time = (int)current_time_val.tv_usec/USEC_PER_MSEC;
        printf("# Service %s Run time %llu @, msec=%d\n",ser_name, S2Cnt,end_time-start_time);
        //syslog(LOG_CRIT, "# Service S1 %llu @ sec=%d, msec=%d\n", S1Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    }

    pthread_exit((void *)0);
}

void *Service_2_old(void *threadp)
{
    struct timeval current_time_val;
    //double current_time;
    unsigned long long S2Cnt=0;
    //threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "S2 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("S2 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    while(!abortS2)
    {
        sem_wait(&semS2);
        S2Cnt++;

        gettimeofday(&current_time_val, (struct timezone *)0);
        //syslog(LOG_CRIT, "## Service S2 %llu @ sec=%d, msec=%d\n", S2Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    }

    pthread_exit((void *)0);
}

void *Service_3(void *threadp)
{
    struct timeval current_time_val;
    //double current_time;
    unsigned long long S3Cnt=0;
    //threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "S3 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("S3 thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    while(!abortS3)
    {
        sem_wait(&semS3);
        S3Cnt++;

        gettimeofday(&current_time_val, (struct timezone *)0);
        //syslog(LOG_CRIT, "### Service S3 %llu @ sec=%d, msec=%d\n", S3Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    }

    pthread_exit((void *)0);
}



double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_MONOTONIC, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
       case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
         break;
       case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
       default:
           printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }
}
