/*
Copyright 2010 Google, Inc.

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

#include "subprocess.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

int childpid = -1; /* default to a bogus pid */
volatile sig_atomic_t killed_by_us = 0;
volatile sig_atomic_t fatal_error_in_progress = 0;

void kill_process_group() {
  int pgid;

  killed_by_us = 1;
  pgid = getpgid(childpid);
  if (killpg(pgid, SIGTERM) < 0) {
    perror("killpg");
    exit(EX_OSERR);
  }
}

static void termination_handler(int sig);
static void termination_handler(int sig) {
  int old_errno;

  if (fatal_error_in_progress) {
    raise(sig);
  }
  fatal_error_in_progress = 1;

  if (childpid > 0) {
    old_errno = errno;
    /* we were killed (SIGTERM), so make sure child dies too */
    kill_process_group();
    errno = old_errno;
  }

  signal(sig, SIG_DFL);
  raise(sig);
}

void install_termination_handler(void);
void install_termination_handler(void) {
  struct sigaction sa, old_sa;
  sa.sa_handler = termination_handler;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGINT);
  sigaddset(&sa.sa_mask, SIGHUP);
  sigaddset(&sa.sa_mask, SIGTERM);
  sigaddset(&sa.sa_mask, SIGQUIT);
  sa.sa_flags = 0;
  sigaction(SIGINT, NULL, &old_sa);
  if (old_sa.sa_handler != SIG_IGN) sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, NULL, &old_sa);
  if (old_sa.sa_handler != SIG_IGN) sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGTERM, NULL, &old_sa);
  if (old_sa.sa_handler != SIG_IGN) sigaction(SIGTERM, &sa, NULL);
}

int run_subprocess(char* command, char** args,
                   void (*pre_wait_function)(void)) {
  int pid;
  int status;

  childpid = fork();
  if (childpid == 0) {
    /* try to detach from parent's process group */
    if (setsid() == -1) {
      syslog(LOG_ERR, "Unable to detach child.  Aborting");
      return -1;
    }
    if (execvp(command, args)) {
      perror("execvp");
      exit(EX_NOINPUT);
    } else {
      /* Control almost certainly will not get to this point, ever. If
       * the call to execvp returned, instead of switching to a new memory
       * image, there was a problem. This exit will be collected by the
       * parent's call to waitpid() below.
       */
      exit(EX_UNAVAILABLE);
    }
  } else if (childpid < 0) {
    perror("fork");
    exit(EX_OSERR);
  } else {
    /* Make sure the child dies if we get killed. */
    /* Only the parent should do this, of course! */
    install_termination_handler();

    if (pre_wait_function != NULL) {
      pre_wait_function();
    }

    /* blocking wait on the child */
    while ((pid = waitpid(childpid, &status, 0)) < 0) {
      if (errno == EINTR) {
        if (killed_by_us) {
          break;
        } /* else restart the loop */
      } else {
        perror("waitpid");
      }
    }
    alarm(0);
    if (pid > 0) {
      if (pid != childpid) {
        syslog(LOG_ERR, "childpid %d not returned by waitpid! instead %d",
               childpid, pid);
        kill_process_group();
        exit(EX_OSERR);
      }

      /* exited normally? */
      if (WIFEXITED(status)) {
        /* decode and return exit sttus */
        status = WEXITSTATUS(status);
        syslog(LOG_DEBUG, "child exited with status %d", status);
      } else {
        /* This formula is a Unix shell convention */
        status = 128 + WTERMSIG(status);
        syslog(LOG_DEBUG, "child exited via signal %d", WTERMSIG(status));
      }
    }
    childpid = -1;
  }
  return status;
}
