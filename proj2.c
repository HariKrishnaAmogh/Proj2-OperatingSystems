// — proj2.c template

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <errno.h>
#include <unistd.h>
#include <time.h> // For srand/rand seeding

// --- Message Queue Definitions ---
#define QUEUE_NAME_1 "/mqd_mb1"
#define QUEUE_NAME_2 "/mqd_mb2"
#define MAX_MSGS 10
#define MAX_MSG_SIZE 256
#define NUM_REQUESTS 100

// --- Log File Definitions ---
#define LOG_TH1_NAME "stage2.log"
#define LOG_TH2_NAME "stage3.log"

// --- Data Structures (Messages) ---

// Message format for the first queue (Raw Request)
typedef struct {
    int id;
    char type[8];        // "READ" or "WRITE"
    int block_address;   // Logical block address
	int priority;
} msg_stage1_t;

// Message format for the second queue (Validated Request)
typedef struct {
    int id;
    int type_code;       // 0 for READ, 1 for WRITE
    int physical_address; // Translated physical disk address
    int priority;        // For scheduling (e.g., 0-9)
} msg_stage2_t;

// --- Prototypes ---
void* stage1_producer(void* arg);
void* stage2_con_prod(void* arg);
void* stage3_consumer(void* arg);
int bsort();
void bubbleSortHelper();

// Structure to hold all necessary descriptors and file pointers
typedef struct {
    mqd_t mqd_mb1; // Descriptor for Mailbox 1 (Stage 2 Thread)
    mqd_t mqd_mb2; // Descriptor for Mailbox 2 (Stage 3 Thread)
	FILE* input_fp; // File pointer for Stage 1 Thread input
    FILE* log_th2_fp; // Log file pointer for Stage 2 Thread output
    FILE* log_th3_fp; // Log file pointer for Stage 3 Thread execution
} thread_args_t;

// --- Main Function ---
int main(int argc, char** argv) {
	if (argc < 2 || argc > 2 || argv[1] == 0) {
		perror("Usage: ./proj2 <Input file>\n");
		exit(EXIT_FAILURE);
	}
	
    pthread_t stage1_tid, stage2_tid, stage3_tid;
    struct mq_attr attr1, attr2;
    thread_args_t args;

    // Seed the random number generator
    srand(time(NULL));

    // 1. Define Message Queue Attributes
    attr1.mq_flags = 0;
    attr1.mq_maxmsg = MAX_MSGS;
    attr1.mq_msgsize = sizeof(msg_stage1_t);
    attr1.mq_curmsgs = 0;

    attr2.mq_flags = 0;
    attr2.mq_maxmsg = MAX_MSGS;
    attr2.mq_msgsize = sizeof(msg_stage2_t);
    attr2.mq_curmsgs = 0;

    // 2. Open File streams
	if (!(args.input_fp = fopen(argv[1], "r"))){
		perror("fopen (Input File)");
		exit(EXIT_FAILURE);
	}
	else
		printf("Opened input file %s\n", argv[1]);
    args.log_th2_fp = fopen(LOG_TH1_NAME, "w");
    args.log_th3_fp = fopen(LOG_TH2_NAME, "w");
    if (!args.log_th2_fp || !args.log_th3_fp) {
        perror("fopen (Log File)");
        exit(EXIT_FAILURE);
    }
    fprintf(args.log_th2_fp, "--- Thread 2 Stage Output (Requests ready for Thread 3) ---\n");
    fprintf(args.log_th2_fp, "ID\tType\tPAddr\tPriority\n");
    fprintf(args.log_th3_fp, "--- Thread 3 Execution Log (Requests processed by priority) ---\n");
    fprintf(args.log_th3_fp, "ID\tType\tPAddr\tPriority\n");
    printf("OS Pipeline Logs opened successfully.\n");

    // 3. Unlink (Cleanup) old queues before opening
    mq_unlink(QUEUE_NAME_1);
    mq_unlink(QUEUE_NAME_2);

    // 4. Open Mailbox 1 (Thread 2 Queue)
    args.mqd_mb1 = mq_open(QUEUE_NAME_1, O_CREAT | O_RDWR, 0644, &attr1);
    if (args.mqd_mb1 == (mqd_t)-1) {
        perror("mq_open (Thread 2)");
        fclose(args.log_th2_fp);
        fclose(args.log_th3_fp);
        exit(EXIT_FAILURE);
    }

    // 5. Open Mailbox 2 (Thread 3 Queue)
    args.mqd_mb2 = mq_open(QUEUE_NAME_2, O_CREAT | O_RDWR, 0644, &attr2);
    if (args.mqd_mb2 == (mqd_t)-1) {
        perror("mq_open (Thread 3)");
        mq_close(args.mqd_mb1);
        mq_unlink(QUEUE_NAME_1);
        fclose(args.log_th2_fp);
        fclose(args.log_th3_fp);
        exit(EXIT_FAILURE);
    }
    printf("OS Pipeline Mailboxes opened successfully.\n");

    // 6. Create Threads
    pthread_create(&stage1_tid, NULL, stage1_producer, &args);
    pthread_create(&stage2_tid, NULL, stage2_con_prod, &args);
    pthread_create(&stage3_tid, NULL, stage3_consumer, &args);

    // 7. Wait for all threads to complete
    pthread_join(stage1_tid, NULL);
    pthread_join(stage2_tid, NULL);
    pthread_join(stage3_tid, NULL);

    // 8. Cleanup (Close everything)
    if (mq_close(args.mqd_mb1) == -1) perror("mq_close (Thread 2)");
    if (mq_close(args.mqd_mb2) == -1) perror("mq_close (Thread 3)");
    if (mq_unlink(QUEUE_NAME_1) == -1) perror("mq_unlink (Thread 2)");
    if (mq_unlink(QUEUE_NAME_2) == -1) perror("mq_unlink (Thread 3)");
	if (fclose(args.input_fp) == -1) perror("fclose (Input File)");
    if (fclose(args.log_th2_fp) == EOF) perror("fclose (Thread 2 Log)");
    if (fclose(args.log_th3_fp) == EOF) perror("fclose (Thread 3 Log)");

    printf("\n--- OS Pipeline Simulation Complete. Mailboxes and Logs cleaned up. ---\n");
    return 0;
}

// ----------------------------------------------------------------------------------
// --- Thread 1: (Producer for Q1) 
// ----------------------------------------------------------------------------------
void* stage1_producer(void* arg) {
    thread_args_t *args = (thread_args_t*)arg;
    msg_stage1_t msg;

    printf("T1: Stage1 thread starting\n\n");

    for (int i = 0; i < NUM_REQUESTS; i++) {
        if (fscanf(args->input_fp, "%d %7s %d %d",
                   &msg.id, msg.type, &msg.block_address, &msg.priority) != 4) {
            perror("fscanf (stage1_producer)");
            break;
        }

        // Send to mailbox 1 with priority 0
        if (mq_send(args->mqd_mb1, (const char*)&msg, sizeof(msg), 0) == -1) {
            perror("mq_send (stage1_producer -> mqd_mb1)");
        } else {
	// format output.txt
            printf("T1 ID %d: sending message to Q1 (%s %d) for Q1\n",
                   msg.id, msg.type, msg.block_address);
        }

        usleep(100000); // 100ms
        bsort();
    }

    return NULL;
}

// ----------------------------------------------------------------------------------
// --- Thread 2: (Consumer for Q1, Producer for Q2) 
// ----------------------------------------------------------------------------------
void* stage2_con_prod(void* arg) {
    thread_args_t *args = (thread_args_t*)arg;
    msg_stage1_t raw;
    msg_stage2_t valid;

    printf("T2: Stage2 thread starting\n\n");
    unsigned int msgprio;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        ssize_t bytes = mq_receive(args->mqd_mb1, (char*)&raw, MAX_MSG_SIZE, &msgprio);
        if (bytes == -1) {
            perror("mq_receive (stage2_con_prod) from mqd_mb1");
            continue;
        }

        valid.id = raw.id;
        // case-insensitive check for READ & WRITE
        if (strncasecmp(raw.type, "READ", 4) == 0) valid.type_code = 0;
        else valid.type_code = 1;
        valid.physical_address = raw.block_address - 500;
        valid.priority = raw.priority;

	printf("T2 ID %d: receiving message from Q1 Validating/Translating...\n", valid.id);

        // Timestamp and log to stage2.log
        time_t now = time(NULL);
        char *tstr = ctime(&now);
        if (tstr) tstr[strcspn(tstr, "\n")] = 0; // to get rid of the newline for aesthetic purposes

        fprintf(args->log_th2_fp, "%d\t%s\t%d\t%d\t%s\n",
                valid.id,
                (valid.type_code == 0) ? "0" : "1",
                valid.physical_address,
                valid.priority,
                tstr);
        fflush(args->log_th2_fp);

        // Send to mailbox 2 using the request's priority
        if (mq_send(args->mqd_mb2, (const char*)&valid, sizeof(valid), (unsigned int)valid.priority) == -1) {
            perror("mq_send (stage2_con_prod -> mqd_mb2)");
        } else {
	   printf("T2 ID %d: sending message to Q2 Validated Request Priority: %d\n",
               valid.id, valid.priority);
        }
        bsort();
    }
    return NULL;
}

// --------------------------------------------------------------------------------------
// --- Thread 3: (Final Consumer for Q2)
// --------------------------------------------------------------------------------------
void* stage3_consumer(void* arg) {
    thread_args_t *args = (thread_args_t*)arg;
    msg_stage2_t req;

    printf("T3: Stage3 thread starting\n\n");

    unsigned int msgprio;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        ssize_t bytes = mq_receive(args->mqd_mb2, (char*)&req, MAX_MSG_SIZE, &msgprio);
        if (bytes == -1) {
            perror("mq_receive (stage3_consumer) from mqd_mb2");
            continue;
        }

        // Convert type_code back to string
        const char *typeStr = (req.type_code == 0) ? "READ" : "WRITE";

        // Timestamp and write to stage3.log
        time_t now = time(NULL);
        char *tstr = ctime(&now);
        if (tstr) tstr[strcspn(tstr, "\n")] = 0; 

        fprintf(args->log_th3_fp, "%d\t%s\t%d\t%d\t%s\n",
                req.id, typeStr, req.physical_address, req.priority, tstr);
        fflush(args->log_th3_fp);

	// formatting for output.txt
        printf("T3 ID %d: receiving message from Q2 Type: %s, Physical Address: %d, Priority: %d\n",
               req.id, typeStr, req.physical_address, req.priority);

        // simulate processing
        usleep(300000); // 300ms 
        bsort();
    }
    return NULL;
}

// --------------------------------------------------------------------------------------
// --- Bubble sort functions, do not change these
// --------------------------------------------------------------------------------------
int bsort() {
	int n, i, timeInt;

	// Seed the random number generator
	srand(time(NULL));

	timeInt = rand() % 300; //time interval
	n = 50 * (timeInt + 1);

	int arr[n];

	// looop to create and display unsorted array
	// Generate random numbers and fill the array
	for (i = 0; i < n; i++) {
		arr[i] = rand() % 100; // Generate numbers between 0 and 99
		//printf("%d", arr[i]);
	}
	//printf(" Unsorted array created ");

	bubbleSortHelper(arr, n);

	/* loop to display sorted array
	printf("/n Sorted array finished ");
	for (i = 0; i < n; i++) {
		printf("%d", arr[i]);
	}
	*/

	return 0;
}

void bubbleSortHelper(int arr[], int n) {
	int i, j, temp;
	for (i = 0; i < n - 1; i++) {
		for (j = 0; j < n - i - 1; j++) {
			if (arr[j] > arr[j + 1]) {
				// Swap elements
				temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}
