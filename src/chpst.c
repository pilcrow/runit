#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include "sgetopt.h"
#include "error.h"
#include "strerr.h"
#include "str.h"
#include "uidgid.h"
#include "prot.h"
#include "strerr.h"
#include "scan.h"
#include "fmt.h"
#include "lock.h"
#include "pathexec.h"
#include "stralloc.h"
#include "byte.h"
#include "open.h"
#include "openreadclose.h"
#include "direntry.h"
#include "fd.h"
#include "coe.h"
#include "buffer.h"

#define USAGE_MAIN " [-xvVPN012] [-u user[:group]] [-U user[:group]] [-b argv0] [-e dir] [-/ [+]root] [-n [+|-]nice] [-l|-L lock] [-A n] [-m n] [-d n] [-o n] [-p n] [-f n] [-c n] [-r n] [-t n] prog"
#define FATAL ": fatal: "
#define WARNING ": warning: "

const char *progname;
static stralloc sa;

void fatal(const char *m) { strerr_die4sys(111, progname, FATAL, m, ": "); }
void fatal2(const char *m0, const char *m1) {
  strerr_die6sys(111, progname, FATAL, m0, ": ", m1, ": ");
}
void fatalx(const char *m0, const char *m1) {
  strerr_die5x(111, progname, FATAL, m0, ": ", m1);
}
void warn(const char *m) { strerr_warn3(progname, WARNING, m, 0); }
void die_nomem() { strerr_die3x(111, progname, FATAL, "out of memory."); }
void usage() { strerr_die4x(100, "usage: ", progname, USAGE_MAIN, "\n"); }

char *set_user =0;
char *env_user =0;
const char *argv0 =0;
const char *env_dir =0;
unsigned int verbose =0;
unsigned int pgrp =0;
unsigned int devnull =0;
unsigned int nostdin =0;
unsigned int nostdout =0;
unsigned int nostderr =0;
long setalrm =-1;
long limitd =-2;
long limits =-2;
long limitl =-2;
long limita =-2;
long limito =-2;
long limitp =-2;
long limitf =-2;
long limitc =-2;
long limitr =-2;
long limitt =-2;
long nicelvl =0;
const char *lock =0;
const char *root =0;
unsigned int lockdelay;
unsigned int lockexit0 =0;

void get_uidgid(struct uidgid *ugid, const char *user, unsigned int ext) {
  if (ext) {
    if (! uidgids_get(ugid, (char *)user)) {
      if (*user == ':') fatalx("invalid uid/gids", user +1);
      if (errno) fatal("unable to get password/group file entry");
      fatalx("unknown user/group", user);
    }
  }
  else
    if (! uidgid_get(ugid, (char *)user)) {
      if (errno) fatal("unable to get password file entry");
      fatalx("unknown account", user);
    }
}

void suidgid(struct uidgid *ugid) {
  if (setgroups(ugid->gids, ugid->gid) == -1) fatal("unable to setgroups");
  if (setgid(*ugid->gid) == -1) fatal("unable to setgid");
  if (prot_uid(ugid->uid) == -1) fatal("unable to setuid");
}

void euidgid(const char *user, unsigned int ext) {
  char bufnum[FMT_ULONG];
  struct uidgid ugid;

  get_uidgid(&ugid, user, ext);
  bufnum[fmt_ulong(bufnum, *ugid.gid)] =0;
  if (! pathexec_env("GID", bufnum)) die_nomem();
  bufnum[fmt_ulong(bufnum, ugid.uid)] =0;
  if (! pathexec_env("UID", bufnum)) die_nomem();
}

void newroot(const char *root) {
  if (chdir(root) == -1) fatal2("unable to change directory", root);
  if (chroot(".") == -1) fatal("unable to change root directory");
}

void edir(const char *dirname) {
  int wdir;
  DIR *dir;
  direntry *d;
  int i;

  if ((wdir =open_read(".")) == -1)
    fatal("unable to open current working directory");
  if (chdir(dirname)) fatal2("unable to switch to directory", dirname);
  if (! (dir =opendir("."))) fatal2("unable to open directory", dirname);
  for (;;) {
    errno =0;
    d =readdir(dir);
    if (! d) {
      if (errno) fatal2("unable to read directory", dirname);
      break;
    }
    if (d->d_name[0] == '.') continue;
    if (openreadclose(d->d_name, &sa, 256) == -1) {
      if ((errno == error_isdir) && env_dir) {
        if (verbose)
          strerr_warn6(WARNING, "unable to read ", dirname, "/",
                       d->d_name, ": ", &strerr_sys);
        continue;
      }
      else
        strerr_die6sys(111, FATAL, "unable to read ", dirname, "/",
                             d->d_name, ": ");
    }
    if (sa.len) {
      sa.len =byte_chr(sa.s, sa.len, '\n');
      while (sa.len && (sa.s[sa.len -1] == ' ' || sa.s[sa.len -1] == '\t'))
        --sa.len;
      for (i =0; i < sa.len; ++i) if (! sa.s[i]) sa.s[i] ='\n';
      if (! stralloc_0(&sa)) die_nomem();
      if (! pathexec_env(d->d_name, sa.s)) die_nomem();
    }
    else
      if (! pathexec_env(d->d_name, 0)) die_nomem();
  }
  closedir(dir);
  if (fchdir(wdir) == -1) fatal("unable to switch to starting directory");
  close(wdir);
}

void slock_die(const char *m, const char *f, unsigned int x) {
  if (! x) fatal2(m, f);
  _exit(0);
}
void slock(const char *f, unsigned int d, unsigned int x) {
  int fd;

  if ((fd =open_append(f)) == -1) slock_die("unable to open lock", f, x);
  if (d) {
    if (lock_ex(fd) == -1) slock_die("unable to lock", f, x);
    return;
  }
  if (lock_exnb(fd) == -1) slock_die("unable to lock", f, x);
}

int getlarg(long *l) {
  unsigned long ul;

  if (str_equal(optarg, "=")) { *l =-1; return 1; }
  if (optarg[scan_ulong(optarg, &ul)]) return 0;
  *l =ul;
  return 1;
}

void limit(int what, long l) {
  struct rlimit r;

  if (getrlimit(what, &r) == -1) fatal("unable to getrlimit()");
  if ((l < 0) || (l > r.rlim_max))
    r.rlim_cur =r.rlim_max;
  else
    r.rlim_cur =l;
  if (setrlimit(what, &r) == -1) fatal("unable to setrlimit()");
}
void slimit() {
  if (limitd >= -1) {
#ifdef RLIMIT_DATA
    limit(RLIMIT_DATA, limitd);
#else
    if (verbose) warn("system does not support RLIMIT_DATA");
#endif
  }
  if (limits >= -1) {
#ifdef RLIMIT_STACK
    limit(RLIMIT_STACK, limits);
#else
    if (verbose) warn("system does not support RLIMIT_STACK");
#endif
  }
  if (limitl >= -1) {
#ifdef RLIMIT_MEMLOCK
    limit(RLIMIT_MEMLOCK, limitl);
#else
    if (verbose) warn("system does not support RLIMIT_MEMLOCK");
#endif
  }
  if (limita >= -1) {
#ifdef RLIMIT_VMEM
    limit(RLIMIT_VMEM, limita);
#else
#ifdef RLIMIT_AS
    limit(RLIMIT_AS, limita);
#else
    if (verbose)
      warn("system does neither support RLIMIT_VMEM nor RLIMIT_AS");
#endif
#endif
  }
  if (limito >= -1) {
#ifdef RLIMIT_NOFILE
    limit(RLIMIT_NOFILE, limito);
#else
#ifdef RLIMIT_OFILE
    limit(RLIMIT_OFILE, limito);
#else
    if (verbose)
      warn("system does neither support RLIMIT_NOFILE nor RLIMIT_OFILE");
#endif
#endif
  }
  if (limitp >= -1) {
#ifdef RLIMIT_NPROC
    limit(RLIMIT_NPROC, limitp);
#else
    if (verbose) warn("system does not support RLIMIT_NPROC");
#endif
  }
  if (limitf >= -1) {
#ifdef RLIMIT_FSIZE
    limit(RLIMIT_FSIZE, limitf);
#else
    if (verbose) warn("system does not support RLIMIT_FSIZE");
#endif
  }
  if (limitc >= -1) {
#ifdef RLIMIT_CORE
    limit(RLIMIT_CORE, limitc);
#else
    if (verbose) warn("system does not support RLIMIT_CORE");
#endif
  }
  if (limitr >= -1) {
#ifdef RLIMIT_RSS
    limit(RLIMIT_RSS, limitr);
#else
    if (verbose) warn("system does not support RLIMIT_RSS");
#endif
  }
  if (limitt >= -1) {
#ifdef RLIMIT_CPU
    limit(RLIMIT_CPU, limitt);
#else
    if (verbose) warn("system does not support RLIMIT_CPU");
#endif
  }
}

void coe012(int in, int out, int err) {
  if (in) if (coe(0) == -1)
    fatal("unable to set close-on-exec on standard input");
  if (out) if (coe(1) == -1)
    fatal("unable to set close-on-exec on standard output");
  if (err) if (coe(2) == -1)
    fatal("unable to set close-on-exec on standard error");
}

void dn012(int in, int out, int err) {
  int dn;

  if (in) {
    if ((dn =open_read("/dev/null")) == -1)
      fatal("unable to open /dev/null for reading");
    if (fd_move(0, dn) == -1)
      fatal("unable to duplicate /dev/null to standard input");
  }

  if (out || err) {
    if ((dn =open_write("/dev/null")) == -1)
      fatal("unable to open /dev/null for writing");

    if (out) if (fd_copy(1, dn) == -1)
      fatal("unable to duplicate /dev/null to standard output");

    if (err) {
      int newerr =fd_dup(2);
      if (newerr == -1) fatal("unable to duplicate standard error");
      if (coe(newerr) == -1)
        fatal("unable to set close-on-exec on duplicated standard error");
      buffer_2->fd = newerr;
      if (fd_copy(2, dn) == -1)
        fatal("unable to duplicate /dev/null to standard error");
    }

    close(dn);
  }
}

/* argv[0] */
void setuidgid(int, const char *const *);
void envuidgid(int, const char *const *);
void envdir(int, const char *const *);
void pgrphack(int, const char *const *);
void setlock(int, const char *const *);
void softlimit(int, const char *const *);

int main(int argc, const char **argv) {
  int opt;
  int i;
  unsigned long ul;
  struct uidgid proc_ugid ={0};

  progname =argv[0];
  for (i =str_len(progname); i; --i)
    if (progname[i -1] == '/') {
      progname +=i;
      break;
    }
  if (progname[0] == 'd') ++progname;

  /* argv[0] */
  if (str_equal(progname, "setuidgid")) setuidgid(argc, argv);
  if (str_equal(progname, "envuidgid")) envuidgid(argc, argv);
  if (str_equal(progname, "envdir")) envdir(argc, argv);
  if (str_equal(progname, "pgrphack")) pgrphack(argc, argv);
  if (str_equal(progname, "setlock")) setlock(argc, argv);
  if (str_equal(progname, "softlimit")) softlimit(argc, argv);

  while ((opt =getopt(argc, argv, "u:U:b:e:m:d:o:p:f:c:r:t:/:n:l:L:A:vxNP012V"))
         != opteof)
    switch(opt) {
    case 'u': set_user =(char*)optarg; break;
    case 'U': env_user =(char*)optarg; break;
    case 'b': argv0 =(char*)optarg; break;
    case 'e': env_dir =optarg; break;
    case 'm': if (!getlarg(&limitd)) usage();
              limits =limitl =limita =limitd;
              break;
    case 'd': if (!getlarg(&limitd)) usage(); break;
    case 'o': if (!getlarg(&limito)) usage(); break;
    case 'p': if (!getlarg(&limitp)) usage(); break;
    case 'f': if (!getlarg(&limitf)) usage(); break;
    case 'c': if (!getlarg(&limitc)) usage(); break;
    case 'r': if (!getlarg(&limitr)) usage(); break;
    case 't': if (!getlarg(&limitt)) usage(); break;
    case '/': root =optarg; break;
    case 'n':
      switch (*optarg) {
        case '-':
          if (optarg[scan_ulong(++optarg, &ul)]) usage(); nicelvl =ul;
          nicelvl *=-1;
          break;
        case '+': ++optarg;
        default:
          if (optarg[scan_ulong(optarg, &ul)]) usage(); nicelvl =ul;
          break;
      }
      break;
    case 'l': if (lock) usage(); lock =optarg; lockdelay =1; break;
    case 'L': if (lock) usage(); lock =optarg; lockdelay =0; break;
    case 'x': lockexit0 =1; break;
    case 'v': verbose =1; break;
    case 'A': if (optarg[scan_ulong(optarg, &ul)]) usage(); setalrm =ul; break;
    case 'N': devnull =1; break;
    case 'P': pgrp =1; break;
    case '0': nostdin =1; break;
    case '1': nostdout =1; break;
    case '2': nostderr =1; break;
    case 'V': strerr_warn1("chpst:pilcrow/runit-2.1.1:2012102301", 0);
    case '?': usage();
    }
  argv +=optind;
  if (! argv || ! *argv) usage();

  if (lockexit0 && ! lock) usage();

  if (pgrp) setsid();
  if (env_dir) edir(env_dir);
  (devnull?dn012:coe012)(nostdin, nostdout, nostderr);
  if (nicelvl) {
    errno =0;
    if (nice(nicelvl) == -1) if (errno) fatal("unable to set nice level");
  }
  if (root && root[0] == '+') {
    /* More sensible chroot behavior:
     *   . Lookup account info prior to chroot (-u/-U)
     *   . Obtain lockfile prior to chroot (-l/-L)
     * Note that we necessarily slock() before dropping privs, unlike
     * original behavior.
     */
    if (env_user) euidgid(env_user, 1);
    if (set_user) get_uidgid(&proc_ugid, set_user, 1);
    if (lock) slock(lock, lockdelay, lockexit0);
    root++;
    newroot(root);
    if (proc_ugid.gids) suidgid(&proc_ugid);
  } else {
    /* Less sensible, original behavior */
    if (root) newroot(root);
    if (env_user) euidgid(env_user, 1);
    if (set_user) get_uidgid(&proc_ugid, set_user, 1);
    if (proc_ugid.gids) suidgid(&proc_ugid);
    if (lock) slock(lock, lockdelay, lockexit0);
  }
  slimit();
  if (setalrm >= 0) alarm(setalrm);

  progname =*argv;
  if (argv0) *argv =argv0;
  pathexec_env_run(progname, argv);
  fatal2("unable to run", *argv);
  return(0);
}

/* argv[0] */
#define USAGE_SETUIDGID " account child"
#define USAGE_ENVUIDGID " account child"
#define USAGE_ENVDIR " dir child"
#define USAGE_PGRPHACK " child"
#define USAGE_SETLOCK " [ -nNxX ] file program [ arg ... ]"
#define USAGE_SOFTLIMIT " [-a allbytes] [-c corebytes] [-d databytes] [-f filebytes] [-l lockbytes] [-m membytes] [-o openfiles] [-p processes] [-r residentbytes] [-s stackbytes] [-t cpusecs] child"

void setuidgid_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_SETUIDGID, "\n");
}
void setuidgid(int argc, const char *const *argv) {
  const char *account;
  struct uidgid ugid;

  if (! (account =*++argv)) setuidgid_usage();
  if (! *++argv) setuidgid_usage();
  get_uidgid(&ugid, (char*)account, 0);
  suidgid(&ugid);
  pathexec(argv);
  fatal2("unable to run", *argv);
}

void envuidgid_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_ENVUIDGID, "\n");
}
void envuidgid(int argc, const char *const *argv) {
  const char *account;

  if (! (account =*++argv)) envuidgid_usage();
  if (! *++argv) envuidgid_usage();
  euidgid((char*)account, 0);
  pathexec(argv);
  fatal2("unable to run", *argv);
}

void envdir_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_ENVDIR, "\n");
}
void envdir(int argc, const char *const *argv) {
  const char *dir;

  if (! (dir =*++argv)) envdir_usage();
  if (! *++argv) envdir_usage();
  edir(dir);
  pathexec(argv);
  fatal2("unable to run", *argv);
}

void pgrphack_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_PGRPHACK, "\n");
}
void pgrphack(int argc, const char *const *argv) {
  if (! *++argv) pgrphack_usage();
  setsid();
  pathexec(argv);
  fatal2("unable to run", *argv);
}

void setlock_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_SETLOCK, "\n");
}
void setlock(int argc, const char *const *argv) {
  int opt;
  unsigned int delay =0;
  unsigned int x =0;
  const char *fn;

  while ((opt =getopt(argc, argv, "nNxX")) != opteof)
    switch(opt) {
      case 'n': delay =1; break;
      case 'N': delay =0; break;
      case 'x': x =1; break;
      case 'X': x =0; break;
      default: setlock_usage();
    }
  argv +=optind;
  if (! (fn =*argv)) setlock_usage();
  if (! *++argv) setlock_usage();

  slock(fn, delay, x);
  pathexec(argv);
  if (! x) fatal2("unable to run", *argv);
  _exit(0);
}

void softlimit_usage() {
  strerr_die4x(100, "usage: ", progname, USAGE_SOFTLIMIT, "\n");
}
void softlimit(int argc, const char *const *argv) {
  int opt;

  while ((opt =getopt(argc,argv,"a:c:d:f:l:m:o:p:r:s:t:")) != opteof)
    switch(opt) {
    case '?': softlimit_usage();
    case 'a': if (!getlarg(&limita)) softlimit_usage(); break;
    case 'c': if (!getlarg(&limitc)) softlimit_usage(); break;
    case 'd': if (!getlarg(&limitd)) softlimit_usage(); break;
    case 'f': if (!getlarg(&limitf)) softlimit_usage(); break;
    case 'l': if (!getlarg(&limitl)) softlimit_usage(); break;
    case 'm': if (!getlarg(&limitd)) softlimit_usage();
              limits =limitl =limita =limitd;
              break;
    case 'o': if (!getlarg(&limito)) softlimit_usage(); break;
    case 'p': if (!getlarg(&limitp)) softlimit_usage(); break;
    case 'r': if (!getlarg(&limitr)) softlimit_usage(); break;
    case 's': if (!getlarg(&limits)) softlimit_usage(); break;
    case 't': if (!getlarg(&limitt)) softlimit_usage(); break;
    }
  argv +=optind;
  if (!*argv) softlimit_usage();
  slimit();
  pathexec(argv);
  fatal2("unable to run", *argv);
}
