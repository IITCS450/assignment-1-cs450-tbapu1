#include "common.h"
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void usage(const char *a){fprintf(stderr,"Usage: %s <cmd> [args]\n",a); exit(1);}
static double d(struct timespec a, struct timespec b){
 return (b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;}
int main(int c,char**v){
  if (c < 2) usage(v[0]);

  struct timespec t0, t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) DIE("clock_gettime");

  pid_t pid = fork();
  if (pid < 0) DIE("fork");

  if (pid == 0) {
    execvp(v[1], &v[1]);
    perror("execvp");
    _exit(127);
  }

  int st = 0;
  if (waitpid(pid, &st, 0) < 0) DIE("waitpid");

  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) DIE("clock_gettime");

  double elapsed = d(t0, t1);

  printf("pid=%d elapsed=%.3f ", pid, elapsed);
  if (WIFEXITED(st)) {
    printf("exit=%d\n", WEXITSTATUS(st));
  } else if (WIFSIGNALED(st)) {
    printf("signal=%d\n", WTERMSIG(st));
  } else {
    printf("exit=?\n");
  }
 return 0;
}
