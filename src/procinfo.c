#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
static void usage(const char *a){fprintf(stderr,"Usage: %s <pid>\n",a); exit(1);}
static int isnum(const char*s){for(;*s;s++) if(!isdigit(*s)) return 0; return 1;}
int main(int c,char**v){
 if(c!=2||!isnum(v[1])) usage(v[0]);
 {
  long pid = strtol(v[1], NULL, 10);

  char path_stat[256], path_status[256], path_cmd[256];
  snprintf(path_stat, sizeof(path_stat), "/proc/%ld/stat", pid);
  snprintf(path_status, sizeof(path_status), "/proc/%ld/status", pid);
  snprintf(path_cmd, sizeof(path_cmd), "/proc/%ld/cmdline", pid);

  FILE *fs = fopen(path_stat, "r");
  if (!fs) {
    if (errno == ENOENT) fprintf(stderr, "Error: PID %s not found\n", v[1]);
    else if (errno == EACCES) fprintf(stderr, "Error: Permission denied for PID %s\n", v[1]);
    else fprintf(stderr, "Error: Cannot read /proc for PID %s: %s\n", v[1], strerror(errno));
    return 1;
  }

  char statline[4096];
  if (!fgets(statline, sizeof(statline), fs)) {
    fclose(fs);
    fprintf(stderr, "Error: Cannot read /proc for PID %s: %s\n", v[1], strerror(errno));
    return 1;
  }
  fclose(fs);

  char *lp = strchr(statline, '(');
  char *rp = strrchr(statline, ')');
  if (!lp || !rp || rp <= lp) {
    fprintf(stderr, "Error: Cannot parse stat for PID %s\n", v[1]);
    return 1;
  }

  char comm[256];
  size_t clen = (size_t)(rp - lp - 1);
  if (clen >= sizeof(comm)) clen = sizeof(comm) - 1;
  memcpy(comm, lp + 1, clen);
  comm[clen] = '\0';

  char *rest = rp + 1;
  while (*rest == ' ') rest++;

  char state = '?';
  long ppid = -1;
  unsigned long long utime = 0, stime = 0;

  char *save = NULL;
  char *tok = strtok_r(rest, " ", &save);
  for (int idx = 0; tok; idx++, tok = strtok_r(NULL, " ", &save)) {
    if (idx == 0) state = tok[0];
    else if (idx == 1) ppid = strtol(tok, NULL, 10);
    else if (idx == 11) utime = strtoull(tok, NULL, 10);
    else if (idx == 12) { stime = strtoull(tok, NULL, 10); break; }
  }

  if (state == '?' || ppid < 0) {
    fprintf(stderr, "Error: Cannot parse stat for PID %s\n", v[1]);
    return 1;
  }

  FILE *fc = fopen(path_cmd, "rb");
  if (!fc) {
    if (errno == ENOENT) fprintf(stderr, "Error: PID %s not found\n", v[1]);
    else if (errno == EACCES) fprintf(stderr, "Error: Permission denied for PID %s\n", v[1]);
    else fprintf(stderr, "Error: Cannot read /proc for PID %s: %s\n", v[1], strerror(errno));
    return 1;
  }

  char cmd[4096];
  size_t ncmd = fread(cmd, 1, sizeof(cmd) - 1, fc);
  fclose(fc);
  cmd[ncmd] = '\0';

  for (size_t i = 0; i < ncmd; i++) {
    if (cmd[i] == '\0') cmd[i] = ' ';
  }
  while (ncmd > 0 && isspace((unsigned char)cmd[ncmd - 1])) ncmd--;
  cmd[ncmd] = '\0';

  if (cmd[0] == '\0') snprintf(cmd, sizeof(cmd), "%s", comm);

  FILE *fst = fopen(path_status, "r");
  if (!fst) {
    if (errno == ENOENT) fprintf(stderr, "Error: PID %s not found\n", v[1]);
    else if (errno == EACCES) fprintf(stderr, "Error: Permission denied for PID %s\n", v[1]);
    else fprintf(stderr, "Error: Cannot read /proc for PID %s: %s\n", v[1], strerror(errno));
    return 1;
  }

  long vmrss = 0;
  char line[512];
  while (fgets(line, sizeof(line), fst)) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      char *p = line + 6;
      while (*p && !isdigit((unsigned char)*p) && *p != '-') p++;
      vmrss = strtol(p, NULL, 10);
      break;
    }
  }
  fclose(fst);

  unsigned long long total_ticks = utime + stime;
  long hz = sysconf(_SC_CLK_TCK);
  double cpu_sec = 0.0;
  if (hz > 0) cpu_sec = (double)total_ticks / (double)hz;

  printf("PID:%ld\n", pid);
  printf("State:%c\n", state);
  printf("PPID:%ld\n", ppid);
  printf("Cmd:%s\n", cmd);
  printf("CPU:%llu %.3f\n", total_ticks, cpu_sec);
  printf("VmRSS:%ld\n", vmrss);
}

 return 0;
}
