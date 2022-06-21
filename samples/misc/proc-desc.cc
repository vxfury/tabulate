#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <yaml-cpp/yaml.h>
#include <mutex>
#include <fstream>
#include <iostream>
#include <sstream>

static time_t GetBootTime()
{
    static time_t btime = 0;
    if (btime == 0) {
        std::ifstream ifs("/proc/stat");
        if (ifs.good()) {
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.compare(0, strlen("btime"), "btime") == 0) {
                    btime = atol(line.c_str() + strlen("btime"));
                    break;
                }
            }
            ifs.close();
        }
    }

    return btime;
}

static std::string FromTimestamp(time_t t)
{
    char s[32];
    struct tm tm;
    localtime_r(&t, &tm);
    if (strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm) != 0) {
        return std::string(s);
    }

    return "";
};

#if (defined __GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 28)
#define STATX_SUPPORTED
static std::string FromTime(const struct statx_timestamp *ts)
{
    struct tm tm;
    char buffer[100] = {0}, *p = buffer;

    time_t tim = ts->tv_sec;
    if (!localtime_r(&tim, &tm)) {
        return "<Error>";
    }
    if (int len = strftime(p, sizeof(buffer) - (p - buffer), "%F %T", &tm); len == 0) {
        return "<Error>";
    } else {
        p += len;
    }
    snprintf(p, sizeof(buffer) - (p - buffer), ".%09u", ts->tv_nsec);

    return buffer;
}

static YAML::Node DumpStatx(const struct statx *stx)
{
    YAML::Node ynode;
    if (stx->stx_mask & STATX_MODE) {
        char buff[64];
        snprintf(buff, sizeof(buff), "%04o/%c%c%c%c%c%c%c%c%c", stx->stx_mode & 07777,
                 stx->stx_mode & S_IRUSR ? 'r' : '-', stx->stx_mode & S_IWUSR ? 'w' : '-',
                 stx->stx_mode & S_IXUSR ? 'x' : '-', stx->stx_mode & S_IRGRP ? 'r' : '-',
                 stx->stx_mode & S_IWGRP ? 'w' : '-', stx->stx_mode & S_IXGRP ? 'x' : '-',
                 stx->stx_mode & S_IROTH ? 'r' : '-', stx->stx_mode & S_IWOTH ? 'w' : '-',
                 stx->stx_mode & S_IXOTH ? 'x' : '-');
        ynode["Access"] = std::string(buff);
    }

    if (stx->stx_mask & STATX_ATIME) {
        ynode["AccessTime"] = FromTime(&stx->stx_atime);
    }
    if (stx->stx_mask & STATX_MTIME) {
        ynode["ModifyTime"] = FromTime(&stx->stx_mtime);
    }
    if (stx->stx_mask & STATX_CTIME) {
        ynode["ChangeTime"] = FromTime(&stx->stx_ctime);
    }
    if (stx->stx_mask & STATX_BTIME) {
        ynode["BirthTime"] = FromTime(&stx->stx_btime);
    }

    return ynode;
}
#endif

// https://linux.die.net/man/1/ps
static std::string DescriptionOfStat(char st)
{
    switch (st) {
        case 'D':
            return "Uninterruptible sleep (usually IO)";
        case 'R':
            return "Running or runnable (on run queue)";
        case 'S':
            return "Interruptible sleep (waiting for an event to complete)";
        case 'T':
            return "Stopped, either by a job control signal or because it is being traced.";
        case 'W':
            return "Paging (not valid since the 2.6.xx kernel)";
        case 'X':
            return "Dead (should never be seen)";
        case 'Z':
            return "Defunct (\"zombie\") process, terminated but not reaped by its parent.";
        case '<':
            return "high-priority (not nice to other users)";
        case 'N':
            return "low-priority (nice to other users)";
        case 'L':
            return "has pages locked into memory (for real-time and custom IO)";
        case 's':
            return "is a session leader";
        case 'l':
            return "is multi-threaded (using CLONE_THREAD, like NPTL pthreads do)";
        case '+':
            return "is in the foreground process group";
    }
    return "Unkown<" + std::string(1, st) + ">";
}

int FillDetailsAboutPID(pid_t pid, YAML::Node &ynode, bool get_conf)
{
    ynode["pid"] = pid;

    char filename[256];
    sprintf(filename, "/proc/%i/exe", pid);
    struct stat executable_stat;
    if (stat(filename, &executable_stat) == 0) {
        struct passwd *passwdp = getpwuid(executable_stat.st_uid);
        if (passwdp != NULL && passwdp->pw_name != NULL) {
            ynode["uname"] = passwdp->pw_name;
        }
    } else {
        char errmsg[1024];
        snprintf(errmsg, sizeof(errmsg), "stat(%s) failed: %d(%s)\n", filename, errno, strerror(errno));
        ynode["error"] = errmsg;
        return errno;
    }

    int fd;
    sprintf(filename, "/proc/%i/stat", pid);
    if ((fd = open(filename, O_RDONLY)) >= 0) {
        int size;
        char proc_stat[512] = {0};
        if ((size = read(fd, proc_stat, sizeof(proc_stat))) > 0) {
            const char *spcifiers =
                "%*d %s %c %d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %ld %*ld %llu";

            pid_t ppid;
            char state, name[128] = {0};
            long int number_threads = 0;
            unsigned long long starttime = 0;
            sscanf(proc_stat, spcifiers,
                   name,            // (2) The filename of the executable, in parentheses.
                   &state,          // (3) One character from the string "RSDZTW" where R is running, S is
                                    //     sleeping in  an  interruptible wait, D is waiting in uninterruptible
                                    //     disk sleep, Z is zombie, T is traced or stopped (on a signal), and W
                                    //     is paging.
                   &ppid,           // (4) The PID of the parent.
                   &number_threads, // (20) Number of threads in this process (since  Linux  2.6)
                   &starttime       // (22) The time the process started after system boot.
            );
            ynode["ppid"] = ppid;
            ynode["state"] = DescriptionOfStat(state);
            ynode["number_threads"] = (int)(number_threads);
            ynode["starttime"] = FromTimestamp(GetBootTime() + (starttime / sysconf(_SC_CLK_TCK)));

            /* remove prefix and suffix */
            if (*name == '(') {
                size_t len = strlen(name);
                memmove(name, name + 1, len);
                if (*(name + len - 2) == ')') {
                    *(name + len - 2) = '\0';
                }
            }
            ynode["name"] = name;
        }

        close(fd);
    } else {
        char errmsg[1024];
        snprintf(errmsg, sizeof(errmsg), "fopen(%s) failed: %d(%s)", filename, errno, strerror(errno));
        ynode["error"] = errmsg;
        return errno;
    }

    sprintf(filename, "/proc/%i/cmdline", pid);
    if ((fd = open(filename, O_RDONLY)) >= 0) {
        std::string cmd;
        char cmdline[1024], *lastarg = cmdline;
        int size = read(fd, cmdline, sizeof(cmdline));

        auto try_read_conf = [&](const char *arg) {
            if (get_conf && strstr(arg, ".conf") != NULL) {
                std::string config;
                const char *file = strchr(arg, '=');
                if (file == NULL) {
                    file = arg;
                } else {
                    file += 1;
                }
                std::ifstream ifs(file);
                if (ifs.good()) {
                    ifs.seekg(0, ifs.end);
                    int length = ifs.tellg();
                    ifs.seekg(0, ifs.beg);
                    if (length > 0) {
                        config.resize(length);
                        ifs.read((char *)config.data(), length);
                        ynode["conf"][file] = config;
                    } else {
                        ynode["conf"][file] = "<failed to read>";
                    }
                    ifs.close();
                } else {
                    ynode["conf"][file] = "<failed to open>";
                }
            }
        };
        for (int i = 0; i < size; i++) {
            if (!cmdline[i]) {
                if (lastarg != cmdline) {
                    cmd += " ";
                }
                if (strchr(lastarg, ' ')) {
                    cmd += "\"" + std::string(lastarg) + "\"";
                } else {
                    cmd += lastarg;
                }
                try_read_conf(lastarg);
                lastarg = &cmdline[i + 1];
            }
        }
        ynode["cmdline"] = cmd;
        close(fd);
    } else {
        char errmsg[1024];
        snprintf(errmsg, sizeof(errmsg), "fopen(%s) failed: %d(%s)", filename, errno, strerror(errno));
        ynode["error"] = errmsg;
        return errno;
    }

    char path[512] = {0};
    sprintf(filename, "/proc/%i/exe", pid);
    if (readlink(filename, path, sizeof(path)) >= 0) {
        ynode["path"] = path;
#ifdef STATX_SUPPORTED
        struct statx statxbuf;
        statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_ALL, &statxbuf);
        ynode["ExcutableStatx"] = DumpStatx(&statxbuf);
#endif
    }

    return 0;
}

std::string ToString(const YAML::Node &ynode)
{
    std::stringstream ss;
    ss << ynode;
    return ss.str();
}

#if 0
enum {
    TCK_USER = 0,
    TCK_NICE,
    TCK_SYSTEM,
    TCK_IDLE,
    TCK_IOWAIT,
    TCK_IRQ,
    TCK_SOFTIRQ,
    TCK_STEAL,
    TCK_GUEST,
    TCK_GUEST_NICE,
    NUM_TCK_TYPES
};

typedef struct {
    char name[16];
    uint64_t tcks[NUM_TCK_TYPES];
} cpu_tck_t;

uint64_t idle_ticks(cpu_tck_t *stat)
{
    return stat->tcks[TCK_IDLE] + stat->tcks[TCK_IOWAIT];
}

uint64_t total_ticks(cpu_tck_t *stat)
{
    uint64_t total = 0;
    for (int i = 0; i < NUM_TCK_TYPES; i++) total += stat->tcks[i];
    return total;
}

void cpusage(cpu_tck_t *prev, cpu_tck_t *curr)
{
    int nprocs = get_nprocs();
    for (int i = 1; i <= nprocs; i++) {
        uint64_t total = total_ticks(curr + i) - total_ticks(prev + i);
        uint64_t idle = idle_ticks(curr + i) - idle_ticks(prev + i);
        uint64_t active = total - idle;
        printf("%s - load %.1f%% \n", curr[i].name, active * 100.f / total);
    }
    printf("\n");
}

void read_cpustat(cpu_tck_t *cpu_stat)
{
    FILE *stat_fp = fopen("/proc/stat", "r");

    int nprocs = get_nprocs();
    for (int i = 0; i <= nprocs; i++) {
        fscanf(stat_fp, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n", cpu_stat[i].name, &(cpu_stat[i].tcks[TCK_USER]),
               &(cpu_stat[i].tcks[TCK_NICE]), &(cpu_stat[i].tcks[TCK_SYSTEM]), &(cpu_stat[i].tcks[TCK_IDLE]),
               &(cpu_stat[i].tcks[TCK_IOWAIT]), &(cpu_stat[i].tcks[TCK_IRQ]), &(cpu_stat[i].tcks[TCK_SOFTIRQ]),
               &(cpu_stat[i].tcks[TCK_STEAL]), &(cpu_stat[i].tcks[TCK_GUEST]), &(cpu_stat[i].tcks[TCK_GUEST_NICE]));
    }

    fclose(stat_fp);
}

int main(int ac, char **av)
{
    int opt;
    int sample_times = 5;
    int sample_ms = 1000;
    while ((opt = getopt(ac, av, "s:t:")) != -1) {
        switch (opt) {
            case 's':
                sample_times = atoi(optarg);
                break;
            case 't':
                sample_ms = atoi(optarg);
                break;
        }
    }

    uint16_t nprocs = get_nprocs_conf();
    cpu_tck_t *cpu_stat[2];
    // allocate 1 more for the overall 'cpu' item
    cpu_stat[0] = malloc(sizeof(cpu_tck_t) * (nprocs + 1));
    cpu_stat[1] = malloc(sizeof(cpu_tck_t) * (nprocs + 1));

    int curr = 0;
    read_cpustat(cpu_stat[curr]);
    curr ^= 1;
    while (sample_times--) {
        usleep(sample_ms * 1000);
        read_cpustat(cpu_stat[curr]);
        cpusage(cpu_stat[curr ^ 1], cpu_stat[curr]);
        curr ^= 1;
    }

    free(cpu_stat[0]);
    free(cpu_stat[1]);
}
#endif

int main(int argc, char **argv)
{
    YAML::Node details;
    FillDetailsAboutPID(getpid(), details, true);

    std::cout << ToString(details) << std::endl;

    return 0;
}
