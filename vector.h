/*
  vector.h
  SuperCalc

  Created by Silas Schwarz on 11/16/13.
  Copyright (c) 2013 C0deH4cker and Silas Schwarz. All rights reserved.
*/

#ifndef _SC_VECTOR_H_
#define _SC_VECTOR_H_

#include <stdarg.h>

typedef struct Vector Vector;
#include "value.h"
#include "arglist.h"
#include "context.h"


struct Vector {
	ArgList* vals;
};


/* Constructor */
Vector* Vector_new(ArgList* vals);
Vector* Vector_create(unsigned count, /* Value* */...);
Vector* Vector_vcreate(unsigned count, va_list args);

/* Destructor */
void Vector_free(Vector* vec);

/* Copying */
Vector* Vector_copy(Vector* vec);

/* Parsing */
Value* Vector_parse(const char** expr);

/* Evaluation */
Value* Vector_eval(Vector* vec, Context* ctx);

/* Arithmetic */
Value* Vector_add(Vector* vec, Value* other, Context* ctx);
Value* Vector_sub(Vector* vec, Value* other, Context* ctx);
Value* Vector_rsub(Vector* vec, Value* scalar, Context* ctx);
Value* Vector_mul(Vector* vec, Value* other, Context* ctx);
Value* Vector_div(Vector* vec, Value* other, Context* ctx);
Value* Vector_rdiv(Vector* vec, Value* scalar, Context* ctx);
Value* Vector_pow(Vector* vec, Value* other, Context* ctx);
Value* Vector_rpow(Vector* vec, Value* scalar, Context* ctx);
Value* Vector_dot(Vector* vector1, Vector* vector2, Context* ctx);
Value* Vector_cross(Vector* vector1, Vector* vector2, Context* ctx);
Value* Vector_magnitude(Vector* vec, Context* ctx);
Value* Vector_normalize(Vector* vec, Context* ctx);

/* Access Values */
Value* Vector_elem(Vector* vec, Value* index, Context* ctx);

/* Printing */
char* Vector_verbose(Vector* vec, int indent);
char* Vector_repr(Vector* vec);

#endif
