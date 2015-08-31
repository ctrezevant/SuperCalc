/*
  unop.c
  SuperCalc

  Created by C0deH4cker on 11/6/13.
  Copyright (c) 2013 C0deH4cker. All rights reserved.
*/

#include "unop.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "error.h"
#include "generic.h"
#include "context.h"
#include "value.h"

typedef Value* (*unop_t)(const Context*, const Value*);

static long long fact(long long n);
static Value* unop_fact(const Context* ctx, const Value* a);

static unop_t _unop_table[] = {
	&unop_fact
};
static const char* _unop_repr[] = {
	"!"
};

static long long fact(long long n) {
	long long ret = 1;
	
	while(n > 1) {
		ret *= n--;
	}
	
	return ret;
}

static Value* unop_fact(const Context* ctx, const Value* a) {
	Value* ret;
	
	if(a->type != VAL_INT) {
		return ValErr(typeError("Factorial operand must be an integer."));
	}
	
	if(a->ival > 20) {
		return ValErr(mathError("Factorial operand too large (%lld > 20).", a->ival));
	}
	
	ret = ValInt(fact(a->ival));
	
	return ret;
}

UnOp* UnOp_new(UNTYPE type, Value* a) {
	UnOp* ret = fmalloc(sizeof(*ret));
	
	ret->type = type;
	ret->a = a;
	
	return ret;
}

void UnOp_free(UnOp* term) {
	if(!term) return;
	
	if(term->a) {
		Value_free(term->a);
	}
	
	free(term);
}

UnOp* UnOp_copy(const UnOp* term) {
	return UnOp_new(term->type, Value_copy(term->a));
}

Value* UnOp_eval(const UnOp* term, const Context* ctx) {
	if(!term) {
		return ValErr(nullError());
	}
	
	Value* a = Value_coerce(term->a, ctx);
	if(a->type == VAL_ERR) {
		return a;
	}
	
	Value* ret = _unop_table[term->type](ctx, a);
	
	Value_free(a);
	
	return ret;
}

char* UnOp_verbose(const UnOp* term, int indent) {
	char* ret;
	
	if(!term) {
		return NULL;
	}
	
	char* spacing = spaces(indent + IWIDTH);
	char* current = spaces(indent);
	char* a = Value_verbose(term->a, indent + IWIDTH);
	
	asprintf(&ret, "%s (\n%s%s\n%s)", _unop_repr[term->type],
			 spacing, a,
			 current);
	
	free(spacing);
	free(current);
	free(a);
	
	return ret;
}

char* UnOp_repr(const UnOp* term, bool pretty) {
	char* ret;
	
	if(term == NULL) {
		return strNULL();
	}
	
	char* val = Value_repr(term->a, pretty);
	
	switch(term->type) {
		case UN_FACT:
			asprintf(&ret, "%s%s", val, _unop_repr[term->type]);
			break;
			
		default:
			/* Shouldn't be reached */
			DIE("Unknown unary operator type: %d.", term->type);
	}
	
	free(val);
	
	return ret;
}

