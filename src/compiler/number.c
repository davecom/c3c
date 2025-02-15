// Copyright (c) 2020 Christoffer Lerno. All rights reserved.
// Use of this source code is governed by a LGPLv3.0
// a copy of which can be found in the LICENSE file.

#include "compiler_internal.h"

#define FLOAT32_LIMIT 340282346638528859811704183484516925440.0000000000000000
#define FLOAT64_LIMIT 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0000000000000000
#define FLOAT16_LIMIT 65504

void expr_const_set_int(ExprConst *expr, uint64_t v, TypeKind kind)
{
	expr->ixx.i.high = 0;
	if (type_kind_is_signed(kind))
	{
		if (v > (uint64_t)INT64_MAX) expr->ixx.i.high = UINT64_MAX;
	}
	expr->ixx.i.low = v;
	expr->ixx.type = kind;
	expr->const_kind = CONST_INTEGER;
}

void expr_const_set_bool(ExprConst *expr, bool b)
{
	expr->b = b;
	expr->const_kind = CONST_BOOL;
}

void expr_const_set_null(ExprConst *expr)
{
	expr->ixx = (Int) { .i = (Int128) { 0, 0 }, .type = type_iptr->canonical->type_kind };
	expr->const_kind = CONST_POINTER;
}

static inline bool compare_bool(bool left, bool right, BinaryOp op)
{
	switch (op)
	{
		case BINARYOP_EQ:
			return left == right;
		case BINARYOP_NE:
			return left != right;
		case BINARYOP_GT:
			return left > right;
		case BINARYOP_LT:
			return left < right;
		case BINARYOP_GE:
			return left >= right;
		case BINARYOP_LE:
			return left <= right;
		default:
			UNREACHABLE
	}
}
static inline bool compare_fps(Real left, Real right, BinaryOp op)
{
	switch (op)
	{
		case BINARYOP_GE:
			return left >= right;
		case BINARYOP_LE:
			return left <= right;
		case BINARYOP_NE:
			return left != right;
		case BINARYOP_GT:
			return left > right;
		case BINARYOP_LT:
			return left < right;
		case BINARYOP_EQ:
			return left == right;
		default:
			UNREACHABLE
	}
}

bool expr_const_compare(const ExprConst *left, const ExprConst *right, BinaryOp op)
{
	bool is_eq;

	switch (left->const_kind)
	{
		case CONST_BOOL:
			return compare_bool(left->b, right->b, op);
		case CONST_INTEGER:
			assert(right->const_kind != CONST_ENUM);
			return int_comp(left->ixx, right->ixx, op);
		case CONST_FLOAT:
			return compare_fps(left->fxx.f, right->fxx.f, op);
		case CONST_POINTER:
			return true;
		case CONST_STRING:
			if (left->string.len != right->string.len)
			{
				is_eq = false;
				break;
			}
			if (right->string.chars == left->string.chars)
			{
				is_eq = true;
				break;
			}
			is_eq = !strncmp(left->string.chars, right->string.chars, left->string.len);
			break;
		case CONST_TYPEID:
			return left->typeid == right->typeid;
		case CONST_ERR:
		case CONST_ENUM:
		{
			Decl *left_decl = left->err_val;
			// The error case
			assert(right->const_kind == left->const_kind);
			Decl *right_decl = right->err_val;
			// Non matching cannot be compared.
			if (right_decl->type != left_decl->type) return false;
			int64_t right_ordinal = right->err_val->enum_constant.ordinal;
			switch (op)
			{
				case BINARYOP_GT:
					return left_decl->enum_constant.ordinal > right_ordinal;
				case BINARYOP_GE:
					return left_decl->enum_constant.ordinal >= right_ordinal;
				case BINARYOP_LT:
					return left_decl->enum_constant.ordinal < right_ordinal;
				case BINARYOP_LE:
					return left_decl->enum_constant.ordinal <= right_ordinal;
				case BINARYOP_NE:
					return left_decl->enum_constant.ordinal != right_ordinal;
				case BINARYOP_EQ:
					return left_decl->enum_constant.ordinal == right_ordinal;
				default:
					UNREACHABLE
			}
		}
		case CONST_BYTES:
			if (left->bytes.len != right->bytes.len)
			{
				is_eq = false;
				break;
			}
			if (right->bytes.ptr == left->bytes.ptr)
			{
				is_eq = true;
				break;
			}
			is_eq = !memcmp(left->bytes.ptr, right->bytes.ptr, left->bytes.len);
			break;
		default:
			UNREACHABLE
	}
	assert((op == BINARYOP_EQ) || (op == BINARYOP_NE));
	return (op == BINARYOP_EQ) && is_eq;
}

bool float_const_fits_type(const ExprConst *expr_const, TypeKind kind)
{
	Real hi_limit;
	Real lo_limit;
	switch (kind)
	{
		case TYPE_F16:
			lo_limit = hi_limit = FLOAT16_LIMIT;
			break;
		case TYPE_F32:
			lo_limit = hi_limit = FLOAT32_LIMIT;
			break;
		case TYPE_F64:
			lo_limit = hi_limit = FLOAT64_LIMIT;
			break;
		case TYPE_F128:
			// Assume this to be true
			return true;
		case TYPE_BOOL:
			return true;
		default:
			UNREACHABLE
	}
	assert(expr_const->const_kind == CONST_FLOAT);
	return expr_const->fxx.f >= -lo_limit && expr_const->fxx.f <= hi_limit;
}

bool expr_const_will_overflow(const ExprConst *expr, TypeKind kind)
{
	switch (kind)
	{
		case ALL_INTS:
			return !int_fits(expr->ixx, kind);
		case TYPE_F16:
		case TYPE_F32:
		case TYPE_F64:
		case TYPE_F128:
			return !float_const_fits_type(expr, kind);
		case TYPE_BOOL:
			return false;
		default:
			UNREACHABLE
	}
}

const char *expr_const_to_error_string(const ExprConst *expr)
{
	char *buff = NULL;
	switch (expr->const_kind)
	{
		case CONST_POINTER:
			return "null";
		case CONST_BOOL:
			return expr->b ? "true" : "false";
		case CONST_INTEGER:
			return int_to_str(expr->ixx, 10);
		case CONST_FLOAT:
			return str_printf("%g", expr->fxx.f);
		case CONST_STRING:
			return str_printf("\"%*.s\"", expr->string.len, expr->string.chars);
		case CONST_BYTES:
			return "<binary data>";
		case CONST_ENUM:
			return expr->enum_val->name;
		case CONST_ERR:
			return expr->err_val->name;
		case CONST_TYPEID:
			return type_to_error_string(expr->typeid);
		case CONST_LIST:
			return "constant list";
	}
	UNREACHABLE
}


void expr_const_set_float(ExprConst *expr, Real d, TypeKind kind)
{
	switch (kind)
	{
		case TYPE_F32:
			expr->fxx = (Float) { (float)d, TYPE_F32 };
			break;
		case TYPE_F64:
			expr->fxx = (Float) { (double)d, TYPE_F64 };
			break;
		default:
			expr->fxx = (Float) { d, kind };
			break;
	}
	expr->const_kind = CONST_FLOAT;
}



