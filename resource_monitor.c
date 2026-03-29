/*
 * resource_monitor.c
 * A complete Linux CLI tool to monitor system resources using /proc filesystem.
 * Features:
 * - Real-time CPU usage % (calculated from deltas)
 * - Memory usage % and swap usage with GB values
 * - Load averages + running/total processes
 * - Refreshes every 1 second with a clean "dashboard" view
 * - Graceful exit on Ctrl+C
 *
 * Compile: gcc -o resource_monitor resource_monitor.c
 * Run:    ./resource_monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define PROC_STAT    "/proc/stat"
#define PROC_MEMINFO "/proc/meminfo"
#define PROC_LOADAVG "/proc/loadavg"
#define PROC_UPTIME  "/proc/uptime"

volatile sig_atomic_t running = 1;

void sigint_handler(int sig) {
    running = 0;
}

/* Collect CPU statistics from /proc/stat */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long guest;
    unsigned long long guest_nice;
} cpu_stats_t;

typedef struct {
    unsigned long total, free, available, buffers, cached;
    unsigned long swap_total, swap_free;
} mem_stats_t;

void print_header() {
    printf("\033[2J\033[H");  // Clear screen and move cursor to top
    time_t now = time(NULL);
    char *t = ctime(&now);
    t[strlen(t)-1] = '\0';
    printf("\033[1;36m=== Linux system resource monitor ===\033[0m        %s\n", t);
    printf("──────────────────────────────────────────────────────────────────────────\n");
    printf("Press Ctrl+C to exit...\n\n");
}

/* Read first "cpu" line from /proc/stat */
void read_cpu_stats(cpu_stats_t *stats) {
    FILE *fp = fopen(PROC_STAT, "r");
    if (!fp) {
        perror("fopen /proc/stat");
        exit(EXIT_FAILURE);
    }

    char line[512];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
           &stats->user, &stats->nice, &stats->system, &stats->idle,
           &stats->iowait, &stats->irq, &stats->softirq, &stats->steal,
           &stats->guest, &stats->guest_nice);
}

/* Calculate CPU usage percentage from two snapshots */
double calculate_cpu_usage(cpu_stats_t *prev, cpu_stats_t *curr) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_non_idle = prev->user + prev->nice + prev->system +
                                       prev->irq + prev->softirq + prev->steal;
    unsigned long long curr_non_idle = curr->user + curr->nice + curr->system +
                                       curr->irq + curr->softirq + curr->steal;

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long curr_total = curr_idle + curr_non_idle;

    unsigned long long delta_total = curr_total - prev_total;
    unsigned long long delta_idle  = curr_idle - prev_idle;

    if (delta_total == 0) return 0.0;
    return (double)(delta_total - delta_idle) * 100.0 / delta_total;
}

/* Memory statistics from /proc/meminfo */
void read_mem_stats(mem_stats_t *stats) {
    FILE *fp = fopen(PROC_MEMINFO, "r");
    if (!fp) {
        perror("fopen /proc/meminfo");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line, "MemTotal: %lu", &stats->total);
        else if (strncmp(line, "MemFree:", 8) == 0)
            sscanf(line, "MemFree: %lu", &stats->free);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line, "MemAvailable: %lu", &stats->available);
        else if (strncmp(line, "Buffers:", 8) == 0)
            sscanf(line, "Buffers: %lu", &stats->buffers);
        else if (strncmp(line, "Cached:", 7) == 0)
            sscanf(line, "Cached: %lu", &stats->cached);
        else if (strncmp(line, "SwapTotal:", 10) == 0)
            sscanf(line, "SwapTotal: %lu", &stats->swap_total);
        else if (strncmp(line, "SwapFree:", 9) == 0)
            sscanf(line, "SwapFree: %lu", &stats->swap_free);
    }
    fclose(fp);
}

/* Read load average + processes from /proc/loadavg */
void read_loadavg(double *load1, double *load2, double *load3, int *running_procs, int *total_procs) {

    FILE *fp = fopen(PROC_LOADAVG, "r");
    if (!fp) {
        perror("fopen /proc/loadavg");
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%lf %lf %lf %d/%d", load1, load2, load3, running_procs, total_procs);
    fclose(fp);
}

void print_stats(cpu_stats_t *prev_cpu, cpu_stats_t *curr_cpu, mem_stats_t *mem,
                 double load1, double load5, double load15, int running_procs, int total_procs) {
    double cpu_usage = calculate_cpu_usage(prev_cpu, curr_cpu);

    unsigned long used_mem = mem->total - mem->free - mem->buffers - mem->cached;
    double mem_percent = (double)used_mem / mem->total * 100.0;
    double swap_percent = mem->swap_total ? (double)(mem->swap_total - mem->swap_free) / mem->swap_total * 100.0 : 0.0;

    // Color coding
    const char *cpu_color = cpu_usage > 80 ? "\033[1;31m" : (cpu_usage > 60 ? "\033[1;33m" : "\033[1;32m");
    const char *mem_color = mem_percent > 80 ? "\033[1;31m" : (mem_percent > 60 ? "\033[1;33m" : "\033[1;32m");

    printf("CPU Usage      : %s%.1f%%\033[0m\n", cpu_color, cpu_usage);
    printf("Memory         : %s%.1f%%\033[0m  (%lu MB / %lu MB)\n",
           mem_color, mem_percent,
           (mem->total - mem->free - mem->buffers - mem->cached) / 1024,
           mem->total / 1024);

    printf("Available Mem  : %lu MB\n", mem->available / 1024);
    printf("Swap Usage     : %.1f%%  (%lu MB / %lu MB)\n",
           swap_percent,
           (mem->swap_total - mem->swap_free) / 1024,
           mem->swap_total / 1024);

    printf("Load Average   : %.2f  %.2f  %.2f\n", load1, load5, load15);
    printf("Processes       : %d running / %d total\n", running_procs, total_procs);

    // Simple bar for CPU
    printf("CPU: [");
    int bars = (int)(cpu_usage / 5);
    for (int i = 0; i < 20; i++) {
        if (i < bars) printf("█");
        else printf(" ");
    }
    printf("] %.1f%%\n", cpu_usage);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    cpu_stats_t prev_cpu = {0}, curr_cpu;
    int running_procs, total_procs;
    mem_stats_t mem;
    double load1, load2, load3;

    read_cpu_stats(&prev_cpu);

    int delay = 1;
    if (argc > 1) {
        delay = atoi(argv[1]);
        if (delay < 1) delay = 1;
    }

    while (running) {
        sleep(delay);
        read_cpu_stats(&curr_cpu);

        read_mem_stats(&mem);
        read_loadavg(&load1, &load2, &load3, &running_procs, &total_procs);

        /* Clear screen and print fresh dashboard */
        printf("\033[2J\033[H"); 

        print_header();
        print_stats(&prev_cpu, &curr_cpu, &mem, load1, load2, load3, running_procs, total_procs);
    }

    printf("\n\nExiting resource monitor. Goodbye!\n");
    return EXIT_SUCCESS;
}
