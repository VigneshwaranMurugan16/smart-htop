// Compile with: gcc main.c -o procmon -lncurses
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <ncurses.h>

#define MAX_PROCS 1024
#define TOP_N 5

typedef struct {
    int pid;
    char name[256];
    unsigned long utime;
    unsigned long stime;
    unsigned long total_time;  // utime + stime
    double cpu_usage;          // percentage CPU usage
} ProcInfo;

typedef struct {
    int pid;
    unsigned long total_time;
} ProcPrev;

ProcPrev prev_procs[MAX_PROCS];
int prev_count = 0;

int refresh_delay = 1; // seconds
int paused = 0;

unsigned long get_total_cpu_time() {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;
    char buf[512];
    fgets(buf, sizeof(buf), fp);  // read first line: cpu ...
    fclose(fp);

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(buf, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    return user + nice + system + idle + iowait + irq + softirq + steal;
}

void get_uptime(WINDOW *win) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) {
        wprintw(win, "Error reading /proc/uptime\n");
        return;
    }
    double uptime;
    fscanf(fp, "%lf", &uptime);
    fclose(fp);
    int h = (int)uptime / 3600;
    int m = ((int)uptime % 3600) / 60;
    int s = (int)uptime % 60;
    wprintw(win, "Uptime: %02d:%02d:%02d\n", h, m, s);
}

void get_loadavg(WINDOW *win) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        wprintw(win, "Error reading /proc/loadavg\n");
        return;
    }
    double l1, l5, l15;
    fscanf(fp, "%lf %lf %lf", &l1, &l5, &l15);
    fclose(fp);
    wprintw(win, "Load Average (1,5,15 min): %.2f %.2f %.2f\n", l1, l5, l15);
}

void get_meminfo(WINDOW *win) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        wprintw(win, "Error reading /proc/meminfo\n");
        return;
    }
    char label[256];
    long memTotal = 0, memFree = 0, buffers = 0, cached = 0;
    for(int i=0; i<4; i++) {
        fscanf(fp, "%s %ld", label, &memTotal);
        if (strcmp(label, "MemTotal:") == 0) memTotal = memTotal;
        else if (strcmp(label, "MemFree:") == 0) memFree = memTotal;
        else if (strcmp(label, "Buffers:") == 0) buffers = memTotal;
        else if (strcmp(label, "Cached:") == 0) cached = memTotal;
    }
    fclose(fp);
    long used = memTotal - memFree - buffers - cached;
    double usage_percent = (used * 100.0) / memTotal;
    wprintw(win, "Memory Usage: %.2f%%\n", usage_percent);
}

void get_cpuinfo(WINDOW *win) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        wprintw(win, "Error reading /proc/cpuinfo\n");
        return;
    }
    char line[256];
    char model_name[256] = "";
    int cores = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "model name", 10) == 0 && model_name[0] == '\0') {
            char *colon = strchr(line, ':');
            if (colon) {
                strcpy(model_name, colon + 2);
                model_name[strcspn(model_name, "\n")] = '\0';
            }
        }
        if (strncmp(line, "processor", 9) == 0) {
            cores++;
        }
    }
    fclose(fp);
    wprintw(win, "CPU: %s\n", model_name);
    wprintw(win, "Cores: %d\n", cores);
}

int compare_cpu_usage(const void *a, const void *b) {
    ProcInfo *p1 = (ProcInfo *)a;
    ProcInfo *p2 = (ProcInfo *)b;
    if (p2->cpu_usage > p1->cpu_usage) return 1;
    else if (p2->cpu_usage < p1->cpu_usage) return -1;
    else return 0;
}

// Find previous CPU time for pid, or 0 if not found
unsigned long find_prev_total_time(int pid) {
    for (int i = 0; i < prev_count; i++) {
        if (prev_procs[i].pid == pid) return prev_procs[i].total_time;
    }
    return 0;
}

// Update or add previous CPU time for pid
void update_prev_total_time(int pid, unsigned long total_time) {
    for (int i = 0; i < prev_count; i++) {
        if (prev_procs[i].pid == pid) {
            prev_procs[i].total_time = total_time;
            return;
        }
    }
    if (prev_count < MAX_PROCS) {
        prev_procs[prev_count].pid = pid;
        prev_procs[prev_count].total_time = total_time;
        prev_count++;
    }
}

// Get process info from /proc/[pid]
int get_proc_info(ProcInfo *pinfo, int pid) {
    char path[256], buf[1024];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) return 0;

    // According to proc man page, fields:
    // pid (1), comm (2), state (3), ppid (4), pgrp (5), session (6), tty_nr (7), tpgid (8),
    // flags (9), minflt (10), cminflt (11), majflt (12), cmajflt (13),
    // utime (14), stime (15), cutime (16), cstime (17), priority (18), nice (19), etc.

    // We want pid, comm (process name), utime, stime
    // comm is inside parentheses and may contain spaces, so read carefully

    // Let's read entire line first
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    char *start = strchr(buf, '(');
    char *end = strrchr(buf, ')');
    if (!start || !end) return 0;

    *end = '\0'; // terminate comm string
    strcpy(pinfo->name, start + 1);

    // Extract utime and stime by sscanf after end
    // utime is 14th field, stime 15th field overall

    // Count fields before utime
    // fields 1 = pid, 2=comm, then fields from 3 onwards after ')'
    char *after = end + 2;
    unsigned long utime = 0, stime = 0;
    int scanned = sscanf(after,
        "%*c %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
        &utime, &stime);
    if (scanned != 2) return 0;

    pinfo->pid = pid;
    pinfo->utime = utime;
    pinfo->stime = stime;
    pinfo->total_time = utime + stime;

    return 1;
}

void get_top_processes(WINDOW *win, unsigned long total_cpu_diff) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        wprintw(win, "Failed to open /proc\n");
        return;
    }

    ProcInfo proc_list[MAX_PROCS];
    int proc_count = 0;

    struct dirent *entry;
    while ((entry = readdir(proc))) {
        if (!isdigit(entry->d_name[0])) continue; // skip non-pids
        int pid = atoi(entry->d_name);
        ProcInfo pinfo;

        if (get_proc_info(&pinfo, pid)) {
            // Calculate CPU usage since last update
            unsigned long prev_time = find_prev_total_time(pid);
            unsigned long diff_time = 0;
            if (pinfo.total_time > prev_time)
                diff_time = pinfo.total_time - prev_time;

            if (total_cpu_diff > 0)
                pinfo.cpu_usage = (diff_time * 100.0) / total_cpu_diff;
            else
                pinfo.cpu_usage = 0;

            proc_list[proc_count++] = pinfo;

            update_prev_total_time(pid, pinfo.total_time);

            if (proc_count >= MAX_PROCS) break;
        }
    }
    closedir(proc);

    // Sort by cpu_usage descending
    qsort(proc_list, proc_count, sizeof(ProcInfo), compare_cpu_usage);

    wprintw(win, "\nTop %d processes by CPU usage:\n", TOP_N);
    wprintw(win, "PID\tCPU%%\tName\n");
    for (int i = 0; i < proc_count && i < TOP_N; i++) {
        wprintw(win, "%d\t%.2f\t%s\n", proc_list[i].pid, proc_list[i].cpu_usage, proc_list[i].name);
    }
}

int main() {
    initscr();      // ncurses start
    cbreak();       // disable line buffering
    noecho();       // don't echo pressed keys
    nodelay(stdscr, TRUE); // non-blocking getch()
    keypad(stdscr, TRUE);  // enable arrow keys

    unsigned long prev_total_cpu = get_total_cpu_time();

    while (1) {
        clear();

        time_t now = time(NULL);
        mvprintw(0, 0, "📟 Smart Process Monitor  (Press 'q' to quit, 'p' pause/resume, '+'/'-' speed)  %s", ctime(&now));

        get_uptime(stdscr);
        get_loadavg(stdscr);
        get_meminfo(stdscr);
        get_cpuinfo(stdscr);

        unsigned long current_total_cpu = get_total_cpu_time();
        unsigned long total_cpu_diff = current_total_cpu - prev_total_cpu;
        prev_total_cpu = current_total_cpu;

        if (!paused) {
            get_top_processes(stdscr, total_cpu_diff);
        } else {
            mvprintw(LINES - 2, 0, "Paused");
        }

        refresh();

        int ch = getch();
        if (ch != ERR) {
            if (ch == 'q') break;
            else if (ch == 'p') paused = !paused;
            else if (ch == '+') {
                if (refresh_delay > 1) refresh_delay--;
            }
            else if (ch == '-') {
                if (refresh_delay < 10) refresh_delay++;
            }
        }

        sleep(refresh_delay);
    }

    endwin(); // End ncurses
    return 0;
}
