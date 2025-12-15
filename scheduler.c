#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#define PROC_COUNT 5
#define SLICE 3
#define Q_CAPACITY 20

typedef enum {
    READY,
    RUNNING,
    FINISHED
} ProcState;

typedef struct {
    int id;
    char label[32];
    int burst;
    int time_left;

    ProcState status;

    int arrival;
    int first_run;
    int finished_at;
    int wait_time;
    int turn_time;
    int resp_time;
} PCB;

PCB proc_table[PROC_COUNT];

int run_queue[Q_CAPACITY];
int q_front = 0, q_back = 0, q_size = 0;

int active_pid = -1;
int clock_tick = 0;
int done_total = 0;

struct itimerval timer_cfg;

/* Queue Operations */
void push(int pid) {
    if (q_size == Q_CAPACITY) {
        printf("Ready queue overflow!\n");
        return;
    }
    run_queue[q_back] = pid;
    q_back = (q_back + 1) % Q_CAPACITY;
    q_size++;
}

int pop() {
    if (q_size == 0) return -1;
    int pid = run_queue[q_front];
    q_front = (q_front + 1) % Q_CAPACITY;
    q_size--;
    return pid;
}

int queue_empty() {
    return q_size == 0;
}

/* Scheduler Interrupt */
void scheduler(int sig) {
    if (active_pid != -1) {
        PCB *p = &proc_table[active_pid];
        int exec = SLICE;

        if (p->time_left < SLICE) {
            exec = p->time_left;
        }

        p->time_left -= exec;
        clock_tick += exec;

        printf("\n[Time %d] Scheduler interrupt on PID %d (%s)\n",
               clock_tick, p->id, p->label);

        if (p->time_left <= 0) {
            p->status = FINISHED;
            p->finished_at = clock_tick;

            p->turn_time = p->finished_at - p->arrival;
            p->wait_time = p->turn_time - p->burst;

            printf("%s completed.\n", p->label);
            done_total++;
        } else {
            p->status = READY;
            push(active_pid);
            printf("%s quantum expired → requeued.\n", p->label);
        }
    }

    if (done_total == PROC_COUNT) return;

    if (!queue_empty()) {
        active_pid = pop();
        PCB *np = &proc_table[active_pid];

        np->status = RUNNING;

        if (np->first_run == -1) {
            np->first_run = clock_tick;
            np->resp_time = np->first_run - np->arrival;
        }

        printf("Switching to %s (PID %d)\n", np->label, np->id);
    } else {
        printf("CPU idle… waiting.\n");
        active_pid = -1;
    }
}

/* Setup initial processes */
void boot_system() {
    char *names[] = {"Renderer", "Compiler", "WebServer", "DBEngine", "AVScanner"};
    int bursts[] = {8, 4, 10, 6, 12};

    printf("Booting OS… loading PCBs\n");
    for (int i = 0; i < PROC_COUNT; i++) {
        proc_table[i].id = i;
        strcpy(proc_table[i].label, names[i]);
        proc_table[i].burst = bursts[i];
        proc_table[i].time_left = bursts[i];
        proc_table[i].status = READY;
        proc_table[i].arrival = 0;
        proc_table[i].first_run = -1;

        push(i);

        printf("Process %-12s | Burst %-2d | READY\n",
               proc_table[i].label, bursts[i]);
    }
    printf("\n");
}

/* Timer Setup */
void init_timer() {
    signal(SIGALRM, scheduler);

    timer_cfg.it_value.tv_sec = SLICE;
    timer_cfg.it_value.tv_usec = 0;
    timer_cfg.it_interval.tv_sec = SLICE;
    timer_cfg.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &timer_cfg, NULL);
}

/* Final Output */
void report() {
    printf("\n=== FINAL METRICS ===\n");
    printf("%-3s %-14s %-7s %-10s %-10s %-12s %-12s\n",
           "PID", "Name", "Burst", "Resp", "Wait", "Turnaround", "Finish");

    float avg_r = 0, avg_w = 0, avg_t = 0;

    for (int i = 0; i < PROC_COUNT; i++) {
        PCB p = proc_table[i];

        printf("%-3d %-14s %-7d %-10d %-10d %-12d %-12d\n",
               p.id, p.label, p.burst, p.resp_time,
               p.wait_time, p.turn_time, p.finished_at);

        avg_r += p.resp_time;
        avg_w += p.wait_time;
        avg_t += p.turn_time;
    }

    printf("\nAverage Response:   %.2f\n", avg_r / PROC_COUNT);
    printf("Average Waiting:    %.2f\n", avg_w / PROC_COUNT);
    printf("Average Turnaround: %.2f\n", avg_t / PROC_COUNT);
}

int main() {
    boot_system();

    if (!queue_empty()) {
        active_pid = pop();
        proc_table[active_pid].status = RUNNING;
        proc_table[active_pid].first_run = 0;
        proc_table[active_pid].resp_time = 0;

        printf("CPU starting with %s\n", proc_table[active_pid].label);
    }

    init_timer();

    while (done_total < PROC_COUNT) {
        pause();
    }

    timer_cfg.it_value.tv_sec = 0;
    setitimer(ITIMER_REAL, &timer_cfg, NULL);

    report();
    return 0;
}

