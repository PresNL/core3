/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "types.h"

typedef struct Buffer Buffer;
typedef struct Allocator Allocator;
typedef struct Error Error;
typedef enum EStringCase EStringCase;

//A POD list
//(Not applicable for types that resize)
//Allows both const and non const

typedef struct List {
	U8 *ptr;
	U64 stride;
	U64 length;
	U64 capacity;		//If capacity is 0 or U64_MAX, it indicates a ref
} List;

typedef enum ECompareResult {
	ECompareResult_Lt,
	ECompareResult_Eq,
	ECompareResult_Gt
} ECompareResult;

typedef ECompareResult (*CompareFunction)(const void *aPtr, const void *bPtr);

Bool List_isConstRef(List l);
Bool List_isRef(List l);

Bool List_empty(List l);
Bool List_any(List l);
U64  List_bytes(List l);
U64  List_allocatedBytes(List l);

Buffer List_buffer(List l);
Buffer List_allocatedBuffer(List l);

Buffer List_bufferConst(List l);
Buffer List_allocatedBufferConst(List l);

U8 *List_begin(List list);
U8 *List_end(List list);
U8 *List_last(List list);

const U8 *List_beginConst(List list);
const U8 *List_endConst(List list);
const U8 *List_lastConst(List list);

const U8 *List_ptrConst(List list, U64 elementOffset);
U8 *List_ptr(List list, U64 elementOffset);

Buffer List_at(List list, U64 offset);
Buffer List_atConst(List list, U64 offset);

Error List_eq(List a, List b, Bool *result);
Error List_neq(List a, List b, Bool *result);

List List_createEmpty(U64 stride);
Error List_createFromBuffer(Buffer buf, U64 stride, List *result);
Error List_createSubset(List list, U64 index, U64 length, List *result);

Error List_create(U64 length, U64 stride, Allocator allocator, List *result);
Error List_createNullBytes(U64 length, U64 stride, Allocator allocator, List *result);
Error List_createCopy(List list, Allocator allocator, List *result);

Error List_createRepeated(
	U64 length, 
	U64 stride, 
	Buffer data, 
	Allocator allocator, 
	List *result
);

Error List_createSubsetReverse(
	List list, 
	U64 index,
	U64 length, 
	Allocator allocator, 
	List *result
);

Error List_createReverse(List list, Allocator allocator, List *result);

Error List_createRef(U8 *ptr, U64 length, U64 stride, List *result);
Error List_createConstRef(const U8 *ptr, U64 length, U64 stride, List *result);

Error List_set(List list, U64 index, Buffer buf);
Error List_get(List list, U64 index, Buffer *result);
Error List_getConst(List list, U64 index, Buffer *result);

Error List_copy(
	List src, 
	U64 srcOffset, 
	List dst, 
	U64 dstOffset, 
	U64 count
);

Error List_swap(List l, U64 i, U64 j);
Bool List_reverse(List l);

//Find all occurrences in list
//Returns U64[]
//
Error List_find(List list, Buffer buf, Allocator allocator, List *result);

U64 List_findFirst(List list, Buffer buf, U64 index);
U64 List_findLast(List list, Buffer buf, U64 index);
U64 List_count(List list, Buffer buf);

Bool List_contains(List list, Buffer buf, U64 offset);

Error List_eraseFirst(List *list, Buffer buf, U64 offset);
Error List_eraseLast(List *list, Buffer buf, U64 offset);
Error List_eraseAll(List *list, Buffer buf, Allocator allocator);
Error List_erase(List *list, U64 index);
Error List_eraseAllIndices(List *list, List indices);			//Sorts indices (U64[])
Error List_insert(List *list, U64 index, Buffer buf, Allocator allocator);
Error List_pushAll(List *list, List other, Allocator allocator);
Error List_insertAll(List *list, List other, U64 offset, Allocator allocator);

Error List_reserve(List *list, U64 capacity, Allocator allocator);
Error List_resize(List *list, U64 size, Allocator allocator);

Error List_shrinkToFit(List *list, Allocator allocator);

Bool List_sortU64(List list);
Bool List_sortU32(List list);
Bool List_sortU16(List list);
Bool List_sortU8(List list);

Bool List_sortI64(List list);
Bool List_sortI32(List list);
Bool List_sortI16(List list);
Bool List_sortI8(List list);

Bool List_sortF32(List list);

Bool List_sortString(List list, EStringCase stringCase);

Bool List_sortCustom(List list, CompareFunction func);

//Expects buf to be sized to stride (to allow copying to the stack)

Error List_popBack(List *list, Buffer output);
Error List_popFront(List *list, Buffer output);
Error List_popLocation(List *list, U64 index, Buffer buf);

Error List_pushBack(List *list, Buffer buf, Allocator allocator);
Error List_pushFront(List *list, Buffer buf, Allocator allocator);

Error List_clear(List *list);		//Doesn't remove data, only makes it unavailable

Bool List_free(List *result, Allocator allocator);
