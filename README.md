cronutils - utilities to assist running batch processing jobs
=============================================================

![ci](https://github.com/google/cronutils/workflows/CI/badge.svg)

cronutils is a set of tools to assist the reliable running of periodic and batch jobs.

 *   `runalarm`: Limit the running time of a process.
 *   `runlock`: Prevent concurrent runs of a process.
 *   `runstat`: Export statistics about a process's execution.
 *   `runcron`: Simple wrapper around the above tools.

Used together, they can be used to specify overrun policies for periodic jobs, for example:

 *    Allow overrun -- let the first job run to completion
 *    Kill older job -- specify a timeout on the older job to limit its execution

Additionally, metrics can be exported to your favourite metrics collector at the termination of the job's run, so that abnormal behaviour can be monitored for and alerted on.

Mailing list at http://groups.google.com/group/cronutils-users.
