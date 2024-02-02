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

#define _GNU_SOURCE /* asprintf */

#include "tempdir.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

const char* template = "/tmp/cronutils-";
char* dirname = NULL;

char* make_tempdir(void) {
  uid_t uid;
  struct passwd* pw;
  struct stat st;

  uid = geteuid();
  if ((pw = getpwuid(uid)) == NULL) {
    perror("getpwuid");
    exit(EX_OSERR);
  }
  if (asprintf(&dirname, "%s%s", template, pw->pw_name) == -1) {
    perror("asprintf");
    exit(EX_OSERR);
  }
  syslog(LOG_DEBUG, "temp dir is %s\n", dirname);
  if (mkdir(dirname, S_IRWXU) < 0) {
    if (errno == EEXIST) {
      if (stat(dirname, &st) != 0) {
        perror("stat");
        exit(EX_OSERR);
      }
      if (!S_ISDIR(st.st_mode)) {
        syslog(LOG_ERR, "%s is not a directory\n", dirname);
        exit(EX_IOERR);
      }
      if (st.st_uid != uid) {
        syslog(LOG_ERR, "%s is not owned by %s\n", dirname, pw->pw_name);
        exit(EXIT_FAILURE);
      }
      if (!(st.st_mode & S_IRWXU)) {
        syslog(LOG_ERR, "%s has insecure permissions %u\n", dirname,
               st.st_mode);
        exit(EXIT_FAILURE);
      }
    } else {
      perror("mkdir");
      exit(EX_OSERR);
    }
  }
  return dirname;
}
