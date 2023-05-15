/*
HW2 - Dispatcher/Worker model with Linux pthreads
Name : Asaf Schwartz, I.D : 208436048
*/

#define _POSIX_C_SOURCE 199309L // needed for CLOCK_MONOTONIC in clock_gettime 
#define THREAD_STACK_SIZE  65536


#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>                  
#include <limits.h>




#define NUM_THREADS 4096  
#define NUM_COUNTERS 100
#define MAX_LINE_LEN 1024




// mutex definitions

// declaration of thread condition variable for queue access
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

 
// declaration of mutex for queue access
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
 

// declaration of mutex array for counter access
static pthread_mutex_t cntMutex[NUM_COUNTERS] = {PTHREAD_MUTEX_INITIALIZER};

static pthread_mutex_t terminated_num_mutex = PTHREAD_MUTEX_INITIALIZER;

// declaration of a cond variable for the dispatcher wait command; 
static pthread_cond_t dispatch_wait_cond = PTHREAD_COND_INITIALIZER;

// declaration of an int represting #(jobs in process), used for the dispatcher wait command
int jobs_in_process;

int numCounters;

// define a structure to contain the needed information to execute the thread routine for a certain thread
typedef struct thread_info {
    int index;
    char job_line[MAX_LINE_LEN];

} thread_info;


// define a structure to contain the needed information to execute the thread routine for a certain thread
typedef struct stat_info {

    // for statistics 
    long long int min_turn_around;
    long long int max_turn_around;
    long long int sum_turn_around;
    int num_of_jobs;

} stat_info;

stat_info thr_stat_info[NUM_THREADS];

// declaration of global file pointers
FILE* counterFiles[NUM_COUNTERS];
FILE* logFiles[NUM_THREADS];
FILE* logDispatcher;

// declaration of time struct indicates the test starting time, shared by all threads as reference 
struct timespec test_start;

int terminated_thr;



// ===============================
//          queue features
// ===============================

// define structure for queue of jobs maintained by the dispatcher.
// define its elements to contain the curr jobs arguments
typedef struct queue_element {
    char job_line[MAX_LINE_LEN];
    struct timespec fetching_time;
    struct queue_element* next;
} queue_element;

typedef struct queue {
    queue_element* head;
    queue_element* back;
} queue;




int is_queue_empty(queue* queue_to_check) {
    return (queue_to_check == NULL || queue_to_check->head == NULL );
}

// insert a new jobs to the queue
void enqueue(queue* queue_to_update, char* curr_job_line) {
    queue_element* elem = malloc((sizeof(queue_element)));

    memset(elem, 0, sizeof(queue_element));
 
    struct timespec fetch_time;
    clock_gettime(CLOCK_MONOTONIC, &fetch_time);
    elem->fetching_time.tv_sec = fetch_time.tv_sec;
    elem->fetching_time.tv_nsec = fetch_time.tv_nsec;

    strcpy(elem->job_line, curr_job_line);
    elem->next = NULL;

    if (is_queue_empty(queue_to_update) == 1) {
        // the queue is empty, the elem is the first in the queue and it is the head and the back of the queue
        queue_to_update->head = elem;
        queue_to_update->back = elem; 
    }
    else {
        // there are elements in the queue, add the curr elem to the back of the line
        queue_to_update->back->next = elem; // connect the elem to the linked list
        queue_to_update->back = elem; // update the back of the queue
    }
}

// remove the head of the queue
void dequeue(queue* queue_to_update, char* curr_job_line) {

    // If queue is empty, return NULL
    if (queue_to_update->head == NULL) {
        printf("queue is empty\n");
        return;
    }
    queue_element* curr_head_queue = queue_to_update->head;
    
    strncpy(curr_job_line, curr_head_queue->job_line, MAX_LINE_LEN);
    // update new head
    queue_to_update->head = queue_to_update->head->next;

    // if the new head is NULL, the dequeu made the queue empty, set the back of the queue as NULL as well
    if (queue_to_update->head == NULL) {
        queue_to_update->back = NULL;
    }

    memset(curr_head_queue, 0 , sizeof(queue_element));
    free(curr_head_queue);
}
// the thread function - all along the test. while there are no jobs in the queue - sleep, 
// else execute the job

// A utility function to create an empty queue
queue* createQueue()
{
    queue* q = (queue*)malloc(sizeof(queue));
    q->head = NULL;
    q->back = NULL;
    return q;
}


// ===============================
//  global parameters definitions
// ===============================

queue* job_q;
int dispatcher_done = 0;
int logEn;




void inc_dec_cnt(int cnt_idx, int inc_dec) {
    FILE* cntFile;
    char cntFileName[15];
    long long int cntValue;
    

    pthread_mutex_lock(&cntMutex[cnt_idx]);

    // read current counter value 
    sprintf(cntFileName, "counter%02d.txt", cnt_idx);
    cntFile = fopen(cntFileName, "r");
    fscanf(cntFile, "%lld", &cntValue);
    fclose(cntFile);

    // update the current counter value
    cntValue = cntValue + inc_dec;

    cntFile = fopen(cntFileName, "w");
    fprintf(cntFile, "%lld", cntValue);
    fclose(cntFile);
    pthread_mutex_unlock(&cntMutex[cnt_idx]);

    return;


}

void exec_cmd(char** job_args) {
    // define a cmd variable, 
    //      0 - idle
    //      1 - msleep
    //      2 - increment / decrement
    //          inc_dec,
    //              1 - inc
    //             -1 - dec 
    int cmd = 0;
    int inc_dec = 0;
    int value = 0; 
    
    for (int i=0 ; (i < MAX_LINE_LEN) && (job_args[i] != NULL) && ( strcmp(job_args[i], "\0") != 0 ) ; i ++) {
        if (strstr(job_args[i], "msleep") != NULL) {
            cmd = 1;
        }

        else if (cmd == 1) {
            value = atoi(job_args[i]);
            usleep(value*1000);
            cmd = 0;
            
        }


        else if ( strstr(job_args[i], "increment") != NULL ) {
            cmd = 2;
            inc_dec = 1;
        }

        else if (strstr(job_args[i], "decrement") != NULL) {
            cmd = 2;
            inc_dec = -1;
        }

        else if (strstr(job_args[i], "repeat") != NULL) {
            int rpt_val = atoi(job_args[i + 1]); 
            while (rpt_val != 0) {
                // send the job_args without the two first elements ("repeat" + "x")
                exec_cmd(job_args + i + 2);
                rpt_val --;
            }
            return;
        }


        else if (cmd == 2) {
            value = atoi(job_args[i]); // the counter index 
            if (value > numCounters - 1) {
                printf("couldn't access to exceeding counter index ( higher than the defined #counters = %d)\n", numCounters);
                return;
            }          
            inc_dec_cnt(value, inc_dec);

            cmd = 0;
            
        }

        
        else if ( (strcmp(job_args[i], "worker") == 0 ) | ( strcmp(job_args[i], ";") == 0) ) {
            continue;
        }

        else {
            // printf("else : %s\n", job_args[i]); // j
            // return;
        }
    }

}


void exec_thread_jobs(char* jobs_line) {
    // analyze the current job line
    char* job_args[MAX_LINE_LEN];
    // convert the line into arguments
    char * token = strtok(jobs_line, " ;"); 
    // get next tokens - the command arguments
    int i = 0;
    while (token != NULL) { 
        job_args[i] = token;  
        i ++;
        token = strtok(NULL, " ;");
    }
    if (strstr(job_args[1], "repeat") != NULL) {
        int rpt_val = atoi(job_args[2]); 
        while (rpt_val != 0) {
            // send the job_args without the two first elements ("repeat" + "x") 
            exec_cmd(job_args + 3);
            rpt_val --;

        }
    }
    else {
        exec_cmd(job_args);
    }
    for (int i=0 ; (i < MAX_LINE_LEN) && (job_args[i] != NULL) && ( strcmp(job_args[i], "\0") != 0 ) ; i ++) {

        strcpy(job_args[i], "\0"); 
    }

}

void print_log(int thr_index, char* job_line, long long int started_job, long long int finished_job){
    
    char logFile[20];
    sprintf(logFile, "thread%04d.txt", thr_index);
    logFiles[thr_index] = fopen(logFile, "a+");
    fprintf (logFiles[thr_index],"Time %lld: START job %s \n", started_job, job_line);
    fprintf (logFiles[thr_index],"Time %lld: END job %s \n",   finished_job, job_line);
    fclose(logFiles[thr_index]);
}



// ===============================
//          thread function
// ===============================
void *thr_func (void *thr_info) {
    return;
    printf("4\n");
    struct thread_info* t_info = (thread_info *) thr_info;
    struct timespec curr_jobs_fetch_time;
    long long int curr_turn_around_time;

    long long int max_turn_around = 0;
    long long int min_turn_around = -1;
    int num_of_jobs = 0;
    int sum_turn_around = 0;


    while (true) {

        pthread_mutex_lock(&queue_lock);

        if (is_queue_empty(job_q) == 1) {
            
            // printf("4.1\n"); // q
            // if the thread succeed aquiring the lock after the program finished, break (to unlock and exit)
            if (dispatcher_done) {
                // printf("4.2\n"); // q
                break;
            }

            // if the queue is empty, sleeping until it will be filled (save CPU time by avoiding busy waiting)
            // printf("thread : %d, stuck in cond\n", t_info->index); //b
            // printf("4.3\n"); // q
            // printf("thread : %d , wait for job\n", t_info->index); // q
            // in case the dispatcher is waiting for the queue to be empty (dispatcher still not done)
            pthread_cond_signal(&dispatch_wait_cond);  

            pthread_cond_wait(&queue_cond, &queue_lock);
            pthread_mutex_unlock(&queue_lock);

            // printf("thread : %d , done waiting for job\n", t_info->index); // q

            
        }   
        // printf("4.9\n"); // q
        // if queue is not empty, taking the head queue job
        else {
            // printf("5\n"); // q
            // strcpy(t_info->job_line, job_q->head->job_line);
            usleep(100);
            curr_jobs_fetch_time.tv_sec = job_q->head->fetching_time.tv_sec;
            curr_jobs_fetch_time.tv_nsec = job_q->head->fetching_time.tv_nsec;
            // printf("6 \n"); // q
            printf("6\n"); // l

            dequeue(job_q, t_info->job_line);
            // printf("%")
            printf("7\n"); // l

            jobs_in_process --;

            // for statistics
            num_of_jobs ++;

            pthread_mutex_unlock(&queue_lock);

        
            struct timespec start_time, end_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            char job_line_copy[MAX_LINE_LEN];
            strcpy(job_line_copy, t_info->job_line);
            printf("8\n"); // l
            printf("%s", job_line_copy); // l

            usleep(100);
            exec_thread_jobs(job_line_copy); 
                 
            // printf("thread : %d, executed %s\n", t_info->index, t_info->job_line); //b
            
            printf("9\n"); // l


            clock_gettime(CLOCK_MONOTONIC, &end_time);
            

            curr_turn_around_time = (end_time.tv_sec - curr_jobs_fetch_time.tv_sec) * 1000000 + (end_time.tv_nsec - curr_jobs_fetch_time.tv_nsec) / 1000 ;
            


            if (curr_turn_around_time > max_turn_around) {
               max_turn_around = curr_turn_around_time; 
            }

            // update the min turn around if the current is lower, or if the min was not set yet (equals -1)
            if (curr_turn_around_time < min_turn_around || min_turn_around == -1) {
                min_turn_around = curr_turn_around_time; 
            }

            sum_turn_around += curr_turn_around_time;

            if (logEn) {
                // the timespec struct haws two fields - seconds and nanoseconds, the required print should be in miliseconds
                // it is required to represent the start job time, as the time passed since the start of the test
                long long int start_job = (start_time.tv_sec - test_start.tv_sec) * 1000000 + (start_time.tv_nsec - test_start.tv_nsec) / 1000 ;
                long long int finish_job = (end_time.tv_sec - test_start.tv_sec) * 1000000 + (end_time.tv_nsec - test_start.tv_nsec) / 1000 ;

        
                print_log(t_info->index, t_info->job_line, start_job, finish_job);
                for (int i=0 ; (i < MAX_LINE_LEN) && (job_line_copy[i] != '\0') ; i ++) {
                    t_info->job_line[i] = '\0'; 
                }
            }
        }
    }

    // exit the current thread - after the break of the loop the mutex is taken 
    pthread_cond_signal(&queue_cond); // wake up one of the waiting to access the queue threads 
    pthread_mutex_unlock(&queue_lock);
    // printf("thread = %d killed \n", t_info->index); //b
    
    // for statistics
    thr_stat_info[t_info->index].max_turn_around = max_turn_around;
    thr_stat_info[t_info->index].min_turn_around = min_turn_around;
    thr_stat_info[t_info->index].num_of_jobs = num_of_jobs;
    thr_stat_info[t_info->index].sum_turn_around = sum_turn_around;
    
    pthread_mutex_lock(&terminated_num_mutex);
    terminated_thr ++;
    pthread_mutex_unlock(&terminated_num_mutex);

    pthread_exit(NULL);

}




void print_statistics(int numThreads ,stat_info stat_info_arr[NUM_THREADS]) {
  // initialize a statistics file
    struct timespec program_end_time;
    clock_gettime(CLOCK_MONOTONIC, &program_end_time);

    FILE* statFile;
    statFile= fopen("stats.txt", "w");
    
    long long int max_turn_around = stat_info_arr[0].max_turn_around;
    long long int min_turn_around = stat_info_arr[0].min_turn_around;
    long long int sum_turn_around = stat_info_arr[0].sum_turn_around;
    long long int num_of_jobs = stat_info_arr[0].num_of_jobs;

    for (int i = 1; i < numThreads ; i ++) {
        if (max_turn_around < stat_info_arr[i].max_turn_around) {
            max_turn_around = stat_info_arr[i].max_turn_around;
        }
        if (min_turn_around < stat_info_arr[i].min_turn_around) {
            min_turn_around = stat_info_arr[i].min_turn_around;
        }
        sum_turn_around += stat_info_arr[i].sum_turn_around;
        num_of_jobs += stat_info_arr[i].num_of_jobs;
    }
    long long int avg_turn_around = sum_turn_around/num_of_jobs;
    long long int total_run_time;
    total_run_time = (program_end_time.tv_sec - test_start.tv_sec) * 1000000 + (program_end_time.tv_nsec - test_start.tv_nsec) / 1000 ;
    fprintf(statFile,"total running time: %lld milliseconds\n",total_run_time);
    fprintf(statFile,"sum of jobs turnaround time: %lld milliseconds\n",sum_turn_around);
    fprintf(statFile,"min job turnaround time: %lld milliseconds\n",min_turn_around);
    fprintf(statFile,"average job turnaround time: %lld milliseconds\n",avg_turn_around);
    fprintf(statFile,"max job turnaround time: %lld milliseconds\n",max_turn_around);
    

    fclose(statFile);



}



int main(int argc, const char *argv[]) {

    usleep(200);


    // ===============================
    //          initializations
    // ===============================

    // check if the calling to hw2 is valid, i.e. hw2 <cmdfile.txt> <# threads> <# counters> <log_en>, argc = 5
    clock_gettime(CLOCK_MONOTONIC, &test_start);
    if (argc != 5) { 
        if(argc < 5) {
            printf("At least one argument is missing, the expected form :\n");
            printf("hw2 <cmdfile.txt> <# threads> <# counters> <log_en>\n");
            exit(-1);
        } 
        else {
            printf("\nToo many arguments, the expected form :\n");
            printf("hw2 <cmdfile.txt> <# threads> <# counters> <log_en>\n");
            exit(-1);
        }
    }

    // open the cmd file in a read mode
    const char* cmd_file_name = argv[1];
    FILE* cmdFile = fopen(cmd_file_name, "r");
    if (cmdFile == 0){
        printf("Could not open %s to read from\n", cmd_file_name);
        exit(-1);
    }

    // initialize an array of counter file pointers, of size num_counters
    numCounters = atoi(argv[3]);
    if (numCounters < 0 || numCounters > NUM_COUNTERS) {
        printf("invalid number of counters [0 < numCounters < %d]\n", NUM_COUNTERS);
        exit(-1);
    }
    long long int lld_zero = 0; // the counter files initial value
    for (int i = 0 ; i < numCounters ; i++) {
        char counter_f_name[16];
        sprintf(counter_f_name, "counter%02d.txt", i); // string print function is needed to use an int varibale in a strin
        counterFiles[i] = fopen(counter_f_name, "w");
        fprintf(counterFiles[i], "%lld", lld_zero);
        fclose(counterFiles[i]);
    }

    // initialize an array of threads, of size num_threads
    int numThreads = atoi(argv[2]);
    if (numThreads < 0 || numThreads > NUM_THREADS) {
        printf("invalid number of threads [0 < numThreads < %d]\n", NUM_THREADS);
    }
    terminated_thr = 0;

    // initialize a trace file to the dispatcher, if log enbale == 1
    logEn = atoi(argv[4]);
    if (logEn == 1) {
    
        logDispatcher= fopen("dispatcher.txt", "w");

    }

    // initialize an array of trace file pointers to all threads, of size num_threads
    pthread_t threads[numThreads];
    thread_info threads_info_array[numThreads];
    pthread_attr_t  attrs;

    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_cond, NULL);

    // create a job queue
    char job_line[MAX_LINE_LEN];
    struct timespec job_received_time;
    jobs_in_process = 0;
    job_q = createQueue();
    int counter = 0 ;
    pthread_attr_init(&attrs);
    pthread_attr_setstacksize(&attrs, PTHREAD_STACK_MIN + 0x4000);
    for (int i = 0 ; i < numThreads ; i++) {
        if (logEn == 1) {
            char tlog_f_name[16];
            sprintf(tlog_f_name, "thread%04d.txt", i); // string print function is needed to use an int varibale in a strin
            logFiles[i] = fopen(tlog_f_name, "w");
            fclose(logFiles[i]);
        }
        threads_info_array[i].index = i; // send the thr_data[i] structure to the function call for thread i 
        
        if ( pthread_create(&threads[i], &attrs, &thr_func, (void *)&threads_info_array[i]) ) {
            
            printf("errno is %d\n", errno);
            perror("pthread_create failed : ");
            printf("counter = %d\n", counter); // l
            exit(-1);

        }
        
        // printf("counter = %d\n", counter); // l
        counter ++ ;

        // if (succues != 0) {
        //     printf(" error code %d\n", succues);
        //     exit(-1);
        // }
    }
    pthread_attr_destroy(&attrs);
    printf("counter = %d\n", counter); // l
    
    
    // ===============================
    //          jobs handling
    // ===============================
    


    while (fgets(job_line, MAX_LINE_LEN, cmdFile)) {

        // write to dispatcher log
        if (logEn) {
            // the timespec struct haws two fields - seconds and nanoseconds, the required print should be in miliseconds
            // it is required to represent the received job time, as the time passed since the start of the test to the current job reading
            clock_gettime(CLOCK_MONOTONIC, &job_received_time);
            long long int job_read_time = (job_received_time.tv_sec - test_start.tv_sec) * 1000000 + (job_received_time.tv_nsec - test_start.tv_nsec) / 1000 ;
            fprintf (logDispatcher,"Time %lld: START job %s \n", job_read_time, job_line);
        }

        // get first token, i.e., first word of the inserted line - the command descriptor
        char job_line_copy[MAX_LINE_LEN]; // strtok is damaging the inserted string
        strcpy(job_line_copy, job_line);
        char * cmdType = strtok(job_line_copy, " "); 

        if (strcmp(cmdType, "worker") == 0 ) {
            // add job to queue           
            // printf("dispatcher try lock\n"); // q 
            pthread_mutex_lock(&queue_lock); // lock the access to the queue, a shared resource for all threads
            // printf("4.5\n"); // q
            // printf("dispatcher locked\n"); // q
            jobs_in_process ++;
            enqueue(job_q, job_line);

            pthread_cond_signal(&queue_cond); // wake up one of the waiting to access the queue threads 
            pthread_mutex_unlock(&queue_lock); // unlock the access to the queue
        }

        else if (strcmp(cmdType, "dispatcher_msleep") == 0 ) {

            // Sleep for x milliseconds before continuing to process the next line 
            // in the input file
            int sleep_time;
             
            if (cmdType != NULL) { 
                cmdType = strtok(NULL, " ");
                sleep_time = atoi(cmdType);
            
            }
            sleep (sleep_time/ 1000); // sleep units are in second, the time inserted represented in miliseconds
        }
        else if (strcmp(cmdType, "dispatcher_wait") == 0 ) {
            // printf("must be a better soultion\n"); // guy - waiting option supposed to be with cond?


            while (jobs_in_process != 0) {
                pthread_mutex_lock(&queue_lock); // lock the access to the queue, a shared resource for all threads
                pthread_cond_wait(&dispatch_wait_cond, &queue_lock);

                pthread_mutex_unlock(&queue_lock); // unlock the access to the queue


            }

            
        }

    
    }

    // Note - lock must be taken before updating the global flag distpatcher done
    pthread_mutex_lock(&queue_lock);
    dispatcher_done = 1;
    pthread_mutex_unlock(&queue_lock);

    fclose(logDispatcher);
    // printf("got finally\n"); //b
    // Wait for all pending background commands to complete before continuing to
    // process the next line in the input file
    while (terminated_thr != numThreads) { 
            pthread_mutex_lock(&queue_lock);
            pthread_cond_signal(&queue_cond); // wake up one of the waiting to access the queue threads 
            pthread_mutex_unlock(&queue_lock);
    }
    // pthread_cond_signal(&queue_cond); // wake up one of the waiting to access the queue threads 
    // pthread_mutex_unlock(&queue_lock);

    for (int i = 0; i < numThreads; i++) {

        pthread_join(threads[i], NULL);  

    }
    free(job_q);

  

    print_statistics(numThreads ,thr_stat_info);
    
    printf("################### test finished ###################\n"); //b





}