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

#define _GNU_SOURCE /* dprintf() */

#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/un.h>
#include <sysexits.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "subprocess.h"
#include "tempdir.h"

static void usage(char * prog) {
  fprintf(stderr,
          "Usage: %s [options] command [arg [arg] ...]\n\n"
          "This program tries to execute a command in a"
          "subprocess, and upon termination of the subprocess"
          "writes some runtime statistics to a file."
          "These statistics include time of execution, exit"
          "status, and timestamp of completion.\n"
          "\noptions:\n"
          " -f path  Path to save the statistics file.\n"
          " -C path  Path to collectd socket.\n"
          " -d       send log messages to stderr as well as syslog.\n"
          " -h       print this help\n", prog);
}

const char * TEMPLATE = ".XXXXXX";

enum var_kind {
  GAUGE,
  ABSOLUTE
};

struct variable {
  struct variable * next;

  char * name;
  char * value;
  char * units;
  enum var_kind kind;
};

void add_variable(struct variable ** var_list, const char * name, const enum var_kind kind, const char * units, const char * fmt, ...);
void add_variable(struct variable ** var_list, const char * name, const enum var_kind kind, const char * units, const char * fmt, ...) {
  char buf[1024];
  va_list ap;
  struct variable * var;

  var = malloc(sizeof(struct variable));
  if (!var) {
    perror("malloc");
    exit(EX_OSERR);
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  var->name = strdup(name);
  var->units = units ? strdup(units) : NULL;
  var->value = strdup(buf);
  var->kind = kind;
  var->next = *var_list;
  *var_list = var;
}

int main(int argc, char ** argv) {
  char * progname;
  int arg;
  char collectd_sockname[PATH_MAX] = {'\0'};
  char statistics_filename[PATH_MAX] = {'\0'};
  char temp_filename[PATH_MAX] = {'\0'};
  char * command;
  char ** command_args;
  struct timeval start_wall_time, end_wall_time;
  struct timespec start_run_time, end_run_time;
  long int elapsed_sec, elapsed_nsec;
  int status;
  int temp_fd;
  char buf[1024];
  int debug = 0;
  struct rusage ru;
  struct variable * var_list = NULL, * var;

  progname = argv[0];

  while ((arg = getopt(argc, argv, "+C:f:hd")) > 0) {
    switch (arg) {
    case 'C':
      strncat(collectd_sockname, optarg, PATH_MAX - 1);
      collectd_sockname[PATH_MAX-1] = '\0';
      break;
    case 'h':
      usage(progname);
      exit(EXIT_SUCCESS);
      break;
    case 'f':
      strncat(statistics_filename, optarg, PATH_MAX - 1);
      statistics_filename[PATH_MAX-1] = '\0';
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

  openlog(progname,
          debug|LOG_ODELAY|LOG_PID|LOG_NOWAIT,
          LOG_CRON);
  if (debug)
    setlogmask(LOG_UPTO(LOG_DEBUG));
  else
    setlogmask(LOG_UPTO(LOG_INFO));

  gettimeofday(&start_wall_time, NULL);
  clock_gettime(CLOCK_MONOTONIC, &start_run_time);

  status = run_subprocess(command, command_args, NULL);

  clock_gettime(CLOCK_MONOTONIC, &end_run_time);
  gettimeofday(&end_wall_time, NULL);


  if (statistics_filename[0] == '\0') {
    strncat(statistics_filename, make_tempdir(),
            PATH_MAX - strlen(statistics_filename) - 1);
    strncat(statistics_filename, "/",
            PATH_MAX - strlen(statistics_filename) - 1);
    strncat(statistics_filename, command,
            PATH_MAX - strlen(statistics_filename) - 1);
    strncat(statistics_filename, ".stat",
            PATH_MAX - strlen(statistics_filename) - 1);
    statistics_filename[PATH_MAX-1] = '\0';
  }
  syslog(LOG_DEBUG, "statistics filename is %s", statistics_filename);

  strncat(temp_filename, statistics_filename, PATH_MAX - 1);
  strncat(temp_filename, TEMPLATE, PATH_MAX - strlen(temp_filename) - 1);
  syslog(LOG_DEBUG, "temp filename is %s", temp_filename);

  if ((temp_fd = mkstemp(temp_filename)) < 0) {
    perror("mkstemp");
    exit(EX_OSERR);
  }

  /** process */
  add_variable(&var_list, "exit_status", GAUGE, NULL, "%d", status);

  /** wall time */
  /* ABSOLUTE hostname/runstat-progname/last_run-epoch_timestamp_start */
  add_variable(&var_list, "start_timestamp", ABSOLUTE, "time_t",
               "%ld.%.6ld", start_wall_time.tv_sec, start_wall_time.tv_usec);
  /* ABSOLUTE hostname/runstat-progname/last_run-epoch_timestamp_end */
  add_variable(&var_list, "end_timestamp", ABSOLUTE, "time_t",
               "%ld.%.6ld", end_wall_time.tv_sec, end_wall_time.tv_usec);

  /** timing */
  elapsed_sec = end_run_time.tv_sec - start_run_time.tv_sec;
  elapsed_nsec = end_run_time.tv_nsec - start_run_time.tv_nsec;
  /* GAUGE hostname/runstat-progname/last_run-elapsed-time */
  add_variable(&var_list, "elapsed_time", GAUGE, "s",
               "%ld.%.9ld", elapsed_sec, elapsed_nsec);

  /** resource usage */
  if (getrusage(RUSAGE_CHILDREN, &ru) == 0) {
    add_variable(&var_list, "user_time", GAUGE, "s",
                 "%ld.%.6ld", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
    add_variable(&var_list, "system_time", GAUGE, "s",
                 "%ld.%.6ld", ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);

    add_variable(&var_list, "rss-max", GAUGE, "B", "%ld", ru.ru_maxrss);
    add_variable(&var_list, "rss-shared", GAUGE, "B", "%ld", ru.ru_ixrss);
    add_variable(&var_list, "rss-data_unshared", GAUGE, "B",
                 "%ld", ru.ru_idrss);
    add_variable(&var_list, "rss-stack_unshared", GAUGE, "B",
                 "%ld", ru.ru_isrss);

    add_variable(&var_list, "page-reclaims", GAUGE, "pages",
                 "%ld", ru.ru_minflt);
    add_variable(&var_list, "page-faults", GAUGE, "pages",
                 "%ld", ru.ru_majflt);
    add_variable(&var_list, "swaps", GAUGE, "swaps",
                 "%ld", ru.ru_nswap);

    add_variable(&var_list, "block_ios-in", GAUGE, "block_ios",
                 "%ld", ru.ru_inblock);
    add_variable(&var_list, "block_ios-out", GAUGE, "block_ios",
                 "%ld", ru.ru_oublock);

    add_variable(&var_list, "messages-sent", GAUGE, "messages",
                 "%ld", ru.ru_msgsnd);
    add_variable(&var_list, "messages-received", GAUGE, "messages",
                 "%ld", ru.ru_msgrcv);

    add_variable(&var_list, "signals-received", GAUGE, "signals",
                 "%ld", ru.ru_nsignals);
    add_variable(&var_list, "ctx_switch-voluntary", GAUGE,
                 "context switches", "%ld", ru.ru_nvcsw);
    add_variable(&var_list, "ctx_switch-involuntary", GAUGE,
                 "context switches", "%ld", ru.ru_nivcsw);
  }

  /* CSV emitter */
  for (var = var_list; var != NULL; var = var->next) {
    snprintf(buf, sizeof(buf), "%s,%s,%s,%s\n",
             command,
             var->name,
             var->value,
             var->units ? var->units : ""
             );
    write(temp_fd, buf, strlen(buf));
 }

  fsync(temp_fd);
  close(temp_fd);

  if (rename(temp_filename, statistics_filename) < 0) {
    perror("rename");
    exit(EX_OSERR);
  }

  /* Write to collectd */
  if (collectd_sockname[0] != '\0') {
    char hostname[HOST_NAME_MAX];
    struct sockaddr_un sock;
    int s;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      goto end;
    }
    sock.sun_family = AF_UNIX;
    strncpy(sock.sun_path, collectd_sockname, sizeof(sock.sun_path) - 1);
    sock.sun_path[sizeof(sock.sun_path) - 1] = '\0';
    if (connect(s, (struct sockaddr*) &sock,
                strlen(sock.sun_path) + sizeof(sock.sun_family)) == -1) {
      perror("connect");
      goto end;
    }

    gethostname(hostname, HOST_NAME_MAX-1);

    for (var = var_list; var != NULL; var = var->next) {
      char type[10] = {'\0'};
      switch (var->kind) {
      case GAUGE:
        strncpy(type, "gauge", 9);
        break;
      case ABSOLUTE:
        strncpy(type, "counter", 9);
        break;
      default:
        perror("unknown var->kind");
        break;
      }
      dprintf(s, "PUTVAL \"%s/runstat-%s/%s-%s\" %ld:%s\n",
              hostname,
              basename(command),
              type, var->name,
              end_wall_time.tv_sec,
              var->value);
      /* This next line is a bit of a hack to clear the pipe.*/
      recv(s, buf, sizeof(buf) - 1, 0);
    }
    close(s);
  }
 end:
  closelog();
  return status;
}
