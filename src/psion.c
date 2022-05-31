#include <stdio.h>
#include <stdarg.h>
#include <bfd.h>
#include <signal.h>
#include <getopt.h>
#include <termios.h>
#include "armdefs.h"
#include "armemu.h"

static int big_endian = 0;
struct ARMul_State *state = 0;
int stop_simulator = 0;
struct termios old, tmp;


void usage(void)
{
  printf("Psion Series 5 emulator\n");
  printf("Usage: psion [-v]\n");
  exit(0);
}

void term_handler( int sig )
{FILE *f;
  printf("Got signal %d, exiting\n", sig);
  dump_dram(state);
  /* Restore the original terminal settings */    
  tcsetattr(0, TCSANOW, &old);
  exit(0);
}

int
main (int ac, char **av)
{int i,verbose;
 struct sigaction  act;

    while ((i = getopt (ac, av, "v")) != EOF) 
    switch (i)
    {
      case 'v':
	/* Things that are printed with -v are the kinds of things that
	   gcc -v prints.  This is not meant to include detailed tracing
	   or debugging information, just summaries.  */
	verbose = 1;
	break;
      default:
	usage ();
    }

    /* Set the terminal for non-blocking per-character (not per-line) input, no echo */
    tcgetattr(0, &old);
    tcgetattr(0, &tmp);
    tmp.c_lflag &= ~ICANON;
    tmp.c_lflag |= ISIG;
    tmp.c_lflag &= ~ECHO;
    tmp.c_cc[VMIN] = 0;
    tmp.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tmp);

    /* first of all set SIGTERM signal handler */
    act.sa_handler = term_handler;
    act.sa_flags   = 0;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);

    ARMul_EmulateInit();
    state = ARMul_NewState ();
    state->bigendSig = big_endian ? HIGH : LOW;
    ARMul_CoProInit(state); 
    state->verbose = verbose;
    ARMul_SelectProcessor(state, ARM600);
    ARMul_SetCPSR(state, USER32MODE);
    ARMul_Reset(state);
    ARMul_SetPC (state, 0);
    state->NextInstr = RESUME; /* treat as PC change */
    state->Reg[15] = ARMul_DoProg (state);
    exit(0);
}
