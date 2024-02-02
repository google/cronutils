/*
Copyright 2010 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#define _GNU_SOURCE /* asprintf, basename */

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "subprocess.h"
#include "tempdir.h"

char* lock_filename = NULL;
volatile sig_atomic_t timeout_expired = 0;

static void usage(char* prog) {
  fprintf(stderr,
          "Usage: %s [options] command [arg [arg] ...]\n\n"
          "This program prevents concurrent execution of a process\n"
          "by holding an exclusive lock while the subprocess is running.\n"
          "Subsequent attempts to run a process with the same lock,\n"
          "while another process has the lock open, and is still\n"
          "executing, will exit with a failure exit code.\n",
          prog);
  fprintf(stderr,
          "\noptions:\n"
          " -d       send log messages to stderr as well as syslog.\n"
          " -f lock_filename path to use as a lock file\n"
          " -t timeout  time in seconds to wait to acquire the lock\n"
          " -h       this help.\n");
}

static void alarm_handler(int signum);
static void alarm_handler(int signum) {
  (void)signum; /* suppress unused variable warning */
  timeout_expired = 1;
}

int main(int argc, char** argv) {
  char* progname;
  int arg;
  char* command;
  char** command_args;
  int status = 0;
  struct flock fl;
  int fd;
  char buf[BUFSIZ];
  struct sigaction sa, old_sa;
  int debug = 0;
  int timeout = 5;
  char* endptr;

  memset(&fl, 0, sizeof(fl));
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;

  progname = argv[0];

  while ((arg = getopt(argc, argv, "+df:ht:")) > 0) {
    switch (arg) {
      case 'h':
        usage(progname);
        exit(EXIT_SUCCESS);
        break;
      case 'd':
        debug = LOG_PERROR;
        break;
      case 'f':
        if (asprintf(&lock_filename, "%s", optarg) == -1) {
          perror("asprintf");
          exit(EX_OSERR);
        }
        break;
      case 't':
        timeout = strtol(optarg, &endptr, 10);
        if (*endptr || !optarg) {
          fprintf(stderr, "invalid timeout specified: %s\n", optarg);
          exit(EX_DATAERR);
        }
        break;
      default:
        break;
    }
  }
  if (optind >= argc) {
    usage(progname);
    exit(EXIT_FAILURE);
  } else {
    command = strdup(argv[optind]);
    command_args = &argv[optind];
  }

  openlog(progname, debug | LOG_ODELAY | LOG_PID | LOG_NOWAIT, LOG_CRON);
  if (debug)
    setlogmask(LOG_UPTO(LOG_DEBUG));
  else
    setlogmask(LOG_UPTO(LOG_INFO));

  if (lock_filename == NULL) {
    if (asprintf(&lock_filename, "%s/%s.pid", make_tempdir(),
                 basename(command)) == -1) {
      perror("asprintf");
      exit(EX_OSERR);
    }
  }
  syslog(LOG_DEBUG, "lock filename is %s", lock_filename);

  sa.sa_handler = alarm_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, &old_sa);
  alarm(timeout);
  if ((fd = open(lock_filename, O_CREAT | O_RDWR | O_TRUNC,
                 S_IRUSR | S_IWUSR)) < 0) {
    perror(lock_filename);
    exit(EX_NOINPUT);
  } else {
    if (fcntl(fd, F_SETLKW, &fl) < 0) {
      switch (errno) {
        case EINTR:
          if (timeout_expired) {
            syslog(LOG_INFO,
                   "waited %d seconds, already locked by another process",
                   timeout);
            exit(EX_CANTCREAT);
          }
          break;
        case EACCES:
        case EAGAIN:
          syslog(LOG_INFO, "already locked by another process");
          exit(EX_CANTCREAT);
          break;
        default:
          perror("fcntl");
          exit(EXIT_FAILURE);
      }
    } else {
      alarm(0);
      sigaction(SIGALRM, &old_sa, NULL);
      snprintf(buf, BUFSIZ, "%d\n", getpid());
      if (write(fd, buf, strlen(buf)) == -1) {
        perror("write");
      }
      fsync(fd);
      syslog(LOG_DEBUG, "lock granted");
      status = run_subprocess(command, command_args, NULL);
      close(fd);
    }
  }
  closelog();
  return status;
}
