/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

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

#if (defined __GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 28)
#define STATX_SUPPORTED
#endif

struct xtask {
    pid_t pid;             /* The process ID */
    pid_t ppid;            /* The PID of the parent */
    char state;            /* State of the process, one character from "RSDZTW" */
    char uname[64];        /* The user name */
    char name[128];        /* Name of the process */
    char path[512];        /* Full path of the executeable */
    char cmdline[1024];    /* The cmdline of the process */
    time_t starttime;      /* Starttime */
    size_t number_threads; /* Number of threads */
#ifdef STATX_SUPPORTED
    struct statx statxbuf; /* Status of the executeable */
#endif
    struct stat executable_stat; /* Status of the executeable */
};

#define KERNEL_VERSION(kernel, major, minor) (((kernel) << 16) | ((major) << 8) | (minor))
static int get_kernel_version()
{
    static int version = 0;
    if (version == 0) {
        struct utsname buffer = {0};
        if (uname(&buffer) == 0) {
            // printf("sysname: %s, nodename: %s, release: %s, version: %s, machine: %s\n", buffer.sysname,
            //        buffer.nodename, buffer.release, buffer.version, buffer.machine);
            int i = 0;
            int ver[16];
            char *p = buffer.release;
            while (*p) {
                if (isdigit(*p)) {
                    ver[i] = strtol(p, &p, 10);
                    i++;
                } else {
                    p++;
                }
            }
            version = KERNEL_VERSION(ver[0], ver[1], ver[2]);
        }
    }

    return version;
}

int get_process_details(int pid, struct xtask *task)
{
    memset(task, 0, sizeof(struct xtask));
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
            char spcifiers[512] = {0}, *p = spcifiers;
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
            p = strcat(p, "%d %s %c %d %*d %*d %*d %*d %*lu");
            p = strcat(p, " %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %ld %*ld %llu");
            p = strcat(p, " %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu");
            do {
                int version = get_kernel_version();
                if (version < KERNEL_VERSION(2, 1, 22)) {
                    break;
                }
                p = strcat(p, " %*d");
                if (version < KERNEL_VERSION(2, 2, 8)) {
                    break;
                }
                p = strcat(p, " %*d");
                if (version < KERNEL_VERSION(2, 5, 19)) {
                    break;
                }
                p = strcat(p, " %*lu %*lu");
                if (version < KERNEL_VERSION(2, 6, 18)) {
                    break;
                }
                p = strcat(p, " %*llu");
                if (version < KERNEL_VERSION(2, 6, 24)) {
                    break;
                }
                p = strcat(p, " %*lu %*ld");
                if (version < KERNEL_VERSION(3, 3, 0)) {
                    break;
                }
                p = strcat(p, " %*lu %*lu %*lu");
                if (version < KERNEL_VERSION(3, 5, 0)) {
                    break;
                }
                p = strcat(p, " %*lu %*lu %*lu %*lu %*d");
            } while (0);
            // printf("format: %s\nstat: %s\n", spcifiers, proc_stat);

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
            task->starttime = starttime / sysconf(_SC_CLK_TCK);
            task->number_threads = number_threads;

            struct sysinfo sys_info;
            if (sysinfo(&sys_info) == 0) {
                // task->starttime += sys_info.uptime;
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
        int size = read(fd, task->cmdline, sizeof(task->cmdline));
        for (int i = 0; i < size - 1; i++) {
            if (!task->cmdline[i]) {
                task->cmdline[i] = ' ';
            }
        }
        task->cmdline[size - 1] = '\0';
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

#ifdef STATX_SUPPORTED
static void print_time(const char *field, struct statx_timestamp *ts)
{
    struct tm tm;
    time_t tim;
    char buffer[100];
    int len;

    tim = ts->tv_sec;
    if (!localtime_r(&tim, &tm)) {
        perror("localtime_r");
        exit(1);
    }
    len = strftime(buffer, 100, "%F %T", &tm);
    if (len == 0) {
        perror("strftime");
        exit(1);
    }
    printf("%s", field);
    fwrite(buffer, 1, len, stdout);
    printf(".%09u", ts->tv_nsec);
    len = strftime(buffer, 100, " %z", &tm);
    if (len == 0) {
        perror("strftime2");
        exit(1);
    }
    fwrite(buffer, 1, len, stdout);
    printf("\n");
}

static void dump_statx(struct statx *stx)
{
    char buffer[256], ft = '?';

    printf(" ");
    if (stx->stx_mask & STATX_SIZE) printf(" Size: %-15llu", (unsigned long long)stx->stx_size);
    if (stx->stx_mask & STATX_BLOCKS) printf(" Blocks: %-10llu", (unsigned long long)stx->stx_blocks);
    printf(" IO Block: %-6llu", (unsigned long long)stx->stx_blksize);
    if (stx->stx_mask & STATX_TYPE) {
        switch (stx->stx_mode & S_IFMT) {
            case S_IFIFO:
                printf("  FIFO\n");
                ft = 'p';
                break;
            case S_IFCHR:
                printf("  character special file\n");
                ft = 'c';
                break;
            case S_IFDIR:
                printf("  directory\n");
                ft = 'd';
                break;
            case S_IFBLK:
                printf("  block special file\n");
                ft = 'b';
                break;
            case S_IFREG:
                printf("  regular file\n");
                ft = '-';
                break;
            case S_IFLNK:
                printf("  symbolic link\n");
                ft = 'l';
                break;
            case S_IFSOCK:
                printf("  socket\n");
                ft = 's';
                break;
            default:
                printf(" unknown type (%o)\n", stx->stx_mode & S_IFMT);
                break;
        }
    } else {
        printf(" no type\n");
    }

    sprintf(buffer, "%02x:%02x", stx->stx_dev_major, stx->stx_dev_minor);
    printf("Device: %-15s", buffer);
    if (stx->stx_mask & STATX_INO) printf(" Inode: %-11llu", (unsigned long long)stx->stx_ino);
    if (stx->stx_mask & STATX_NLINK) printf(" Links: %-5u", stx->stx_nlink);
    if (stx->stx_mask & STATX_TYPE) {
        switch (stx->stx_mode & S_IFMT) {
            case S_IFBLK:
            case S_IFCHR:
                printf(" Device type: %u,%u", stx->stx_rdev_major, stx->stx_rdev_minor);
                break;
        }
    }
    printf("\n");

    if (stx->stx_mask & STATX_MODE)
        printf("Access: (%04o/%c%c%c%c%c%c%c%c%c%c)  ", stx->stx_mode & 07777, ft, stx->stx_mode & S_IRUSR ? 'r' : '-',
               stx->stx_mode & S_IWUSR ? 'w' : '-', stx->stx_mode & S_IXUSR ? 'x' : '-',
               stx->stx_mode & S_IRGRP ? 'r' : '-', stx->stx_mode & S_IWGRP ? 'w' : '-',
               stx->stx_mode & S_IXGRP ? 'x' : '-', stx->stx_mode & S_IROTH ? 'r' : '-',
               stx->stx_mode & S_IWOTH ? 'w' : '-', stx->stx_mode & S_IXOTH ? 'x' : '-');
    if (stx->stx_mask & STATX_UID) printf("Uid: %5d   ", stx->stx_uid);
    if (stx->stx_mask & STATX_GID) printf("Gid: %5d\n", stx->stx_gid);

    if (stx->stx_mask & STATX_ATIME) print_time("Access: ", &stx->stx_atime);
    if (stx->stx_mask & STATX_MTIME) print_time("Modify: ", &stx->stx_mtime);
    if (stx->stx_mask & STATX_CTIME) print_time("Change: ", &stx->stx_ctime);
    if (stx->stx_mask & STATX_BTIME) print_time(" Birth: ", &stx->stx_btime);

    if (stx->stx_attributes_mask) {
        unsigned char bits, mbits;
        int loop, byte;

        static char attr_representation[64 + 1] =
            /* STATX_ATTR_ flags: */
            "????????" /* 63-56 */
            "????????" /* 55-48 */
            "????????" /* 47-40 */
            "????????" /* 39-32 */
            "????????" /* 31-24	0x00000000-ff000000 */
            "????????" /* 23-16	0x00000000-00ff0000 */
            "???me???" /* 15- 8	0x00000000-0000ff00 */
            "?dai?c??" /*  7- 0	0x00000000-000000ff */
            ;

        printf("Attributes: %016llx (", stx->stx_attributes);
        for (byte = 64 - 8; byte >= 0; byte -= 8) {
            bits = stx->stx_attributes >> byte;
            mbits = stx->stx_attributes_mask >> byte;
            for (loop = 7; loop >= 0; loop--) {
                int bit = byte + loop;

                if (!(mbits & 0x80))
                    putchar('.'); /* Not supported */
                else if (bits & 0x80)
                    putchar(attr_representation[63 - bit]);
                else
                    putchar('-'); /* Not set */
                bits <<= 1;
                mbits <<= 1;
            }
            if (byte) putchar(' ');
        }
        printf(")\n");
    }
}
#endif

int main(int argc, char **argv)
{
    struct xtask task;
    pid_t pid = getpid();

    if (get_process_details(pid, &task) == 0) {
        char buff[20];
        printf("pid: %d, ppid: %d, state: %c, uname: %s, name: %s, cmdline: \"%s\", path: \"%s\", "
               "threads: %ld, starttime: %lu\n",
               task.pid, task.ppid, task.state, task.uname, task.name, task.cmdline, task.path, task.number_threads,
               task.starttime);

        {
            char buff[20];
            time_t now = time(NULL);
            strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
            printf("now: %s\n\n", buff);
        }

#ifdef STATX_SUPPORTED
        printf("  File: %s\n", task.name);
        dump_statx(&task.statxbuf);
#endif
    }

    return 0;
}
