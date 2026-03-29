/* 
 * resource_monitor.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

volatile sig_atomic_t running = 1;

void sigint_handler(int sig) {
    running = 0;
}

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



void read_cpu_stats(cpu_stats_t *stats) {
    FILE *fp = fopen("/proc/stat", "r");
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



double get_cpu_percent(const cpu_stats_t *prev, const cpu_stats_t *curr) {
    unsigned long long prev_total = prev->user + prev->nice + prev->system + prev->idle +
                                    prev->iowait + prev->irq + prev->softirq + prev->steal +
                                    prev->guest + prev->guest_nice;
    unsigned long long curr_total = curr->user + curr->nice + curr->system + curr->idle +
                                    curr->iowait + curr->irq + curr->softirq + curr->steal +
                                    curr->guest + curr->guest_nice;

    unsigned long long prev_idle_time = prev->idle + prev->iowait;
    unsigned long long curr_idle_time = curr->idle + curr->iowait;

    unsigned long long delta_total = curr_total - prev_total;
    unsigned long long delta_idle  = curr_idle_time - prev_idle_time;

    if (delta_total == 0) return 0.0;
    return (double)(delta_total - delta_idle) * 100.0 / delta_total;
}

typedef struct {
    unsigned long mem_total;
    unsigned long mem_available;
} mem_stats_t;

void read_mem_stats(mem_stats_t *stats) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen /proc/meminfo");
        exit(EXIT_FAILURE);
    }
    char line[256];
    stats->mem_total = 0;
    stats->mem_available = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &stats->mem_total) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", &stats->mem_available) == 1) continue;
    }
    fclose(fp);
}

void read_loadavg(double *load1, double *load2, double *load3, int *running_procs, int *total_procs) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        perror("fopen /proc/loadavg");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%lf %lf %lf %d/%d", load1, load2, load3, running_procs, total_procs);
    fclose(fp);
}

int main(void) {
    signal(SIGINT, sigint_handler);

    cpu_stats_t prev_cpu, curr_cpu;
    read_cpu_stats(&prev_cpu);

    printf("Linux System Resource Monitor \n\n");
    printf("Monitoring every 1 second. Press Ctrl+C to exit.\n\n");

    while (running) {
        sleep(1);
        read_cpu_stats(&curr_cpu);

        double cpu_pct = get_cpu_percent(&prev_cpu, &curr_cpu);
        prev_cpu = curr_cpu;  /* struct copy for next iteration */

        mem_stats_t mem;
        read_mem_stats(&mem);
        double mem_pct = (mem.mem_total > 0) ?
                         (mem.mem_total - mem.mem_available) * 100.0 / mem.mem_total : 0.0;

        double load1, load2, load3;
        int running_procs, total_procs;
        read_loadavg(&load1, &load2, &load3, &running_procs, &total_procs);

        /* Clear screen and print fresh dashboard */
        printf("\033[2J\033[H"); 

        printf("=== Linux System Resource Monitor (procfs) ===\n");
        printf("CPU Usage      : %.2f%%\n", cpu_pct);
        printf("Memory Usage   : %.2f%%  (Used: %.2f GB / Total: %.2f GB)\n",
               mem_pct,
               (mem.mem_total - mem.mem_available) / 1024.0 / 1024.0,
               mem.mem_total / 1024.0 / 1024.0);
        printf("Load Average   : %.2f  %.2f  %.2f\n", load1, load2, load3);
        printf("Processes      : %d running / %d total\n", running_procs, total_procs);
        printf("────────────────────────────────────────────────\n");
        printf("Monitoring via /proc/stat, /proc/meminfo, /proc/loadavg\n");
        printf("Press Ctrl+C to exit...\n");
    }

    printf("\n\nExiting resource monitor. Goodbye!\n");
    return EXIT_SUCCESS;
}
