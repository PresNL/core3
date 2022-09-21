#pragma once
#include "math/vec.h"

struct BitRef {
	U8 *ptr, off;
};

inline Bool BitRef_get(struct BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
inline void BitRef_set(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }
inline void BitRef_reset(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }

inline void BitRef_setTo(struct BitRef b, Bool v) { 

	if(!b.ptr) return;

	if(v) 
		BitRef_set(b);

	else BitRef_reset(b);
}

struct Error Bit_get(struct Buffer buf, U64 offset, Bool *output);

void Bit_copy(struct Buffer dst, struct Buffer src);
void Bit_revCopy(struct Buffer dst, struct Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

struct Error Bit_set(struct Buffer buf, U64 offset);
struct Error Bit_reset(struct Buffer buf, U64 offset);

inline struct Error Bit_setTo(struct Buffer buf, U64 offset, Bool value) {

	if (!value)
		return Bit_reset(buf, offset);

	return Bit_set(buf, offset);
}

struct Error Bit_or(struct Buffer dst, struct Buffer src);
struct Error Bit_and(struct Buffer dst, struct Buffer src);
struct Error Bit_xor(struct Buffer dst, struct Buffer src);
struct Error Bit_not(struct Buffer dst);

struct Error Bit_setRange(struct Buffer dst, U64 dstOff, U64 bits);
struct Error Bit_unsetRange(struct Buffer dst, U64 dstOff, U64 bits);

struct Error Bit_setAll(struct Buffer dst);
struct Error Bit_unsetAll(struct Buffer dst);

inline struct Error Bit_setAllTo(struct Buffer buf, Bool isOn) {

	if (isOn)
		return Bit_setAll(buf);

	return Bit_unsetAll(buf);
}

U64 Bit_hash(struct Buffer buf);

//Comparison

struct Error Bit_cmp(struct Buffer buf0, struct Buffer buf1, I8 *result);		//Returns -1: <, 0: ==, 1: >
struct Error Bit_eq(struct Buffer buf0, struct Buffer buf1, Bool *result);		//Also compares size
struct Error Bit_neq(struct Buffer buf0, struct Buffer buf1, Bool *result);		//Also compares size

inline struct Error Bit_lt(struct Buffer buf0, struct Buffer buf1, Bool *result) { 
	I8 res = 0;
	struct Error e = Bit_cmp(buf0, buf1, &res);
	*result = res < 0;
	return e;
}

struct Error Bit_gt(struct Buffer buf0, struct Buffer buf1, Bool *result) { 
	I8 res = 0;
	struct Error e = Bit_cmp(buf0, buf1, &res);
	*result = res > 0;
	return e;
}

//All these functions allocate, so Bit_free them later

struct Buffer Bit_createRef(void *v, U64 len) { return (struct Buffer) { .ptr = (U8*) v, .siz = len }; }

struct Error Bit_createDuplicate(struct Buffer buf, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createEmpty(U64 siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createFull(U64 siz, struct Allocator alloc, struct Buffer *output);

inline struct Error Bit_createFilled(U64 siz, Bool value, struct Allocator alloc, struct Buffer *result) {

	if (!value)
		return Bit_createEmpty(siz, alloc, result);

	return Bit_createFull(siz, alloc, result);
}

struct Error Bit_free(struct Buffer *buf, struct Allocator alloc);
struct Error Bit_createEmptyBytes(U64 siz, struct Allocator alloc, struct Buffer *output);

struct Error Bit_createBytes(U64 siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createSubset(struct Buffer buf, U64 offset, U64 siz, struct Buffer *output);

//Writing data

struct Error Bit_offset(struct Buffer *buf, U64 siz);
struct Error Bit_append(struct Buffer *buf, const void *v, U64 siz);
struct Error Bit_appendBuffer(struct Buffer *buf, struct Buffer append);

inline struct Error Bit_appendU64(struct Buffer *buf, U64 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendU32(struct Buffer *buf, U32 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendU16(struct Buffer *buf, U16 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendU8(struct Buffer *buf, U8 v) { return Bit_append(buf, &v, sizeof(v)); }

inline struct Error Bit_appendI64(struct Buffer *buf, I64 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendI32(struct Buffer *buf, I32 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendI16(struct Buffer *buf, I16 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendI8(struct Buffer *buf, I8 v) { return Bit_append(buf, &v, sizeof(v)); }

inline struct Error Bit_appendF64(struct Buffer *buf, F32 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendF32(struct Buffer *buf, F32 v) { return Bit_append(buf, &v, sizeof(v)); }

inline struct Error Bit_appendF32x4(struct Buffer *buf, F32x4 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendF32x2(struct Buffer *buf, I32x2 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendI32x4(struct Buffer *buf, I32x4 v) { return Bit_append(buf, &v, sizeof(v)); }
inline struct Error Bit_appendI32x2(struct Buffer *buf, I32x2 v) { return Bit_append(buf, &v, sizeof(v)); }
