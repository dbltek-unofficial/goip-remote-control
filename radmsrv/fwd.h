#ifndef __FWD__H
#define __FWD__H

#include "rc4.h"

void do_forward(int tsock, int psock, struct rc4_state* enc, struct rc4_state* dec);

#endif
