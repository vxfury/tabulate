#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <sstream>

#if (defined __GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 28)
#define STATX_SUPPORTED
#endif

struct ProcessDetails {
    pid_t pid;                    /* The process ID */
    pid_t ppid;                   /* The PID of the parent */
    char state;                   /* State of the process, one character from "RSDZTW" */
    char uname[64];               /* The user name */
    char name[128];               /* Name of the process */
    char path[512];               /* Full path of the executeable */
    std::string cmdline;          /* The cmdline of the process */
    unsigned long long starttime; /* Starttime, in clock ticks */
    size_t number_threads;        /* Number of threads */
#ifdef STATX_SUPPORTED
    struct statx statxbuf; /* Status of the executeable */
#endif
    struct stat executable_stat; /* Status of the executeable */
};

int GetProcessDetails(int pid, struct ProcessDetails *task)
{
    char filename[256];
    sprintf(filename, "/proc/%i/stat", pid);
    if (stat(filename, &task->executable_stat) == 0) {
        struct passwd *passwdp = getpwuid(task->executable_stat.st_uid);
        if (passwdp != NULL && passwdp->pw_name != NULL) {
            strncpy(task->uname, passwdp->pw_name, sizeof(task->uname));
            task->uname[sizeof(task->uname) - 1] = '\0';
        }
    } else {
        printf("stat(%s) failed: %d(%s)\n", filename, errno, strerror(errno));
        return errno;
    }

    int fd;
    if ((fd = open(filename, O_RDONLY)) >= 0) {
        int size;
        char proc_stat[512];
        if ((size = read(fd, proc_stat, sizeof(proc_stat))) > 0) {
            /**
              Status information about the process.   This  is  used  by  ps(1).   It  is  defined  in
              /usr/src/linux/fs/proc/array.c.

              The fields, in order, with their proper scanf(3) format specifiers, are:

              pid %d      (1) The process ID.

              comm %s     (2) The filename of the executable, in parentheses.  This is visible whether
                          or not the executable is swapped out.

              state %c    (3) One character from the string "RSDZTW" where R is running, S is sleeping
                          in  an  interruptible wait, D is waiting in uninterruptible disk sleep, Z is
                          zombie, T is traced or stopped (on a signal), and W is paging.

              ppid %d     (4) The PID of the parent.

              pgrp %d     (5) The process group ID of the process.

              session %d  (6) The session ID of the process.

              tty_nr %d   (7) The controlling terminal of the process.  (The minor  device  number  is
                          contained  in  the combination of bits 31 to 20 and 7 to 0; the major device
                          number is in bits 15 to 8.)

              tpgid %d    (8) The ID of the foreground process group of the  controlling  terminal  of
                          the process.

              flags %u (%lu before Linux 2.6.22)
                          (9)  The  kernel  flags word of the process.  For bit meanings, see the PF_*
                          defines in the Linux  kernel  source  file  include/linux/sched.h.   Details
                          depend on the kernel version.

              minflt %lu  (10) The number of minor faults the process has made which have not required
                          loading a memory page from disk.

              cminflt %lu (11) The number of minor faults that the process's waited-for children  have
                          made.

              majflt %lu  (12)  The  number  of  major faults the process has made which have required
                          loading a memory page from disk.

              cmajflt %lu (13) The number of major faults that the process's waited-for children  have
                          made.

              utime %lu   (14)  Amount of time that this process has been scheduled in user mode, mea‐
                          sured in clock ticks (divide by sysconf(_SC_CLK_TCK)).  This includes  guest
                          time,  guest_time  (time  spent  running  a virtual CPU, see below), so that
                          applications that are not aware of the guest time field  do  not  lose  that
                          time from their calculations.

              stime %lu   (15)  Amount  of  time  that this process has been scheduled in kernel mode,
                          measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).

              cutime %ld  (16) Amount of time that this process's waited-for children have been sched‐
                          uled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
                          (See also times(2).)  This includes guest time, cguest_time (time spent run‐
                          ning a virtual CPU, see below).

              cstime %ld  (17) Amount of time that this process's waited-for children have been sched‐
                          uled   in   kernel   mode,   measured   in   clock    ticks    (divide    by
                          sysconf(_SC_CLK_TCK)).

              priority %ld
                          (18)  (Explanation for Linux 2.6) For processes running a real-time schedul‐
                          ing policy (policy below; see sched_setscheduler(2)), this  is  the  negated
                          scheduling  priority,  minus one; that is, a number in the range -2 to -100,
                          corresponding to real-time priorities 1 to 99.  For processes running  under
                          a  non-real-time  scheduling  policy,  this is the raw nice value (setprior‐
                          ity(2)) as represented in the kernel.  The kernel stores nice values as num‐
                          bers  in  the  range 0 (high) to 39 (low), corresponding to the user-visible
                          nice range of -20 to 19.

                          Before Linux 2.6, this was a scaled value based on the  scheduler  weighting
                          given to this process.

              nice %ld    (19)  The nice value (see setpriority(2)), a value in the range 19 (low pri‐
                          ority) to -20 (high priority).

              num_threads %ld
                          (20) Number of threads in this process (since  Linux  2.6).   Before  kernel
                          2.6,  this field was hard coded to 0 as a placeholder for an earlier removed
                          field.

              itrealvalue %ld
                          (21) The time in jiffies before the next SIGALRM is sent to the process  due
                          to  an  interval  timer.  Since kernel 2.6.17, this field is no longer main‐
                          tained, and is hard coded as 0.

              starttime %llu (was %lu before Linux 2.6)
                          (22) The time the process started after  system  boot.   In  kernels  before
                          Linux  2.6, this value was expressed in jiffies.  Since Linux 2.6, the value
                          is expressed in clock ticks (divide by sysconf(_SC_CLK_TCK)).

              vsize %lu   (23) Virtual memory size in bytes.

              rss %ld     (24) Resident Set Size: number of pages the  process  has  in  real  memory.
                          This  is just the pages which count toward text, data, or stack space.  This
                          does not include pages which have not been demand-loaded in,  or  which  are
                          swapped out.

              rsslim %lu  (25) Current soft limit in bytes on the rss of the process; see the descrip‐
                          tion of RLIMIT_RSS in getrlimit(2).

              startcode %lu
                          (26) The address above which program text can run.

              endcode %lu (27) The address below which program text can run.

              startstack %lu
                          (28) The address of the start (i.e., bottom) of the stack.

              kstkesp %lu (29) The current value of ESP (stack pointer), as found in the kernel  stack
                          page for the process.

              kstkeip %lu (30) The current EIP (instruction pointer).

              signal %lu  (31)  The  bitmap  of pending signals, displayed as a decimal number.  Obso‐
                          lete, because it does not provide  information  on  real-time  signals;  use
                          /proc/[pid]/status instead.

              blocked %lu (32)  The  bitmap  of blocked signals, displayed as a decimal number.  Obso‐
                          lete, because it does not provide  information  on  real-time  signals;  use
                          /proc/[pid]/status instead.

              sigignore %lu
                          (33)  The  bitmap  of ignored signals, displayed as a decimal number.  Obso‐
                          lete, because it does not provide  information  on  real-time  signals;  use
                          /proc/[pid]/status instead.

              sigcatch %lu
                          (34) The bitmap of caught signals, displayed as a decimal number.  Obsolete,
                          because  it  does  not  provide  information  on  real-time   signals;   use
                          /proc/[pid]/status instead.

              wchan %lu   (35)  This  is  the  "channel"  in  which the process is waiting.  It is the
                          address of a system call, and can be looked up in a namelist if you  need  a
                          textual name.  (If you have an up-to-date /etc/psdatabase, then try ps -l to
                          see the WCHAN field in action.)

              nswap %lu   (36) Number of pages swapped (not maintained).

              cnswap %lu  (37) Cumulative nswap for child processes (not maintained).

              exit_signal %d (since Linux 2.1.22)
                          (38) Signal to be sent to parent when we die.

              processor %d (since Linux 2.2.8)
                          (39) CPU number last executed on.

              rt_priority %u (since Linux 2.5.19; was %lu before Linux 2.6.22)
                          (40) Real-time scheduling priority, a number in the range 1 to 99  for  pro‐
                          cesses scheduled under a real-time policy, or 0, for non-real-time processes
                          (see sched_setscheduler(2)).

              policy %u (since Linux 2.5.19; was %lu before Linux 2.6.22)
                          (41)  Scheduling  policy  (see  sched_setscheduler(2)).   Decode  using  the
                          SCHED_* constants in linux/sched.h.

              delayacct_blkio_ticks %llu (since Linux 2.6.18)
                          (42) Aggregated block I/O delays, measured in clock ticks (centiseconds).

              guest_time %lu (since Linux 2.6.24)
                          (43) Guest time of the process (time spent running a virtual CPU for a guest
                          operating system), measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).

              cguest_time %ld (since Linux 2.6.24)
                          (44) Guest time of the process's children, measured in clock  ticks  (divide
                          by sysconf(_SC_CLK_TCK)).

              start_data %lu (since Linux 3.3)
                          (45) Address above which program data+bss is placed.

              end_data %lu (since Linux 3.3)
                          (46) Address below which program data+bss is placed.

              start_brk %lu (since Linux 3.3)
                          (47) Address above which program heap can be expanded with brk().

              arg_start %lu (since Linux 3.5)
                          (48) Address above which program command line is placed.

              arg_end %lu (since Linux 3.5)
                          (49) Address below which program command line is placed.

              env_start %lu (since Linux 3.5)
                          (50) Address above which program environment is placed.

              env_end %lu (since Linux 3.5)
                          (51) Address below which program environment is placed.

              exit_code %d (since Linux 3.5)
                          (52) The thread's exit_code in the form reported by the waitpid system.
            */
            const char *spcifiers =
                "%d %s %c %d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %ld %*ld %llu";

            long int number_threads = 0;
            unsigned long long starttime = 0;
            sscanf(proc_stat, spcifiers,
                   &task->pid,      // (1) The process ID.
                   &task->name,     // (2) The filename of the executable, in parentheses.
                   &task->state,    // (3) One character from the string "RSDZTW" where R is running, S is
                                    //     sleeping in  an  interruptible wait, D is waiting in uninterruptible
                                    //     disk sleep, Z is zombie, T is traced or stopped (on a signal), and W
                                    //     is paging.
                   &task->ppid,     // (4) The PID of the parent.
                   &number_threads, // (20) Number of threads in this process (since  Linux  2.6)
                   &starttime       // (22) The time the process started after system boot.
            );
            task->starttime = starttime;
            task->number_threads = number_threads;

            struct sysinfo sys_info;
            if (sysinfo(&sys_info) == 0) {
                task->starttime += sys_info.uptime * sysconf(_SC_CLK_TCK);
            } else {
                printf("sysinfo error\n");
            }

            /* remove prefix and suffix */
            if (*task->name == '(') {
                size_t len = strlen(task->name);
                memmove(task->name, task->name + 1, len);
                if (*(task->name + len - 2) == ')') {
                    *(task->name + len - 2) = '\0';
                }
            }
        }

        close(fd);
    } else {
        printf("fopen(%s) failed: %d(%s)\n", filename, errno, strerror(errno));
        return errno;
    }

    sprintf(filename, "/proc/%i/cmdline", pid);
    if ((fd = open(filename, O_RDONLY)) >= 0) {
        char cmdline[1024], *lastarg = cmdline;
        int size = read(fd, cmdline, sizeof(cmdline));
        for (int i = 0; i < size; i++) {
            if (!cmdline[i]) {
                if (lastarg != cmdline) {
                    task->cmdline += " ";
                }
                if (strchr(lastarg, ' ')) {
                    task->cmdline += "\"" + std::string(lastarg) + "\"";
                } else {
                    task->cmdline += lastarg;
                }
                lastarg = &cmdline[i + 1];
            }
        }
        close(fd);
    } else {
        printf("fopen(%s) failed: %d(%s)\n", filename, errno, strerror(errno));
        return errno;
    }

    sprintf(filename, "/proc/%i/exe", pid);
    if (readlink(filename, task->path, sizeof(task->path)) >= 0) {
#ifdef STATX_SUPPORTED
        statx(AT_FDCWD, task->path, AT_SYMLINK_NOFOLLOW, STATX_ALL, &task->statxbuf);
#endif
    }

    if (stat(filename, &task->executable_stat) == 0) {
        struct passwd *passwdp = getpwuid(task->executable_stat.st_uid);
        if (passwdp != NULL && passwdp->pw_name != NULL) {
            strncpy(task->uname, passwdp->pw_name, sizeof(task->uname));
            task->uname[sizeof(task->uname) - 1] = '\0';
        }
    } else {
        printf("stat(%s) failed: %d(%s)\n", filename, errno, strerror(errno));
        return errno;
    }

    return 0;
}

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

#ifdef STATX_SUPPORTED
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
        snprintf(buff, sizeof(buff), "Access: (%04o/%c%c%c%c%c%c%c%c%c)  ", stx->stx_mode & 07777,
                 stx->stx_mode & S_IRUSR ? 'r' : '-', stx->stx_mode & S_IWUSR ? 'w' : '-',
                 stx->stx_mode & S_IXUSR ? 'x' : '-', stx->stx_mode & S_IRGRP ? 'r' : '-',
                 stx->stx_mode & S_IWGRP ? 'w' : '-', stx->stx_mode & S_IXGRP ? 'x' : '-',
                 stx->stx_mode & S_IROTH ? 'r' : '-', stx->stx_mode & S_IWOTH ? 'w' : '-',
                 stx->stx_mode & S_IXOTH ? 'x' : '-');
        ynode["Access"] = std::string(buff);
    }

    if (stx->stx_mask & STATX_ATIME) {
        ynode["Access"] = FromTime(&stx->stx_atime);
    }
    if (stx->stx_mask & STATX_MTIME) {
        ynode["Modify"] = FromTime(&stx->stx_mtime);
    }
    if (stx->stx_mask & STATX_CTIME) {
        ynode["Change"] = FromTime(&stx->stx_ctime);
    }
    if (stx->stx_mask & STATX_BTIME) {
        ynode["Birth"] = FromTime(&stx->stx_btime);
    }

    return ynode;
}
#endif

int FillDetailsAboutPID(pid_t pid, YAML::Node &ynode, const std::string label)
{
    if (pid <= 0) {
        return -EINVAL;
    }
    struct ProcessDetails task;
    if (GetProcessDetails(pid, &task) == 0) {
        YAML::Node details;
        details["pid"] = task.pid;
        details["ppid"] = task.ppid;
        details["state"] = DescriptionOfStat(task.state);
        details["uname"] = task.uname;
        details["name"] = task.name;
        details["path"] = task.path;
        details["cmdline"] = task.cmdline;
        details["number_threads"] = (int)(task.number_threads);
        details["starttime"] = FromTimestamp(task.starttime / sysconf(_SC_CLK_TCK));

        if (auto pos = task.cmdline.find("-i"); pos != std::string::npos) {
            const char *p = task.cmdline.c_str();
            const char *e = p + strlen("-i");
            while (*e || *e == ' ') {
                e++; // skip spaces
            }
            p = e;
            char c = *e == '\"' ? '\"' : ' ';
            while (*e && *e != c) {
                e++;
            }

            std::string config;
            std::ifstream ifs(std::string(p, e - p + 1).c_str());
            if (ifs.good()) {
                ifs.seekg(0, ifs.end);
                int length = ifs.tellg();
                ifs.seekg(0, ifs.beg);
                if (length > 0) {
                    config.resize(length);
                    ifs.read((char *)config.data(), length);
                }
                ifs.close();
            }

            details["conf"] = config;
        }

#ifdef STATX_SUPPORTED
        details["ExcutableStatx"] = DumpStatx(&task.statxbuf);
#endif

        ynode[label] = details;

        return 0;
    }

    return -1;
}

std::string ToString(const YAML::Node &ynode)
{
    std::stringstream ss;
    ss << ynode;
    return ss.str();
}

int main(int argc, char **argv)
{
    YAML::Node details;
    FillDetailsAboutPID(getpid(), details, "AboutThis");

    std::cout << ToString(details) << std::endl;

    return 0;
}
