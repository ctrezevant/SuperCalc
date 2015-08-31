/*
  supercalc.h
  SuperCalc

  Created by C0deH4cker on 11/22/13.
  Copyright (c) 2013 C0deH4cker. All rights reserved.
*/

#ifndef _SUPERCALC_H_
#define _SUPERCALC_H_

#include <stdio.h>
#include <stdbool.h>

typedef struct SuperCalc SuperCalc;
#include "value.h"
#include "context.h"
#include "generic.h"

struct SuperCalc {
	Context* ctx;
	bool interactive;
	FILE* fin;
	FILE* fout;
};

SuperCalc* SC_new(FILE* fout);
void SC_free(SuperCalc* sc);
Value* SC_runFile(const SuperCalc* sc, FILE* fp);
Value* SC_runInteractive(const SuperCalc* sc, FILE* fp);
Value* SC_runString(const SuperCalc* sc, const char* str, VERBOSITY v);

#endif