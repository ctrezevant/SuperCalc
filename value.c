/*
  value.c
  SuperCalc

  Created by C0deH4cker on 10/21/13.
  Copyright (c) 2013 C0deH4cker. All rights reserved.
*/

#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <float.h>

#include "support.h"
#include "error.h"
#include "generic.h"
#include "binop.h"
#include "fraction.h"
#include "unop.h"
#include "funccall.h"
#include "vector.h"
#include "context.h"
#include "variable.h"
#include "arglist.h"
#include "supercalc.h"
#include "template.h"


static Value* allocValue(VALTYPE type);
static void treeAddValue(BinOp** tree, BinOp** prev, BINTYPE op, Value* val);
static Value* parseNum(const char** expr);
static Value* subscriptVector(Value* val, const char** expr, parser_cb* cb);
static Value* callFunc(Value* val, const char** expr, parser_cb* cb);
static Value* parseToken(const char** expr, parser_cb* cb);


/* By default, the '@' character is illegal */
Value* _default_cb(const char** expr, void* data) {
	return ValErr(badChar(**expr));
}
parser_cb default_cb = {&_default_cb, NULL};


static Value* allocValue(VALTYPE type) {
	Value* ret = fcalloc(1, sizeof(*ret));
	ret->type = type;
	return ret;
}

Value* ValEnd(void) {
	return allocValue(VAL_END);
}

Value* ValErr(Error* err) {
	Value* ret = allocValue(VAL_ERR);
	ret->err = err;
	return ret;
}

Value* ValNeg(void) {
	return allocValue(VAL_NEG);
}

Value* ValInt(long long val) {
	Value* ret = allocValue(VAL_INT);
	ret->ival = val;
	return ret;
}

Value* ValReal(double val) {
	Value* ret = allocValue(VAL_REAL);
	ret->rval = val;
	return ret;
}

Value* ValFrac(Fraction* frac) {
	Value* ret = allocValue(VAL_FRAC);
	ret->frac = frac;
	Fraction_reduce(ret);
	return ret;
}

Value* ValExpr(BinOp* expr) {
	Value* ret = allocValue(VAL_EXPR);
	ret->expr = expr;
	return ret;
}

Value* ValUnary(UnOp* term) {
	Value* ret = allocValue(VAL_UNARY);
	ret->term = term;
	return ret;
}

Value* ValCall(FuncCall* call) {
	Value* ret = allocValue(VAL_CALL);
	ret->call = call;
	return ret;
}

Value* ValVar(const char* name) {
	Value* ret = allocValue(VAL_VAR);
	ret->name = strdup(name);
	return ret;
}

Value* ValVec(Vector* vec) {
	Value* ret = allocValue(VAL_VEC);
	ret->vec = vec;
	return ret;
}

Value* ValPlace(Placeholder* ph) {
	Value* ret = allocValue(VAL_PLACE);
	ret->ph = ph;
	return ret;
}

void Value_free(Value* val) {
	if(!val) return;
	
	switch(val->type) {
		case VAL_EXPR:
			BinOp_free(val->expr);
			break;
		
		case VAL_UNARY:
			UnOp_free(val->term);
			break;
		
		case VAL_CALL:
			FuncCall_free(val->call);
			break;
		
		case VAL_FRAC:
			Fraction_free(val->frac);
			break;
		
		case VAL_VAR:
			free(val->name);
			break;
		
		case VAL_VEC:
			Vector_free(val->vec);
			break;
		
		case VAL_ERR:
			Error_free(val->err);
			break;
		
		default:
			/* The rest don't need to be freed */
			break;
	}
	
	free(val);
}

Value* Value_copy(const Value* val) {
	Value* ret;
	
	switch(val->type) {
		case VAL_INT:
			ret = ValInt(val->ival);
			break;
		
		case VAL_REAL:
			ret = ValReal(val->rval);
			break;
		
		case VAL_FRAC:
			ret = ValFrac(Fraction_copy(val->frac));
			break;
		
		case VAL_EXPR:
			ret = ValExpr(BinOp_copy(val->expr));
			break;
		
		case VAL_CALL:
			ret = ValCall(FuncCall_copy(val->call));
			break;
		
		case VAL_UNARY:
			ret = ValUnary(UnOp_copy(val->term));
			break;
		
		case VAL_VAR:
			ret = ValVar(val->name);
			break;
		
		case VAL_VEC:
			ret = ValVec(Vector_copy(val->vec));
			break;
		
		case VAL_NEG:
			/* Shouldn't be reached, but so easy to code */
			ret = ValNeg();
			break;
		
		case VAL_ERR:
			ret = ValErr(Error_copy(val->err));
			break;
		
		default:
			typeError("Unknown value type: %d.", val->type);
			ret = NULL;
			break;
	}
	
	return ret;
}

Value* Value_eval(const Value* val, const Context* ctx) {
	if(val == NULL) return ValErr(nullError());
	
	Value* ret;
	Variable* var;
	
	switch(val->type) {
		/* These can be evaluated to a simpler form */
		case VAL_EXPR:
			ret = BinOp_eval(val->expr, ctx);
			break;
		
		case VAL_UNARY:
			ret = UnOp_eval(val->term, ctx);
			break;
		
		case VAL_CALL:
			ret = FuncCall_eval(val->call, ctx);
			break;
		
		case VAL_FRAC:
			ret = Value_copy(val);
			Fraction_reduce(ret);
			break;
		
		case VAL_VAR:
			var = Variable_get(ctx, val->name);
			if(var) {
				ret = Variable_eval(var, ctx);
			}
			else {
				ret = ValErr(varNotFound(val->name));
			}
			break;
		
		case VAL_VEC:
			ret = Vector_eval(val->vec, ctx);
			break;
		
		/* These can't be simplified, so just copy them */
		case VAL_INT:
		case VAL_REAL:
		case VAL_NEG:
		case VAL_ERR:
			ret = Value_copy(val);
			break;
		
		default:
			/* Shouldn't be reached */
			badValType(val->type);
	}
	
	return ret;
}

Value* Value_coerce(const Value* val, const Context* ctx) {
	Value* ret = Value_eval(val, ctx);
	
	if(ret->type == VAL_VAR) {
		Variable* var = Variable_get(ctx, ret->name);
		
		if(var == NULL) {
			Value* tmp = ValErr(varNotFound(ret->name));
			Value_free(ret);
			ret = tmp;
		}
		else {
			ret = Variable_coerce(var, ctx);
		}
	}
	
	return ret;
}

double Value_asReal(const Value* val) {
	double ret;
	
	switch(val->type) {
		case VAL_INT:
			ret = val->ival;
			break;
		
		case VAL_REAL:
			ret = val->rval;
			break;
		
		case VAL_FRAC:
			ret = Fraction_asReal(val->frac);
			break;
		
		default:
			/* Expression couldn't be evaluated, so it's not a number */
			ret = NAN;
			break;
	}
	
	return ret;
}

Value* Value_parse(const char** expr, char sep, char end, parser_cb* cb) {
	Value* val;
	BINTYPE op = BIN_UNK;
	BinOp* tree = NULL;
	BinOp* prev;
	
	while(1) {
		/* Get next value */
		val = Value_next(expr, end, cb);
		
		/* Error parsing next value? */
		if(val->type == VAL_ERR) {
			if(tree) {
				BinOp_free(tree);
			}
			
			return val;
		}
		
		/* End of input? */
		if(val->type == VAL_END) {
			if(tree) {
				BinOp_free(tree);
			}
			
			return val;
		}
		
		/* Special case: negative value */
		if(val->type == VAL_NEG) {
			Value_free(val);
			
			BinOp* cur = BinOp_new(BIN_MUL, ValInt(-1), NULL);
			
			if(tree) {
				prev->b = ValExpr(cur);
			}
			else {
				tree = cur;
			}
			
			prev = cur;
			continue;
		}
		
		/* Get next operator if it exists */
		op = BinOp_nextType(expr, sep, end);
		
		/* Invalid operator? Return syntax error */
		if(op == BIN_UNK) {
			/* Exit gracefully and return error */
			if(tree) {
				BinOp_free(tree);
			}
			
			Value_free(val);
			return ValErr(badChar(**expr));
		}
		/* End of the statement? */
		else if(op == BIN_END) {
			/* Only skip end character if there's only one value to parse */
			if(!sep && **expr && **expr == end) {
				(*expr)++;
			}
			
			/* If there was only one value, return it */
			if(!tree) {
				return val;
			}
			
			/* Otherwise, place the final value into the tree and break out of the parse loop */
			prev->b = val;
			break;
		}
		
		/* Tree not yet begun? Initialize it! */
		if(tree == NULL) {
			tree = BinOp_new(op, val, NULL);
			prev = tree;
		}
		else {
			/* Tree already started, so add to it */
			treeAddValue(&tree, &prev, op, val);
		}
	}
	
	return ValExpr(tree);
}

static void treeAddValue(BinOp** tree, BinOp** prev, BINTYPE op, Value* val) {
	BinOp* parent = *tree;
	BinOp* cur = *tree;
	BinOp* next;
	
	/*
	 Descend down the right side of the tree until we find either an empty node or
	 an operator with a higher precedence than the current one.
	 */
	while(cur->b && BinOp_cmp(cur->type, op) < 0) {
		parent = cur;
		cur = cur->b->expr;
	}
	
	if(BinOp_cmp(cur->type, op) >= 0) {
		/* Replace current node with new one */
		if(cur == *tree) {
			/* At the tree's root */
			next = BinOp_new(op, ValExpr(*tree), NULL);
			*tree = next;
		}
		else {
			/* Somewhere in the tree */
			next = BinOp_new(op, parent->b, NULL);
			parent->b = ValExpr(next);
		}
		
		(*prev)->b = val;
	}
	else {
		/* New node is child of current node */
		next = BinOp_new(op, val, NULL);
		(*prev)->b = ValExpr(next);
	}
	
	*prev = next;
}

static Value* parseNum(const char** expr) {
	Value* ret;
	char* end1;
	char* end2;
	
	errno = 0;
	double dbl = strtod(*expr, &end1);
	if(errno != 0 || *expr == end1) {
		/* An error occurred (EINVAL, ERANGE) */
		end1 = NULL;
	}
	
	long long ll = strtoll(*expr, &end2, 10);
	if(errno != 0 || *expr == end2) {
		/* An error occurred (EINVAL, ERANGE) */
		end2 = NULL;
	}
	
	if(end1 > end2) {
		/* Must be a double because more of the string was parsed as double than long long */
		ret = ValReal(dbl);
		*expr = end1;
	}
	else if(end2 != NULL) {
		/* Must be an integer */
		ret = ValInt(ll);
		*expr = end2;
	}
	else {
		/* Both failed to convert the data */
		ret = ValErr(badChar(**expr));
	}
	
	return ret;
}

static Value* subscriptVector(Value* val, const char** expr, parser_cb* cb) {
	/* Move past the '[' character */
	(*expr)++;
	
	/* Parse inside of brackets */
	Value* index = Value_parse(expr, 0, ']', cb);
	if(index->type == VAL_ERR) {
		Value_free(val);
		return index;
	}
	
	/* Use builtin function from vector.c */
	TP(tp);
	return TP_FILL(tp, "@elem(@@, @@)", val, index);
}

static Value* callFunc(Value* val, const char** expr, parser_cb* cb) {
	/* Ugly, but parses better. Only variables and the results of calls can be funcs */
	if(val->type != VAL_VAR && val->type != VAL_CALL && val->type != VAL_PLACE) {
		return val;
	}
	
	/* Move past the opening parenthesis */
	(*expr)++;
	
	ArgList* args = ArgList_parse(expr, ',', ')', cb);
	if(args == NULL) {
		Value_free(val);
		return ValErr(ignoreError());
	}
	
	return ValCall(FuncCall_new(val, args));
}

static Value* parseToken(const char** expr, parser_cb* cb) {
	Value* ret;
	
	char* token = nextToken(expr);
	if(token == NULL) {
		return ValErr(badChar(**expr));
	}
	
	trimSpaces(expr);
	if(**expr == '(') {
		(*expr)++;
		ArgList* arglist = ArgList_parse(expr, ',', ')', cb);
		if(arglist == NULL) {
			/* Parse error occurred and has already been raised */
			free(token);
			return ValErr(ignoreError());
		}
		
		ret = ValCall(FuncCall_create(token, arglist));
	}
	else {
		ret = ValVar(token);
	}
	
	free(token);
	return ret;
}

Value* Value_next(const char** expr, char end, parser_cb* cb) {
	Value* ret;
	
	trimSpaces(expr);
	if(**expr == end) {
		return ValEnd();
	}
	
	if(getSign(expr) == -1) {
		return ValNeg();
	}
	
	trimSpaces(expr);
	
	
	if(isdigit(**expr) || **expr == '.') {
		ret = parseNum(expr);
	}
	else if(**expr == '(') {
		(*expr)++;
		ret = Value_parse(expr, 0, ')', cb);
	}
	else if(**expr == '<') {
		(*expr)++;
		ret = Vector_parse(expr, cb);
	}
	else if(**expr == '|') {
		(*expr)++;
		Value* val = Value_parse(expr, 0, '|', cb);
		
		/* Error checking */
		if(val->type == VAL_ERR) {
			return val;
		}
		
		/* Use absolute value builtin */
		TP(tp);
		ret = TP_FILL(tp, "@abs(@@)", val);
	}
	else if(**expr == '@') {
		/* Invoke callback to handle special value */
		ret = cb->func(expr, cb->data);
	}
	else {
		ret = parseToken(expr, cb);
	}
	
	/* Check if a parse error occurred */
	if(ret->type == VAL_ERR) {
		return ret;
	}
	
	while(1) {
		bool again = true;
		Value* tmp = NULL;
		
		trimSpaces(expr);
		switch(**expr) {
			case '[':
				tmp = subscriptVector(ret, expr, cb);
				break;
			
			case '(':
				tmp = callFunc(ret, expr, cb);
				break;
			
			default:
				again = false;
				break;
		}
		
		if(!again || tmp == ret) {
			break;
		}
		
		if(tmp->type == VAL_ERR) {
			return tmp;
		}
		
		ret = tmp;
	}
	
	/* Check for unary signs afterwards */
	while(**expr == '!') {
		(*expr)++;
		trimSpaces(expr);
		ret = ValUnary(UnOp_new(UN_FACT, ret));
	}

	return ret;
}

char* Value_repr(const Value* val, bool pretty, bool top) {
	char* ret;
	
	switch(val->type) {
		case VAL_INT:
			asprintf(&ret, "%lld", val->ival);
			break;
			
		case VAL_REAL:
			if(pretty && isinf(val->rval)) {
				ret = strdup(val->rval < 0 ? "-∞" : "∞");
			}
			else {
				asprintf(&ret, "%.*g", DBL_DIG, approx(val->rval));
			}
			break;
			
		case VAL_FRAC:
			ret = Fraction_repr(val->frac, top);
			break;
			
		case VAL_UNARY:
			ret = UnOp_repr(val->term, pretty);
			break;
			
		case VAL_EXPR:
			ret = BinOp_repr(val->expr, pretty);
			break;
			
		case VAL_CALL:
			ret = FuncCall_repr(val->call, pretty);
			break;
			
		case VAL_VAR:
			ret = strdup(pretty ? getPretty(val->name) : val->name);
			break;
			
		case VAL_VEC:
			ret = Vector_repr(val->vec, pretty);
			break;
			
		case VAL_PLACE:
			ret = Placeholder_repr(val->ph);
			break;
			
		default:
			/* Shouldn't be reached */
			badValType(val->type);
	}
	
	return ret;
}

char* Value_wrap(const Value* val, bool top) {
	char* ret;
	
	switch(val->type) {
		case VAL_INT:
			asprintf(&ret, "%lld", val->ival);
			break;
			
		case VAL_REAL:
			asprintf(&ret, "%.*g", DBL_DIG, approx(val->rval));
			break;
			
		case VAL_FRAC:
			ret = Fraction_repr(val->frac, top);
			break;
			
		case VAL_UNARY:
			ret = UnOp_wrap(val->term);
			break;
			
		case VAL_EXPR:
			ret = BinOp_wrap(val->expr);
			break;
			
		case VAL_CALL:
			ret = FuncCall_wrap(val->call);
			break;
			
		case VAL_VAR:
			ret = strdup(val->name);
			break;
			
		case VAL_VEC:
			ret = Vector_wrap(val->vec);
			break;
			
		case VAL_PLACE:
			ret = Placeholder_repr(val->ph);
			break;
			
		default:
			/* Shouldn't be reached */
			badValType(val->type);
	}
	
	return ret;
}

char* Value_verbose(const Value* val, unsigned indent) {
	char* ret;
	
	switch(val->type) {
		case VAL_INT:
			asprintf(&ret, "%lld", val->ival);
			break;
		
		case VAL_REAL:
			asprintf(&ret, "%.*g", DBL_DIG, approx(val->rval));
			break;
		
		case VAL_FRAC:
			ret = Fraction_repr(val->frac, indent == 0);
			break;
		
		case VAL_UNARY:
			ret = UnOp_verbose(val->term, indent);
			break;
		
		case VAL_EXPR:
			ret = BinOp_verbose(val->expr, indent);
			break;
		
		case VAL_CALL:
			ret = FuncCall_verbose(val->call, indent);
			break;
		
		case VAL_VAR:
			ret = strdup(val->name);
			break;
		
		case VAL_VEC:
			ret = Vector_verbose(val->vec, indent);
			break;
		
		case VAL_PLACE:
			ret = Placeholder_repr(val->ph);
			break;
		
		default:
			badValType(val->type);
	}
	
	return ret;
}

char* Value_xml(const Value* val, unsigned indent) {
	char* ret;
	
	switch(val->type) {
		case VAL_INT:
			asprintf(&ret, "<int>%lld</int>", val->ival);
			break;
			
		case VAL_REAL:
			asprintf(&ret, "<real>%.*g</real>", DBL_DIG, val->rval);
			break;
			
		case VAL_FRAC:
			ret = Fraction_xml(val->frac);
			break;
			
		case VAL_UNARY:
			ret = UnOp_xml(val->term, indent);
			break;
			
		case VAL_EXPR:
			ret = BinOp_xml(val->expr, indent);
			break;
			
		case VAL_CALL:
			ret = FuncCall_xml(val->call, indent);
			break;
			
		case VAL_VAR:
			if(val->name[0] == '@') {
				asprintf(&ret,
						 "<var name=\"%s\" internal=\"true\"/>",
						 &val->name[1]);
			}
			else {
				asprintf(&ret,
						 "<var name=\"%s\"/>",
						 val->name);
			}
			break;
			
		case VAL_VEC:
			ret = Vector_xml(val->vec, indent);
			break;
			
		case VAL_PLACE:
			ret = Placeholder_xml(val->ph, indent);
			break;
			
		default:
			badValType(val->type);
	}
	
	return ret;
}

void Value_print(const Value* val, const SuperCalc* sc, VERBOSITY v) {
	if(val->type == VAL_ERR) {
		/* An error occurred, so print it and continue. */
		Error_raise(val->err, false);
		return;
	}
	
	/* Print the value */
	char* valString = Value_repr(val, v & V_PRETTY, true);
	if(valString) {
		fprintf(sc->fout, "%s", valString);
		free(valString);
	}
	
	fputc('\n', sc->fout);
}

