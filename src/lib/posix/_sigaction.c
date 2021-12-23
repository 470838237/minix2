#include <lib.h>
#define sigaction _sigaction
#include <sys/sigcontext.h>
#include <signal.h>
//typedef void _PROTOTYPE( (*__sighandler_t), (int) );
//struct sigaction {
//    __sighandler_t sa_handler;	/* SIG_DFL, SIG_IGN, or pointer to function */
//    sigset_t sa_mask;		/* signals to be blocked during handler */
//    int sa_flags;			/* special flags */
//};
_PROTOTYPE(int __sigreturn, (void));//汇编库__sigreturn.s实现该函数，sp+=16并跳转到_sigreturn

PUBLIC int sigaction(sig, act, oact)
int sig;
_CONST struct sigaction *act;
struct sigaction *oact;
{
  message m;

  m.m1_i2 = sig;

  /* XXX - yet more type puns because message struct is short of types. */
  m.m1_p1 = (char *) act;
  m.m1_p2 = (char *) oact;
  m.m1_p3 = (char *) __sigreturn;

  return(_syscall(MM, SIGACTION, &m));
}
