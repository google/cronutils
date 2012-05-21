/*
Copyright 2000-2010 Google, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
qyou may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sysexits.h>
#include <unistd.h>

#include "subprocess.h"

int timeout = 60 * 60 * 24; /* 1 day */
volatile sig_atomic_t alarm_triggered = 0;

static void usage(char * prog) {
  fprintf(stderr,
          "Usage: %s [options] command [arg [arg...]]\n\n"
          "This program tries to run a command and, if the timeout is\n"
          "reached before the command exits, kills that process.\n"
          "Otherwise the errorcode of the command is returned.\n"
          "\noptions:\n"
          " -t timeout  time in seconds to wait before process is killed\n"
          " -d   send log messages to stderr as well as syslog.\n"
          " -h   print this help\n", prog);
 }

static void alarm_handler(int signum) {
  int old_errno;

  (void) signum; /* suppress unused parameter warnings */
  old_errno = errno;
  alarm_triggered = 1;
  kill_process_group();
  errno = old_errno;
}

static void set_timeout_alarm(void) {
  alarm(timeout);
}

int main(int argc, char ** argv) {
  int arg;
  char * progname;
  int status = -1;
  char * command;
  char ** command_args;
  char * endptr;
  struct sigaction sa, old_sa;
  int debug = 0;

  progname = argv[0];

  while ((arg = getopt(argc, argv, "+t:hd")) > 0) {
    switch (arg) {
    case 'h':
      usage(progname);
      exit(EXIT_SUCCESS);
      break;
    case 't':
      timeout = strtol(optarg, &endptr, 10);
      if (*endptr || !optarg) {
        fprintf(stderr, "invalid timeout specified: %s\n", optarg);
        exit(EX_DATAERR);
      }
      break;
    case 'd':
      debug = LOG_PERROR;
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

  openlog(progname, debug|LOG_ODELAY|LOG_PID|LOG_NOWAIT, LOG_CRON);
  if (debug)
    setlogmask(LOG_UPTO(LOG_DEBUG));
  else
    setlogmask(LOG_UPTO(LOG_INFO));

  /* set up the alarm handler */
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = alarm_handler;
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, &old_sa);
  /* exec the command */
  status = run_subprocess(command, command_args, &set_timeout_alarm);
  alarm(0); /* shutdown the alarm */
  if (alarm_triggered) {
    syslog(LOG_INFO, "command '%s' timed out after %d seconds", command,
           timeout);
    status = 128 + SIGALRM;
  }
  closelog();
  exit(status);
}
