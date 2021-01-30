#ifndef _SIMTBL_H_
#define _SIMTBL_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

extern char encode_tbl[16][95];
extern char decode_tbl[16][95];

int simtbl_encode(int, char*, char*);
int simtbl_decode(int, char*, char*);
#endif
