/*
 *  The implementation of cstl types.
 *  Copyright (C)  2008 2009  Wangbo
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Author e-mail: activesys.wb@gmail.com
 *                 activesys@sina.com.cn
 */

/** include section **/
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>

#include "cstl_alloc.h"
#include "cstl_types.h"

#include "cvector.h"
#include "clist.h"
#include "cslist.h"
#include "cdeque.h"
#include "cstack.h"
#include "cqueue.h"
#include "cset.h"
#include "cmap.h"
#include "chash_set.h"
#include "chash_map.h"
#include "cstring.h"
#include "cutility.h"

/** local constant declaration and local macro section **/
/* the pt_type, pt_node and t_pos must be defined before use those macro */
#define _TYPE_REGISTER_BEGIN()\
    _typenode_t* pt_node = NULL;\
    _type_t*     pt_type = NULL;\
    size_t       t_pos = 0
#define _TYPE_REGISTER_TYPE(type, type_text, type_suffix)\
    do{\
        pt_type = (_type_t*)allocate(&_gt_typeregister._t_allocator, sizeof(_type_t), 1);\
        assert(pt_type != NULL);\
        pt_type->_t_typesize = sizeof(type);\
        memset(pt_type->_sz_typename, '\0', _TYPE_NAME_SIZE+1);\
        strncpy(pt_node->_sz_typename, type_text, _TYPE_NAME_SIZE);\
        pt_type->_t_typecopy = _type_copy_##type_suffix;\
        pt_type->_t_typeless = _type_less_##type_suffix;\
        pt_type->_t_typedestroy = _type_destroy_##type_suffix;\
    }while(false)
#define _TYPE_REGISTER_TYPE_NODE(type, type_text)\
    do{\
        pt_node = (_typenode_t*)allocate(\
            &_gt_typeregister._t_allocator, sizeof(_typenode_t), 1);\
        assert(pt_node != NULL);\
        memset(pt_node->_sz_typename, '\0', _TYPE_NAME_SIZE+1);\
        strncpy(pt_node->_sz_typename, type_text, _TYPE_NAME_SIZE);\
        t_pos = _type_hash(sizeof(type), type_text);\
        pt_node->_pt_next = _gt_typeregister._apt_bucket[t_pos];\
        _gt_typeregister._apt_bucket[t_pos] = pt_node;\
        pt_node->_pt_type = pt_type;\
    }while(false)
#define _TYPE_REGISTER_END()
/*
 * _gt_typeregister
 * +--------------------------------------------------------------------+
 * |     |    |    |    |    |  ...  |    |    |    |    |    |    |    |  
 * +-------+--------------------------------+----+----------------------+
 *         |                                |    |
 *         V                                V    V
 *      +-------------+            +--------+    +----------+
 *      | _typenode_t |            |        |    |          |
 *      +--+----------+            +--------+    +----------+
 *         |                                |    |
 *         V                                V    V
 *      +-------------+                  NULL    +----------+
 *      | "abc_t"     | major name       +-------|"my_abc_t"| duplicated name
 *      +--+----------+                  |       +----------+
 *         |                             |       |
 *         V                             |       V
 *      +-------------+                  |       NULL
 *      |             |----------+-------+ 
 *      +--+----------+          |
 *         |                     |
 *         V                     |
 *         NULL                  V
 *                             +------------------------------+
 *                             | _t_typesize = ???            |
 *                             | _sz_typename = "abc_t"       |
 *                             | _t_typecopy = abc_copy       | "registered type abc_t"
 *                             | _t_typeless = abc_less       |
 *                             | _t_typedestroy = abc_destroy |
 *                             +------------------------------+
 */
static _typeregister_t _gt_typeregister = {false, {NULL}, {{NULL}, NULL, NULL, 0, 0, 0}};

/** local data type declaration and local struct, union, enum section **/
typedef enum _tagtypestley
{
    _TYPE_INVALID, _TYPE_C_BUILTIN, _TYPE_USER_DEFINE, _TYPE_CSTL_CONTAINER
}_typestyle_t;

typedef enum _tagtypetoken
{
    /* invalid token */
    _TOKEN_INVALID,
    /* EOI */
    _TOKEN_END_OF_INPUT,
    /* c builtin */
    _TOKEN_KEY_CHAR, _TOKEN_KEY_SHORT, _TOKEN_KEY_INT, _TOKEN_KEY_LONG, _TOKEN_KEY_FLOAT,
    _TOKEN_KEY_DOUBLE, _TOKEN_KEY_SIGNED, _TOKEN_KEY_UNSIGNED, _TOKEN_KEY_CHAR_POINTER,
    _TOKEN_KEY_BOOL,
    /* user define */
    _TOKEN_KEY_STRUCT, _TOKEN_KEY_ENUM, _TOKEN_KEY_UNION, _TOKEN_IDENTIFIER,
    /* cstl container */
    _TOKEN_KEY_VECTOR, _TOKEN_KEY_LIST, _TOKEN_KEY_SLIST, _TOKEN_KEY_DEQUE, _TOKEN_KEY_STACK,
    _TOKEN_KEY_QUEUE, _TOKEN_KEY_PRIORITY_QUEUE, _TOKEN_KEY_SET, _TOKEN_KEY_MAP,
    _TOKEN_KEY_MULTISET, _TOKEN_KEY_MULTIMAP, _TOKEN_KEY_HASH_SET, _TOKEN_KEY_HASH_MAP,
    _TOKEN_KEY_HASH_MULTISET, _TOKEN_KEY_HASH_MULTIMAP, _TOKEN_KEY_PAIR, _TOKEN_KEY_STRING,
    /* sign */
    _TOKEN_SIGN_LEFT_BRACKET, _TOKEN_SIGN_RIGHT_BRACKET, _TOKEN_SIGN_COMMA, _TOKEN_SIGN_SPACE,
    /* empty */
    _TOKEN_EMPTY
}_typetoken_t;

typedef struct _tagtypeanalysis
{
    char         _sz_typename[_TYPE_NAME_SIZE+1];
    size_t       _t_index;
    _typetoken_t _t_token;
}_typeanalysis_t;
_typeanalysis_t _gt_typeanalysis = {{'\0'}, 0, _TOKEN_INVALID};

#define _TOKEN_MATCH(token) true

#define _TOKEN_TEXT_CHAR           "char"
#define _TOKEN_TEXT_SHORT          "short"
#define _TOKEN_TEXT_INT            "int"
#define _TOKEN_TEXT_LONG           "long"
#define _TOKEN_TEXT_FLOAT          "float"
#define _TOKEN_TEXT_DOUBLE         "double"
#define _TOKEN_TEXT_SIGNED         "signed"
#define _TOKEN_TEXT_UNSIGNED       "unsigned"
#define _TOKEN_TEXT_CHAR_POINTER   "char*"
#define _TOKEN_TEXT_BOOL           "bool_t"
#define _TOKEN_TEXT_STRUCT         "struct"
#define _TOKEN_TEXT_ENUM           "enum"
#define _TOKEN_TEXT_UNION          "union"
#define _TOKEN_TEXT_VECTOR         "vector_t"
#define _TOKEN_TEXT_LIST           "list_t"
#define _TOKEN_TEXT_SLIST          "slist_t"
#define _TOKEN_TEXT_DEQUE          "deque_t"
#define _TOKEN_TEXT_STACK          "stack_t"
#define _TOKEN_TEXT_QUEUE          "queue_t"
#define _TOKEN_TEXT_PRIORITY_QUEUE "priority_queue_t"
#define _TOKEN_TEXT_SET            "set_t"
#define _TOKEN_TEXT_MAP            "map_t"
#define _TOKEN_TEXT_MULTISET       "multiset_t"
#define _TOKEN_TEXT_MULTIMAP       "multimap_t"
#define _TOKEN_TEXT_HASH_SET       "hash_set_t"
#define _TOKEN_TEXT_HASH_MAP       "hash_map_t"
#define _TOKEN_TEXT_HASH_MULTISET  "hash_multiset_t"
#define _TOKEN_TEXT_HASH_MULTIMAP  "hash_multimap_t"
#define _TOKEN_TEXT_PAIR           "pair_t"
#define _TOKEN_TEXT_STRING         "string_t"
#define _TOKEN_TEXT_SPACE          " "

typedef enum _tagtypelex
{
    _LEX_START, _LEX_IN_IDENTIFIER, _LEX_IN_SPACE, _LEX_ACCEPT
}_typelex_t;

/** local function prototype section **/
/******************************* destroy in the future *****************************/
/* default compare function */
static int _cmp_char(const void* cpv_first, const void* cpv_second);
static int _cmp_uchar(const void* cpv_first, const void* cpv_second);
static int _cmp_short(const void* cpv_first, const void* cpv_second);
static int _cmp_ushort(const void* cpv_first, const void* cpv_second);
static int _cmp_int(const void* cpv_first, const void* cpv_second);
static int _cmp_uint(const void* cpv_first, const void* cpv_second);
static int _cmp_long(const void* cpv_first, const void* cpv_second);
static int _cmp_ulong(const void* cpv_first, const void* cpv_second);
static int _cmp_float(const void* cpv_first, const void* cpv_second);
static int _cmp_double(const void* cpv_first, const void* cpv_second);
/******************************* destroy in the future *****************************/

/* register hash function */
static size_t _type_hash(size_t t_typesize, const char* s_typename);
/* init the register and register c builtin type and cstl builtin type */
static void _type_init(void);
/* test the type is registered or not */
static bool_t _type_is_registered(size_t t_typesize, const char* s_typename);
/* normalize the typename, test the typename is valid or not and get the type style */
static _typestyle_t _type_get_style(const char* s_typename);
/* register c builtin and cstl container */
static void _type_register_c_builtin(void);
static void _type_register_cstl_container(void);

/* the functions blow is used for analyse the type style */
static _typetoken_t _type_get_token(
    const char* s_input, char* s_tokentext, size_t* pt_tokenlen);
static bool_t _type_parse_c_builtin(
    const char* s_input, _typetoken_t t_token, char* s_formalname);

/*
 * the cstl builtin copy, compare destroy function for c builtin type and cstl containers.
 */
/* c builtin */
/* char */
static void _type_copy_char(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_char(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_char(
    const void* cpv_input, void* pv_output);
/* unsigned char */
static void _type_copy_uchar(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_uchar(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_uchar(
    const void* cpv_input, void* pv_output);
/* short */
static void _type_copy_short(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_short(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_short(
    const void* cpv_input, void* pv_output);
/* unsigned short */
static void _type_copy_ushort(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_ushort(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_ushort(
    const void* cpv_input, void* pv_output);
/* int */
static void _type_copy_int(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_int(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_int(
    const void* cpv_input, void* pv_output);
/* unsigned int */
static void _type_copy_uint(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_uint(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_uint(
    const void* cpv_input, void* pv_output);
/* long */
static void _type_copy_long(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_long(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_long(
    const void* cpv_input, void* pv_output);
/* unsigned long */
static void _type_copy_ulong(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_ulong(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_ulong(
    const void* cpv_input, void* pv_output);
/* float */
static void _type_copy_float(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_float(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_float(
    const void* cpv_input, void* pv_output);
/* double */
static void _type_copy_double(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_double(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_double(
    const void* cpv_input, void* pv_output);
/* long double */
static void _type_copy_long_double(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_long_double(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_long_double(
    const void* cpv_input, void* pv_output);
/* bool_t */
static void _type_copy_bool(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_bool(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_bool(
    const void* cpv_input, void* pv_output);
/* char* */
static void _type_copy_cstr(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_cstr(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_cstr(
    const void* cpv_input, void* pv_output);
/* cstl container */
/* vector_t */
static void _type_copy_vector(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_vector(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_vector(
    const void* cpv_input, void* pv_output);
/* list_t */
static void _type_copy_list(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_list(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_list(
    const void* cpv_input, void* pv_output);
/* slist_t */
static void _type_copy_slist(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_slist(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_slist(
    const void* cpv_input, void* pv_output);
/* deque_t */
static void _type_copy_deque(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_deque(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_deque(
    const void* cpv_input, void* pv_output);
/* stack_t */
static void _type_copy_stack(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_stack(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_stack(
    const void* cpv_input, void* pv_output);
/* queue_t */
static void _type_copy_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_queue(
    const void* cpv_input, void* pv_output);
/* priority_queue_t */
static void _type_copy_priority_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_priority_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_priority_queue(
    const void* cpv_input, void* pv_output);
/* set_t */
static void _type_copy_set(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_set(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_set(
    const void* cpv_input, void* pv_output);
/* map_t */
static void _type_copy_map(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_map(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_map(
    const void* cpv_input, void* pv_output);
/* multiset_t */
static void _type_copy_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_multiset(
    const void* cpv_input, void* pv_output);
/* multimap_t */
static void _type_copy_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_multimap(
    const void* cpv_input, void* pv_output);
/* hash_set_t */
static void _type_copy_hash_set(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_hash_set(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_hash_set(
    const void* cpv_input, void* pv_output);
/* hash_map_t */
static void _type_copy_hash_map(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_hash_map(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_hash_map(
    const void* cpv_input, void* pv_output);
/* hash_multiset_t */
static void _type_copy_hash_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_hash_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_hash_multiset(
    const void* cpv_input, void* pv_output);
/* hash_multimap_t */
static void _type_copy_hash_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_hash_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_hash_multimap(
    const void* cpv_input, void* pv_output);
/* pair_t */
static void _type_copy_pair(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_pair(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_pair(
    const void* cpv_input, void* pv_output);
/* string_t */
static void _type_copy_string(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_less_string(
    const void* cpv_first, const void* cpv_second, void* pv_output);
static void _type_destroy_string(
    const void* cpv_input, void* pv_output);

/** exported global variable definition section **/

/** local global variable definition section **/

/** exported function implementation section **/
bool_t _type_register(
    size_t t_typesize, const char* s_typename,
    binary_function_t t_typecopy,
    binary_function_t t_typeless,
    unary_function_t t_typedestroy)
{
    if(!_gt_typeregister._t_isinit)
    {
        _type_init();
    }

    /* test the typename is valid */
    /* if the type is registered */
    if(_type_is_registered(t_typesize, s_typename) || strlen(s_typename) > _TYPE_NAME_SIZE)
    {
         return false;
    }
    /* else */
    else
    {
        size_t t_pos = 0;
        _typenode_t* pt_node = (_typenode_t*)allocate(
            &_gt_typeregister._t_allocator, sizeof(_typenode_t), 1);
        _type_t* pt_type = (_type_t*)allocate(
            &_gt_typeregister._t_allocator, sizeof(_type_t*), 1);

        memset(pt_node->_sz_typename, '\0', _TYPE_NAME_SIZE+1);
        memset(pt_type->_sz_typename, '\0', _TYPE_NAME_SIZE+1);

        /* register the new type */
        strncpy(pt_node->_sz_typename, s_typename, _TYPE_NAME_SIZE);
        strncpy(pt_type->_sz_typename, s_typename, _TYPE_NAME_SIZE);
        pt_type->_t_typesize = t_typesize;
        pt_type->_t_typecopy = t_typecopy;
        pt_type->_t_typeless = t_typeless;
        pt_type->_t_typedestroy = t_typedestroy;

        pt_node->_pt_type = pt_type;
        t_pos = _type_hash(t_typesize, s_typename);
        pt_node->_pt_next = _gt_typeregister._apt_bucket[t_pos];
        _gt_typeregister._apt_bucket[t_pos] = pt_node;

        return true;
    }
}

void _type_unregister(size_t t_typesize, const char* s_typename)
{
    _typenode_t* pt_node = NULL;
    _type_t*     pt_type = NULL;

    if(strlen(s_typename) > _TYPE_NAME_SIZE)
    {
        return;
    }

    /* get the registered type pointer */
    pt_node = _gt_typeregister._apt_bucket[_type_hash(t_typesize, s_typename)];
    while(pt_node != NULL)
    {
        if(strncmp(pt_node->_sz_typename, s_typename, _TYPE_NAME_SIZE) == 0)
        {
            pt_type = pt_node->_pt_type;
            assert(pt_type != NULL);
            assert(pt_type->_t_typesize == t_typesize);
            break;
        }
        else
        {
            pt_node = pt_node->_pt_next;
        }
    }

    if(pt_type != NULL)
    {
        _typenode_t* pt_curnode = NULL;
        _typenode_t* pt_prevnode = NULL;
        size_t       t_i = 0;

        for(t_i = 0; t_i < _TYPE_REGISTER_BUCKET_COUNT; ++t_i)
        {
            pt_curnode = pt_prevnode = _gt_typeregister._apt_bucket[t_i];
            while(pt_curnode != NULL)
            {
                if(pt_curnode->_pt_type == pt_type)
                {
                    if(pt_curnode == _gt_typeregister._apt_bucket[t_i])
                    {
                        _gt_typeregister._apt_bucket[t_i] = pt_curnode->_pt_next;
                        deallocate(&_gt_typeregister._t_allocator,
                            pt_curnode, sizeof(_typenode_t), 1);
                        pt_curnode = pt_prevnode = _gt_typeregister._apt_bucket[t_i];
                    }
                    else
                    {
                        assert(pt_prevnode->_pt_next == pt_curnode);
                        pt_prevnode->_pt_next = pt_curnode->_pt_next;
                        deallocate(&_gt_typeregister._t_allocator,
                            pt_curnode, sizeof(_typenode_t), 1);
                        pt_curnode = pt_prevnode->_pt_next;
                    }
                }
                else
                {
                    if(pt_curnode == _gt_typeregister._apt_bucket[t_i])
                    {
                        assert(pt_curnode == pt_prevnode);
                        pt_curnode = pt_curnode->_pt_next;
                    }
                    else
                    {
                        assert(pt_curnode == pt_prevnode->_pt_next);
                        pt_prevnode = pt_curnode;
                        pt_curnode = pt_curnode->_pt_next;
                    }
                }
            }
        }

        deallocate(&_gt_typeregister._t_allocator, pt_type, sizeof(_type_t), 1);
    }
}

bool_t _type_duplicate(
    size_t t_typesize1, const char* s_typename1,
    size_t t_typesize2, const char* s_typename2)
{
    bool_t t_registered1 = false;
    bool_t t_registered2 = false;

    if(!_gt_typeregister._t_isinit)
    {
        _type_init();
    }

    if(strlen(s_typename1) > _TYPE_NAME_SIZE ||
       strlen(s_typename2) > _TYPE_NAME_SIZE ||
       t_typesize1 != t_typesize2)
    {
        return false;
    }

    /* test the type1 and type2 is registered or not */
    t_registered1 = _type_is_registered(t_typesize1, s_typename1);
    t_registered2 = _type_is_registered(t_typesize2, s_typename2);

    /* type1 and type2 all unregistered */
    if(!t_registered1 && !t_registered2)
    {
        return false;
    }
    /* type1 and type2 all registered */
    else if(t_registered1 && t_registered2)
    {
        _typenode_t* pt_node1 = NULL;
        _typenode_t* pt_node2 = NULL;
        _type_t*     pt_type1 = NULL;
        _type_t*     pt_type2 = NULL;

        pt_node1 = _gt_typeregister._apt_bucket[_type_hash(t_typesize1, s_typename1)];
        pt_node2 = _gt_typeregister._apt_bucket[_type_hash(t_typesize2, s_typename2)]; 
        assert(pt_node1 != NULL && pt_node2 != NULL);

        while(pt_node1 != NULL)
        {
            if(strncmp(pt_node1->_sz_typename, s_typename1, _TYPE_NAME_SIZE) == 0)
            {
                pt_type1 = pt_node1->_pt_type;
                assert(pt_type1 != NULL);
                assert(pt_type1->_t_typesize == t_typesize1);
                break;
            }
            else
            {
                pt_node1 = pt_node1->_pt_next;
            }
        }
        assert(pt_node1 != NULL && pt_type1 != NULL);

        while(pt_node2 != NULL)
        {
            if(strncmp(pt_node2->_sz_typename, s_typename2, _TYPE_NAME_SIZE) == 0)
            {
                pt_type2 = pt_node2->_pt_type;
                assert(pt_type2 != NULL);
                assert(pt_type2->_t_typesize == t_typesize2);
                break;
            }
            else
            {
                pt_node2 = pt_node2->_pt_next;
            }
        }
        assert(pt_node2 != NULL && pt_type2 != NULL);

        /* is same registered type */
        if(pt_type1 == pt_type2)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    /* only one type is registered */
    else
    {
        size_t       t_typesize = t_typesize1;
        size_t       t_pos = 0;
        char*        s_registeredname = NULL;
        char*        s_duplicatename = NULL;
        _typenode_t* pt_registered = NULL;
        _typenode_t* pt_duplicate = NULL;
        _type_t*     pt_type = NULL;

        /* type1 is registered and type2 is unregistered */
        if(t_registered1 && !t_registered2)
        {
            s_registeredname = (char*)s_typename1;
            s_duplicatename = (char*)s_typename2;
        }
        /* type1 is unregistered and type2 is registered */
        else
        {
            s_registeredname = (char*)s_typename2;
            s_duplicatename = (char*)s_typename1;
        }

        /* get the registered type pointer */
        pt_registered = _gt_typeregister._apt_bucket[_type_hash(t_typesize, s_registeredname)];
        assert(pt_registered != NULL);
        while(pt_registered != NULL)
        {
            if(strncmp(pt_registered->_sz_typename, s_registeredname, _TYPE_NAME_SIZE) == 0)
            {
                pt_type = pt_registered->_pt_type;
                assert(pt_type != NULL);
                assert(pt_type->_t_typesize == t_typesize);
                break;
            }
            else
            {
                pt_registered = pt_registered->_pt_next;
            }
        }
        assert(pt_registered != NULL && pt_type != NULL);

        /* malloc typenode for unregistered type */
        pt_duplicate = (_typenode_t*)allocate(
            &_gt_typeregister._t_allocator, sizeof(_typenode_t), 1);
        memset(pt_duplicate->_sz_typename, '\0', _TYPE_NAME_SIZE+1);
        strncpy(pt_duplicate->_sz_typename, s_duplicatename, _TYPE_NAME_SIZE);

        pt_duplicate->_pt_type = pt_type;

        t_pos = _type_hash(t_typesize, s_duplicatename);
        pt_duplicate->_pt_next = _gt_typeregister._apt_bucket[t_pos];
        _gt_typeregister._apt_bucket[t_pos] = pt_duplicate;

        return true;
    }
}

/* default copy, less, and destroy function */
void _type_copy_default(const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    memcpy((void*)cpv_first, cpv_second, *(size_t*)pv_output);
    *(size_t*)pv_output = true;
}
void _type_less_default(const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(size_t*)pv_output = memcmp(cpv_first, cpv_second, *(size_t*)pv_output) == 0 ?
                          true : false;
}
void _type_destroy_default(const void* cpv_input, void* pv_output)
{
    void* pv_avoidwarning = NULL;
    assert(cpv_first != NULL && pv_output != NULL);
    pv_avoidwarning = (void*)cpv_input;
    *(bool_t*)pv_output = true;
}

static bool_t _type_is_registered(size_t t_typesize, const char* s_typename)
{
    _type_t*     pt_registered = NULL;
    _typenode_t* pt_node = NULL;

    if(strlen(s_typename) > _TYPE_NAME_SIZE)
    {
        return false;
    }

    /* get the registered type pointer */
    pt_node = _gt_typeregister._apt_bucket[_type_hash(t_typesize, s_typename)];
    if(pt_node != NULL)
    {
        while(pt_node != NULL)
        {
            if(strncmp(s_typename, pt_node->_sz_typename, _TYPE_NAME_SIZE) == 0)
            {
                pt_registered = pt_node->_pt_type;
                assert(pt_registered != NULL);
                assert(t_typesize == pt_registered->_t_typesize);
                break;
            }
            else
            {
                pt_node = pt_node->_pt_next;
            }
        }

        if(pt_registered != NULL)
        {
            assert(pt_node != NULL);
            return true;
        }
        else
        {
            assert(pt_node == NULL);
            return false;
        }
    }
    else
    {
        return false;
    }
}

static size_t _type_hash(size_t t_typesize, const char* s_typename)
{
    size_t t_namesum = 0;
    size_t t_namelen = strlen(s_typename);
    size_t t_i = 0;

    t_typesize = 0; /* avoid warning */
    for(t_i = 0; t_i < t_namelen; ++t_i)
    {
        t_namesum += (size_t)s_typename[t_i];
    }

    return t_namesum % _TYPE_REGISTER_BUCKET_COUNT;
}

static _typestyle_t _type_get_style(const char* s_typename)
{
    /*
     * this parser algorithm is associated with BNF in cstl.bnf that is issured by
     * activesys.cublog.cn
     */
    char   s_formalname[_TYPE_NAME_SIZE + 1];
    char   s_tokentext[_TYPE_NAME_SIZE + 1];
    char   s_userdefine[_TYPE_NAME_SIZE + 1];
    size_t t_index = 0;
    size_t t_tokenlen = 0;
    _typestyle_t t_style = _TYPE_INVALID;

    if(strlen(s_typename) > _TYPE_NAME_SIZE)
    {
        return _TYPE_INVALID;
    }

    memset(s_formalname, '\0', _TYPE_NAME_SIZE+1);
    memset(s_tokentext, '\0', _TYPE_NAME_SIZE+1);
    memset(s_userdefine, '\0', _TYPE_NAME_SIZE+1);

    /* initialize the type analysis */
    memset(_gt_typeanalysis._sz_typename, '\0', _TYPE_NAME_SIZE+1);
    _gt_typeanalysis._t_tokenindex = 0;
    _gt_typeanalysis._t_token = _TOKEN_INVALID;
    strncpy(_gt_typeanalysis._sz_typename, s_typename, _TYPE_NAME_SIZE);

    /* TYPE_DESCRIPT -> C_BUILTIN | USER_DEFINE | CSTL_CONTAINER */
    _type_get_token_skip_space(s_tokentext);
    switch(t_token)
    {
    /* TYPE_DESCRIPT -> C_BUILTIN */
    case _TOKEN_KEY_CHAR:
    case _TOKEN_KEY_SHORT:
    case _TOKEN_KEY_INT:
    case _TOKEN_KEY_LONG:
    case _TOKEN_KEY_FLOAT:
    case _TOKEN_KEY_DOUBLE:
    case _TOKEN_KEY_SIGNED:
    case _TOKEN_KEY_UNSIGNED:
    case _TOKEN_KEY_CHAR_POINTER:
    case _TOKEN_KEY_BOOL:
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        t_style = _type_parse_c_builtin(s_formalname) ?
                  _TYPE_C_BUILTIN : _TYPE_INVALID;
        break;
    /* TYPE_DESCRIPT -> USER_DEFINE */
    case _TOKEN_KEY_STRUCT:
    case _TOKEN_KEY_ENUM:
    case _TOKEN_KEY_UNION:
    case _TOKEN_IDENTIFIER:
        strncat(s_userdefine, s_tokentext, _TYPE_NAME_SIZE);
        if(_type_parse_user_define(s_userdefine))
        {
            if(_type_is_registered(s_userdefine))
            {
                strncat(s_formalname, s_userdefine, _TYPE_NAME_SIZE);
                t_style = _TYPE_USER_DEFINE;
            }
            else
            {
                t_style = _TYPE_INVALID;
            }
        }
        else
        {
            t_style = _TYPE_INVALID;
        }
        break;
    /* TYPE_DESCRIPT -> CSTL_CONTAINER */
    case _TOKEN_KEY_VECTOR:
    case _TOKEN_KEY_LIST:
    case _TOKEN_KEY_SLIST:
    case _TOKEN_KEY_DEQUE:
    case _TOKEN_KEY_STACK:
    case _TOKEN_KEY_QUEUE:
    case _TOKEN_KEY_PRIORITY_QUEUE:
    case _TOKEN_KEY_SET:
    case _TOKEN_KEY_MAP:
    case _TOKEN_KEY_MULTISET:
    case _TOKEN_KEY_MULTIMAP:
    case _TOKEN_KEY_HASH_SET:
    case _TOKEN_KEY_HASH_MAP:
    case _TOKEN_KEY_HASH_MULTISET:
    case _TOKEN_KEY_HASH_MULTIMAP:
    case _TOKEN_KEY_PAIR:
    case _TOKEN_KEY_STRING:
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        t_style = _type_parse_cstl_container(s_formalname) ?
                  _TYPE_CSTL_CONTAINER : _TYPE_INVALID;
        break;
    default:
        t_style = _TYPE_INVALID;
        break;
    }

    return t_style;
}

static bool_t _type_parse_type_descript(
    const char* s_input, _typetoken_t t_token, char* s_formalname, const char* s_tokentext)
{
    char   s_userdefine[_TYPE_NAME_SIZE + 1];

    memset(s_userdefine, '\0', _TYPE_NAME_SIZE+1);

    /* TYPE_DESCRIPT -> C_BUILTIN | USER_DEFINE | CSTL_CONTAINER */
    switch(t_token)
    {
    /* TYPE_DESCRIPT -> C_BUILTIN */
    case _TOKEN_KEY_CHAR:
    case _TOKEN_KEY_SHORT:
    case _TOKEN_KEY_INT:
    case _TOKEN_KEY_LONG:
    case _TOKEN_KEY_FLOAT:
    case _TOKEN_KEY_DOUBLE:
    case _TOKEN_KEY_SIGNED:
    case _TOKEN_KEY_UNSIGNED:
    case _TOKEN_KEY_CHAR_POINTER:
    case _TOKEN_KEY_BOOL:
        return _type_parse_c_builtin(s_input, t_token, s_formalname);
        break;
    /* TYPE_DESCRIPT -> USER_DEFINE */
    case _TOKEN_KEY_STRUCT:
    case _TOKEN_KEY_ENUM:
    case _TOKEN_KEY_UNION:
    case _TOKEN_IDENTIFIER:
        strncat(s_userdefine, s_tokentext, _TYPE_NAME_SIZE);
        if(_type_parse_user_define(s_input, t_token, s_userdefine))
        {
            strncat(s_formalname, s_userdefine, _TYPE_NAME_SIZE);
            return _type_is_registered(s_userdefine);
        }
        else
        {
            return false;
        }
        break;
    /* TYPE_DESCRIPT -> CSTL_CONTAINER */
    case _TOKEN_KEY_VECTOR:
    case _TOKEN_KEY_LIST:
    case _TOKEN_KEY_SLIST:
    case _TOKEN_KEY_DEQUE:
    case _TOKEN_KEY_STACK:
    case _TOKEN_KEY_QUEUE:
    case _TOKEN_KEY_PRIORITY_QUEUE:
    case _TOKEN_KEY_SET:
    case _TOKEN_KEY_MAP:
    case _TOKEN_KEY_MULTISET:
    case _TOKEN_KEY_MULTIMAP:
    case _TOKEN_KEY_HASH_SET:
    case _TOKEN_KEY_HASH_MAP:
    case _TOKEN_KEY_HASH_MULTISET:
    case _TOKEN_KEY_HASH_MULTIMAP:
    case _TOKEN_KEY_PAIR:
    case _TOKEN_KEY_STRING:
        return _type_parse_cstl_container(s_input, t_token, s_formalname);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_cstl_container(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    /* CSTL_CONTAINER -> SEQUENCE | RELATION | string_t */
    switch(t_token)
    {
    /* CSTL_CONTAINER -> SEQUENCE */
    case _TOKEN_KEY_VECTOR:
    case _TOKEN_KEY_LIST:
    case _TOKEN_KEY_SLIST:
    case _TOKEN_KEY_DEQUE:
    case _TOKEN_KEY_STACK:
    case _TOKEN_KEY_QUEUE:
    case _TOKEN_KEY_PRIORITY_QUEUE:
        return _type_parse_sequence(s_input, t_token, s_formalname);
        break;
    /* CSTL_CONTAINER -> RELATION */
    case _TOKEN_KEY_SET:
    case _TOKEN_KEY_MAP:
    case _TOKEN_KEY_MULTISET:
    case _TOKEN_KEY_MULTIMAP:
    case _TOKEN_KEY_HASH_SET:
    case _TOKEN_KEY_HASH_MAP:
    case _TOKEN_KEY_HASH_MULTISET:
    case _TOKEN_KEY_HASH_MULTIMAP:
    case _TOKEN_KEY_PAIR:
        return _type_parse_relation(s_input, t_token, s_formalname);
        break;
    /* CSTL_CONTAINER -> string_t */
    case _TOKEN_KEY_STRING:
        return _TOKEN_MATCH(_TOKEN_KEY_STRING);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_relation(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    size_t t_leftbracklen = 0;
    size_t t_commalen = 0;
    size_t t_firstdescriptlen = 0;
    size_t t_seconddescriptlen = 0;
    char   s_tokentextindex[_TYPE_NAME_SIZE + 1];
    char   s_firstdescript[_TYPE_NAME_SIZE + 1];
    char   s_seconddescript[_TYPE_NAME_SIZE + 1];

    memset(s_firstdescript, '\0', _TYPE_NAME_SIZE+1);
    memset(s_seconddescript, '\0', _TYPE_NAME_SIZE+1);

    /* RELATION -> RELATION_NAME < TYPE_DESCRIPT, TYPE_DESCRIPT > */
    if(_type_parse_relation_name(s_input, t_token, s_formalname))
    {
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        t_leftbracklen = t_tokenlen;
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        if(t_token != _TOKEN_SIGN_LEFT_BRACKET)
        {
            return false;
        }
        /* handle the first TYPE_DESCRIPT */
        t_token = _type_get_token(s_input+t_leftbracklen, s_tokentext, &t_tokenlen);
        strncat(s_firstdescript, s_tokentext, _TYPE_NAME_SIZE);
        if(!_type_parse_type_descript(
            s_input+t_leftbracklen+t_tokenlen, t_token, s_firstdescript))
        {
            return false;
        }
        strncat(s_formalname, s_firstdescript, _TYPE_NAME_SIZE);
        t_firstdescriptlen = strlen(s_firstdescript);
        /* if the last character is ',' or next token is ',' true */
    }
    else
    {
        return false;
    }
}

static bool_t _type_parse_relation_name(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    /*
     * RELATION_NAME -> set_t | map_t | multiset_t | multimap_t |
     *                  hash_set_t | hash_map_t | hash_multiset_t | hash_multimap_t |
     *                  pair_t
     */
    switch(t_token)
    {
    case _TOKEN_KEY_SET:
        return _TOKEN_MATCH(_TOKEN_KEY_SET);
        break;
    case _TOKEN_KEY_MAP:
        return _TOKEN_MATCH(_TOKEN_KEY_MAP);
        break;
    case _TOKEN_KEY_MULTISET:
        return _TOKEN_MATCH(_TOKEN_KEY_MULTISET);
        break;
    case _TOKEN_KEY_MULTIMAP:
        return _TOKEN_MATCH(_TOKEN_KEY_MULTIMAP);
        break;
    case _TOKEN_KEY_HASH_SET:
        return _TOKEN_MATCH(_TOKEN_KEY_HASH_SET);
        break;
    case _TOKEN_KEY_HASH_MAP:
        return _TOKEN_MATCH(_TOKEN_KEY_HASH_MAP);
        break;
    case _TOKEN_KEY_HASH_MULTISET:
        return _TOKEN_MATCH(_TOKEN_KEY_HASH_MULTISET);
        break;
    case _TOKEN_KEY_HASH_MULTIMAP:
        return _TOKEN_MATCH(_TOKEN_KEY_HASH_MULTIMAP);
        break;
    case _TOKEN_KEY_PAIR:
        return _TOKEN_MATCH(_TOKEN_KEY_PAIR);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_sequence(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_leftbracklen = 0;
    size_t t_tokenlen = 0;
    size_t t_typedescriptlen = 0;                /* get the TYPE_DESCRIPT text len */
    char   s_tokentext[_TYPE_NAME_SIZE + 1];
    char   s_typedescript[_TYPE_NAME_SIZE + 1]; /* get the TYPE_DESCRIPT text */

    memset(s_typedescript, '\0', _TYPE_NAME_SIZE+1);

    /* SEQUENCE -> SEQUENCE_NAME < TYPE_DESCRIPT > */
    if(_type_parse_sequence_name(s_input, t_token, s_formalname))
    {
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        t_leftbracklen = t_tokenlen;
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        if(t_token != _TOKEN_SIGN_LEFT_BRACKET)
        {
            return false;
        }
        /* get TYPE_DESCRIPT */
        t_token = _type_get_token(s_input+t_leftbracklen, s_tokentext, &t_tokenlen);
        strncat(s_typedescript, s_tokentext, _TYPE_NAME_SIZE);
        if(!_type_parse_type_descript(
            s_input+t_leftbracklen+t_tokenlen, t_token, s_typedescript))
        {
            return false;
        }
        strncat(s_formalname, s_typedescript, _TYPE_NAME_SIZE);
        t_typedescriptlen = strlen(s_typedescript);
        /* if the last char is '>' or next token is '>' true else false */
        if(s_formalname[strlen(s_formalname)-1] == '>')
        {
            return true;
        }
        else
        {
            t_token = _type_get_token(
                s_input+t_leftbracklen+t_typedescriptlen, s_tokentext, &t_tokenlen);
            strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
            return t_token == _TOKEN_SIGN_RIGHT_BRACKET ? true : false;
        }
    }
    else
    {
        return false;
    }
}

static bool_t _type_parse_sequence_name(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    /* 
     * SEQUENCE_NAME -> vector_t | list_t | slist_t | deque_t | stack_t | 
     *                  queue_t | priority_queue_t
     */
    switch(t_token)
    {
    case _TOKEN_KEY_VECTOR:
        return _TOKEN_MATCH(_TOKEN_KEY_VECTOR);
        break;
    case _TOKEN_KEY_LIST:
        return _TOKEN_MATCH(_TOKEN_KEY_LIST);
        break;
    case _TOKEN_KEY_SLIST:
        return _TOKEN_MATCH(_TOKEN_KEY_SLIST);
        break;
    case _TOKEN_KEY_DEQUE:
        return _TOKEN_MATCH(_TOKEN_KEY_DEQUE);
        break;
    case _TOKEN_KEY_STACK:
        return _TOKEN_MATCH(_TOKEN_KEY_STACK);
        break;
    case _TOKEN_KEY_QUEUE:
        return _TOKEN_MATCH(_TOKEN_KEY_QUEUE);
        break;
    case _TOKEN_KEY_PRIORITY_QUEUE:
        return _TOKEN_MATCH(_TOKEN_KEY_PRIORITY_QUEUE);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_user_define(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* USER_DEFINE -> USER_DEFINE_TYPE space identifier | identifier */
    switch(t_token)
    {
    /* USER_DEFINE -> USER_DEFINE_TYPE space identifier */
    case _TOKEN_KEY_STRUCT:
    case _TOKEN_KEY_ENUM:
    case _TOKEN_KEY_UNION:
        if(_type_parse_user_define_type(s_input, t_token, s_formalname))
        {
            t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
            strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
            if(t_token != _TOKEN_SIGN_SPACE)
            {
                return false;
            }
            t_token = _type_get_token(s_input+t_tokenlen, s_tokentext, &t_tokenlen);
            strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
            return t_token == _TOKEN_IDENTIFIER ? true :false;
        }
        else
        {
            return false;
        }
        break;
    /* USER_DEFINE -> identifier */
    case _TOKEN_IDENTIFIER:
        return _TOKEN_MATCH(_TOKEN_IDENTIFIER);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_user_define_type(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    /* USER_DEFINE_TYPE -> struct | enum | union */
    switch(t_token)
    {
    case _TOKEN_KEY_STRUCT:
        return _TOKEN_MATCH(_TOKEN_KEY_STRUCT);
        break;
    case _TOKEN_KEY_ENUM:
        return _TOKEN_MATCH(_TOKEN_KEY_ENUM);
        break;
    case _TOKEN_KEY_UNION:
        return _TOKEN_MATCH(_TOKEN_KEY_UNION);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_c_builtin(char* s_formalname)
{
    /* C_BUILTIN -> SIMPLE_BUILTIN | SIGNED_BUILTIN | UNSIGNED_BUILTIN */
    switch(_gt_typeanalysis._t_token)
    {
    /* C_BUILTIN -> SIMPLE_BUILTIN */
    case _TOKEN_KEY_CHAR:
    case _TOKEN_KEY_SHORT:
    case _TOKEN_KEY_INT:
    case _TOKEN_KEY_LONG:
    case _TOKEN_KEY_FLOAT:
    case _TOKEN_KEY_DOUBLE:
    case _TOKEN_KEY_CHAR_POINTER:
    case _TOKEN_KEY_BOOL:
        return _type_parse_simple_builtin(s_formalname);
        break;
    /* C_BUILTIN -> SIGNED_BUILTIN */
    case _TOKEN_KEY_SIGNED:
        return _type_parse_signed_builtin(s_formalname);
        break;
    /* C_BUILTIN -> UNSIGNED_BUILTIN */
    case _TOKEN_KEY_UNSIGNED:
        return _type_parse_unsigned_builtin(s_formalname);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_signed_builtin(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* SIGNED_BUILTIN -> signed COMPLEX_SUFFIX */
    switch(t_token)
    {
    case _TOKEN_KEY_SIGNED:
        _TOKEN_MATCH(_TOKEN_KEY_SIGNED);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_complex_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_unsigned_builtin(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* UNSIGNED_BUILTIN -> unsigned COMPLEX_SUFFIX */
    switch(t_token)
    {
    case _TOKEN_KEY_UNSIGNED:
        _TOKEN_MATCH(_TOKEN_KEY_UNSIGNED);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_complex_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_complex_suffix(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* COMPLEX_SUFFIX -> space SPACE_SUFFIX | $ */
    switch(t_token)
    {
    /* COMPLEX_SUFFIX -> space SPACE_SUFFIX */
    case _TOKEN_SIGN_SPACE:
        _TOKEN_MATCH(_TOKEN_SIGN_SPACE);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_space_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    /* COMPLEX_SUFFIX -> $ */
    case _TOKEN_END_OF_INPUT:
    case _TOKEN_SIGN_RIGHT_BRACKET:
    case _TOKEN_SIGN_COMMA:
        return _TOKEN_MATCH(_TOKEN_EMPTY);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_space_suffix(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* SPACE_SUFFIX -> char | short COMMON_SUFFIX | int | long COMMON_SUFFIX */
    switch(t_token)
    {
    /* SPACE_SUFFIX -> char */
    case _TOKEN_KEY_CHAR:
        return _TOKEN_MATCH(_TOKEN_KEY_CHAR);
        break;
    /* SPACE_SUFFIX -> short COMMON_SUFFIX */
    case _TOKEN_KEY_SHORT:
        _TOKEN_MATCH(_TOKEN_KEY_SHORT);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_common_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    /* SPACE_SUFFIX -> int */
    case _TOKEN_KEY_INT:
        return _TOKEN_MATCH(_TOKEN_KEY_INT);
        break;
    /* SPACE_SUFFIX -> long COMMON_SUFFIX */
    case _TOKEN_KEY_LONG:
        _TOKEN_MATCH(_TOKEN_KEY_LONG);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_common_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_simple_builtin(char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* 
     * SIMPLE_BUILTIN -> char | short COMMON_SUFFIX | int | long SIMPLE_LONG_SUFFIX |
     *                   float | double | char* | bool_t
     */
    switch(_gt_typeanalysis._t_token)
    {
    /* SIMPLE_BUILTIN -> char */
    case _TOKEN_KEY_CHAR:
        return _TOKEN_MATCH(_TOKEN_KEY_CHAR);
        break;
    /* SIMPLE_BUILTIN -> short COMMON_SUFFIX */
    case _TOKEN_KEY_SHORT:
        _TOKEN_MATCH(_TOKEN_KEY_SHORT);
        _type_get_token(s_tokentext);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_common_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    /* SIMPLE_BUILTIN -> int */
    case _TOKEN_KEY_INT:
        return _TOKEN_MATCH(_TOKEN_KEY_INT);
        break;
    /* SIMPLE_BUILTIN -> long SIMPLE_LONG_SUFFIX */
    case _TOKEN_KEY_LONG:
        _TOKEN_MATCH(_TOKEN_KEY_LONG);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_simple_long_suffix(s_input+t_tokenlen, t_token, s_formalname);
        break;
    /* SIMPLE_BUILTIN -> float */
    case _TOKEN_KEY_FLOAT:
        return _TOKEN_MATCH(_TOKEN_KEY_FLOAT);
        break;
    /* SIMPLE_BUILTIN -> double */
    case _TOKEN_KEY_DOUBLE:
        return _TOKEN_MATCH(_TOKEN_KEY_DOUBLE);
        break;
    /* SIMPLE_BUILTIN -> char* */
    case _TOKEN_KEY_CHAR_POINTER:
        return _TOKEN_MATCH(_TOKEN_KEY_CHAR_POINTER);
        break;
    /* SIMPLE_BUILTIN -> bool_t */
    case _TOKEN_KEY_BOOL:
        return _TOKEN_MATCH(_TOKEN_KEY_BOOL);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_common_suffix(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* COMMON_SUFFIX -> space int | $ */
    switch(t_token)
    {
    /* COMMON_SUFFIX -> space int */
    case _TOKEN_SIGN_SPACE:
        _TOKEN_MATCH(_TOKEN_SIGN_SPACE);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return t_token == _TOKEN_KEY_INT ? true : false;
        break;
    /* COMMON_SUFFIX -> $ */
    case _TOKEN_END_OF_INPUT:
    case _TOKEN_SIGN_RIGHT_BRACKET:
    case _TOKEN_SIGN_COMMA:
        return _TOKEN_MATCH(_TOKEN_EMPTY);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_simple_long_suffix(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    size_t t_tokenlen = 0;
    char   s_tokentext[_TYPE_NAME_SIZE + 1];

    /* SIMPLE_LONG_SUFFIX -> space SIMPLE_LONG_SUFFIX_X | $ */
    switch(t_token)
    {
    /* SIMPLE_LONG_SUFFIX -> space SIMPLE_LONG_SUFFIX_X */
    case _TOKEN_SIGN_SPACE:
        _TOKEN_MATCH(_TOKEN_SIGN_SPACE);
        t_token = _type_get_token(s_input, s_tokentext, &t_tokenlen);
        strncat(s_formalname, s_tokentext, _TYPE_NAME_SIZE);
        return _type_parse_simple_long_suffix_x(s_input+t_tokenlen, t_token, s_formalname);
        break;
    /* SIMPLE_LONG_SUFFIX -> $ */
    case _TOKEN_END_OF_INPUT:
    case _TOKEN_SIGN_RIGHT_BRACKET:
    case _TOKEN_SIGN_COMMA:
        return _TOKEN_MATCH(_TOKEN_EMPTY);
        break;
    default:
        return false;
        break;
    }
}

static bool_t _type_parse_simple_long_suffix_x(
    const char* s_input, _typetoken_t t_token, char* s_formalname)
{
    /* SIMPLE_LONG_SUFFIX_X -> int | double */
    switch(t_token)
    {
    case _TOKEN_KEY_INT:
        return _TOKEN_MATCH(_TOKEN_KEY_INT);
        break;
    case _TOKEN_KEY_DOUBLE:
        return _TOKEN_MATCH(_TOKEN_KEY_DOUBLE);
        break;
    default:
        return false;
        break;
    }
}

static void _type_get_token_skip_space(char* s_tokentext)
{
    _type_get_token(s_tokentext);
    if(_gt_typeanalysis._t_token == _TOKEN_SIGN_SPACE)
    {
        _type_get_token(s_tokentext);
        assert(_gt_typeanalysis._t_token != _TOKEN_SIGN_SPACE);
    }
}

static void _type_get_token(char* s_tokentext)
{
    /*
     * this lexical analysis algorithm is associated with 
     * lexical state machine in cstl.bnf that is issured by activesys.cublog.cn
     */
    size_t       t_tokentextindex = 0;
    _typelex_t   t_lexstate = _LEX_START;

    memset(s_tokentext, '\0', _TYPE_NAME_SIZE+1);

    while(t_lexstate != _LEX_ACCEPT)
    {
        switch(t_lexstate)
        {
        case _LEX_START:
            if(isalpha(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index]) ||
               _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == '_') 
            {
                s_tokentext[t_tokentextindex++] =
                _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index++];
                t_lexstate = _LEX_IN_IDENTIFIER;
            }
            else if(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == '<')
            {
                s_tokentext[t_tokentextindex++] =
                _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index++];
                _gt_typeanalysis._t_token = _TOKEN_SIGN_LEFT_BRACKET;
                t_lexstate = _LEX_ACCEPT;
            }
            else if(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == '>')
            {
                s_tokentext[t_tokentextindex++] =
                _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index++];
                _gt_typeanalysis._t_token = _TOKEN_SIGN_RIGHT_BRACKET;
                t_lexstate = _LEX_ACCEPT;
            }
            else if(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == ',')
            {
                s_tokentext[t_tokentextindex++] =
                _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index++];
                _gt_typeanalysis._t_token = _TOKEN_SIGN_COMMA;
                t_lexstate = _LEX_ACCEPT;
            }
            else if(isspace(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index]))
            {
                s_tokentext[t_tokentextindex++] = ' ';
                _gt_typeanalysis._t_index++;
                t_lexstate = _LEX_IN_SPACE;
            }
            else if(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == '\0')
            {
                _gt_typeanalysis._t_token =  _TOKEN_END_OF_INPUT;
                t_lexstate = _LEX_ACCEPT;
            }
            else
            {
                _gt_typeanalysis._t_token =  _TOKEN_INVALID;
                t_lexstate = _LEX_ACCEPT;
            }
            break;
        case _LEX_IN_IDENTIFIER:
            if(isalpha(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index]) ||
               isdigit(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index]) ||
               _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index] == '_')
            {
                s_tokentext[t_tokentextindex++] =
                _gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index++];
                t_lexstate = _LEX_IN_IDENTIFIER;
            }
            else
            {
                _gt_typeanalysis._t_token = _TOKEN_IDENTIFIER;
                t_lexstate = _LEX_ACCEPT;
            }
            break;
        case _LEX_IN_SPACE:
            if(isspace(_gt_typeanalysis._sz_typename[_gt_typeanalysis._t_index]))
            {
                _gt_typeanalysis._t_index++;
                t_lexstate = _LEX_IN_SPACE;
            }
            else
            {
                _gt_typeanalysis._t_token = _TOKEN_SIGN_SPACE;
                t_lexstate = _LEX_ACCEPT;
            }
            break;
        default:
            _gt_typeanalysis._t_token = _TOKEN_INVALID;
            t_lexstate = _LEX_ACCEPT;
            assert(false);
            break;
        }
    }

    /* if token is identifier then check is keyword */
    if(_gt_typeanalysis._t_token == _TOKEN_IDENTIFIER)
    {
        /* test c builtin */
        if(strncmp(s_tokentext, _TOKEN_TEXT_CHAR, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_CHAR;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_SHORT, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_SHORT;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_INT, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_INT;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_LONG, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_LONG;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_DOUBLE, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_DOUBLE;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_FLOAT, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_FLOAT;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_SIGNED, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_SIGNED;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_UNSIGNED, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_UNSIGNED;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_CHAR_POINTER, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_CHAR_POINTER;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_BOOL, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_BOOL;
        }
        /* user define */
        else if(strncmp(s_tokentext, _TOKEN_TEXT_STRUCT, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_STRUCT;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_ENUM, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_ENUM;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_UNION, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_UNION;
        }
        /* cstl containers */
        else if(strncmp(s_tokentext, _TOKEN_TEXT_VECTOR, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_VECTOR;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_LIST, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_LIST;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_SLIST, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_SLIST;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_DEQUE, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_DEQUE;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_STACK, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_STACK;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_QUEUE, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_QUEUE;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_PRIORITY_QUEUE, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_PRIORITY_QUEUE;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_SET, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_SET;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_MAP, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_MAP;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_MULTISET, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_MULTISET;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_MULTIMAP, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_MULTIMAP;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_HASH_SET, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_HASH_SET;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_HASH_MAP, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_HASH_MAP;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_HASH_MULTISET, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_HASH_MULTISET;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_HASH_MULTIMAP, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_HASH_MULTIMAP;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_PAIR, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_PAIR;
        }
        else if(strncmp(s_tokentext, _TOKEN_TEXT_STRING, _TYPE_NAME_SIZE) == 0)
        {
            _gt_typeanalysis._t_token = _TOKEN_KEY_STRING;
        }
        else
        {
            _gt_typeanalysis._t_token = _TOKEN_IDENTIFIER;
        }
    }
}

static void _type_init(void)
{
    size_t       t_i = 0;

    /* set register hash table */
    for(t_i = 0; t_i < _TYPE_REGISTER_BUCKET_COUNT; ++t_i)
    {
        _gt_typeregister._apt_bucket[t_i] = NULL;
    }
    /* init allocator */
    allocate_init(&_gt_typeregister._t_allocator);

    _type_register_c_builtin();
    _type_register_cstl_container();
    
    _gt_typeregister._t_isinit = true;
}

static void _type_register_c_builtin(void)
{
    _TYPE_REGISTER_BEGIN();

    /* register char type */
    _TYPE_REGISTER_TYPE(char, _CHAR_TYPE, char);
    _TYPE_REGISTER_TYPE_NODE(char, _CHAR_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed char, _SIGNED_CHAR_TYPE);
    /* register unsigned char */
    _TYPE_REGISTER_TYPE(unsigned char, _UNSIGNED_CHAR_TYPE, uchar);
    _TYPE_REGISTER_TYPE_NODE(unsigned char, _UNSIGNED_CHAR_TYPE);
    /* register short */
    _TYPE_REGISTER_TYPE(short, _SHORT_TYPE, short);
    _TYPE_REGISTER_TYPE_NODE(short, _SHORT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(short int, _SHORT_INT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed short, _SIGNED_SHORT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed short int, _SIGNED_SHORT_INT_TYPE);
    /* register unsigned short */
    _TYPE_REGISTER_TYPE(unsigned short, _UNSIGNED_SHORT_TYPE, ushort);
    _TYPE_REGISTER_TYPE_NODE(unsigned short, _UNSIGNED_SHORT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(unsigned short int, _UNSIGNED_SHORT_INT_TYPE);
    /* register int */
    _TYPE_REGISTER_TYPE(int, _INT_TYPE, int);
    _TYPE_REGISTER_TYPE_NODE(int, _INT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed, _SIGNED_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed int, _SIGNED_INT_TYPE);
    /* register unsigned int */
    _TYPE_REGISTER_TYPE(unsigned int, _UNSIGNED_INT_TYPE, uint);
    _TYPE_REGISTER_TYPE_NODE(unsigned int, _UNSIGNED_INT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed, _SIGNED_TYPE);
    /* register long */
    _TYPE_REGISTER_TYPE(long, _LONG_TYPE, long);
    _TYPE_REGISTER_TYPE_NODE(long, _LONG_TYPE);
    _TYPE_REGISTER_TYPE_NODE(long int, _LONG_INT_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed long, _SIGNED_LONG_TYPE);
    _TYPE_REGISTER_TYPE_NODE(signed long int, _SIGNED_LONG_INT_TYPE);
    /* register unsigned long */
    _TYPE_REGISTER_TYPE(unsigned long, _UNSIGNED_LONG_TYPE, ulong);
    _TYPE_REGISTER_TYPE_NODE(unsigned long, _UNSIGNED_LONG_TYPE);
    _TYPE_REGISTER_TYPE_NODE(unsigned long int, _UNSIGNED_LONG_INT_TYPE);
    /* register float */
    _TYPE_REGISTER_TYPE(float, _FLOAT_TYPE, float);
    _TYPE_REGISTER_TYPE_NODE(float, _FLOAT_TYPE);
    /* register double */
    _TYPE_REGISTER_TYPE(double, _DOUBLE_TYPE, double);
    _TYPE_REGISTER_TYPE_NODE(double, _DOUBLE_TYPE);
    /* register long double */
    _TYPE_REGISTER_TYPE(long double, _LONG_DOUBLE_TYPE, long_double);
    _TYPE_REGISTER_TYPE_NODE(long double, _LONG_DOUBLE_TYPE);
    /* register bool_t */
    _TYPE_REGISTER_TYPE(bool_t, _BOOL_TYPE, bool);
    _TYPE_REGISTER_TYPE_NODE(bool_t, _BOOL_TYPE);
     /* register char* */
    _TYPE_REGISTER_TYPE(string_t, _C_STRING_TYPE, cstr);
    _TYPE_REGISTER_TYPE_NODE(string_t, _C_STRING_TYPE);

    _TYPE_REGISTER_END();
}

static void _type_register_cstl_container(void)
{
    _TYPE_REGISTER_BEGIN();

    /* register vector_t */
    _TYPE_REGISTER_TYPE(vector_t, _VECTOR_TYPE, vector);
    _TYPE_REGISTER_TYPE_NODE(vector_t, _VECTOR_TYPE);
    /* register list_t */
    _TYPE_REGISTER_TYPE(list_t, _LIST_TYPE, list);
    _TYPE_REGISTER_TYPE_NODE(list_t, _LIST_TYPE);
    /* register slist_t */
    _TYPE_REGISTER_TYPE(slist_t, _SLIST_TYPE, slist);
    _TYPE_REGISTER_TYPE_NODE(slist_t, _SLIST_TYPE);
    /* register deque_t */
    _TYPE_REGISTER_TYPE(deque_t, _DEQUE_TYPE, deque);
    _TYPE_REGISTER_TYPE_NODE(deque_t, _DEQUE_TYPE);
    /* register stack_t */
    _TYPE_REGISTER_TYPE(stack_t, _STACK_TYPE, stack);
    _TYPE_REGISTER_TYPE_NODE(stack_t, _STACK_TYPE);
    /* register queue_t */
    _TYPE_REGISTER_TYPE(queue_t, _QUEUE_TYPE, queue);
    _TYPE_REGISTER_TYPE_NODE(queue_t, _QUEUE_TYPE);
    /* register priority_queue_t */
    _TYPE_REGISTER_TYPE(priority_queue_t, _PRIORITY_QUEUE_TYPE, priority_queue);
    _TYPE_REGISTER_TYPE_NODE(priority_queue_t, _PRIORITY_QUEUE_TYPE);
    /* register set_t */
    _TYPE_REGISTER_TYPE(set_t, _SET_TYPE, set);
    _TYPE_REGISTER_TYPE_NODE(set_t, _SET_TYPE);
    /* register map_t */
    _TYPE_REGISTER_TYPE(map_t, _MAP_TYPE, map);
    _TYPE_REGISTER_TYPE_NODE(map_t, _MAP_TYPE);
    /* register multiset_t */
    _TYPE_REGISTER_TYPE(multiset_t, _MULTISET_TYPE, multiset);
    _TYPE_REGISTER_TYPE_NODE(multiset_t, _MULTISET_TYPE);
    /* register multimap_t */
    _TYPE_REGISTER_TYPE(multimap_t, _MULTIMAP_TYPE, multimap);
    _TYPE_REGISTER_TYPE_NODE(multimap_t, _MULTIMAP_TYPE);
    /* register hash_set_t */
    _TYPE_REGISTER_TYPE(hash_set_t, _HASH_SET_TYPE, hash_set);
    _TYPE_REGISTER_TYPE_NODE(hash_set_t, _HASH_SET_TYPE);
    /* register hash_map_t */
    _TYPE_REGISTER_TYPE(hash_map_t, _HASH_MAP_TYPE, hash_map);
    _TYPE_REGISTER_TYPE_NODE(hash_map_t, _HASH_MAP_TYPE);
    /* register hash_multiset_t */
    _TYPE_REGISTER_TYPE(hash_multiset_t, _HASH_MULTISET_TYPE, hash_multiset);
    _TYPE_REGISTER_TYPE_NODE(hash_multiset_t, _HASH_MULTISET_TYPE);
    /* register hash_multimap_t */
    _TYPE_REGISTER_TYPE(hash_multimap_t, _HASH_MULTIMAP_TYPE, hash_multimap);
    _TYPE_REGISTER_TYPE_NODE(hash_multimap_t, _HASH_MULTIMAP_TYPE);
    /* register pair_t */
    _TYPE_REGISTER_TYPE(pair_t, _PAIR_TYPE, pair);
    _TYPE_REGISTER_TYPE_NODE(pair_t, _PAIR_TYPE);
    /* register string_t */
    _TYPE_REGISTER_TYPE(string_t, _STRING_TYPE, string);
    _TYPE_REGISTER_TYPE_NODE(string_t, _STRING_TYPE);

    _TYPE_REGISTER_END();
}

/*
 * the cstl builtin copy, compare destroy function for c builtin type and cstl containers.
 */
/* c builtin */
/* char */
static void _type_copy_char(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(char*)cpv_first = *(char*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_char(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(char*)cpv_first < *(char*)cpv_second ? true : false;
}
static void _type_destroy_char(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* unsigned char */
static void _type_copy_uchar(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(unsigned char*)cpv_first = *(unsigned char*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_uchar(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(unsigned char*)cpv_first < *(unsigned char*)cpv_second ?
                          true : false;
}
static void _type_destroy_uchar(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* short */
static void _type_copy_short(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(short*)cpv_first = *(short*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_short(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(short*)cpv_first < *(short*)cpv_second ? true : false;
}
static void _type_destroy_short(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* unsigned short */
static void _type_copy_ushort(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(unsigned short*)cpv_first = *(unsigned short*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_ushort(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(unsigned short*)cpv_first < *(unsigned short*)cpv_second ? 
                          true : false;
}
static void _type_destroy_ushort(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* int */
static void _type_copy_int(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(int*)cpv_first = *(int*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_int(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(int*)cpv_first < *(int*)cpv_second ? true : false;
}
static void _type_destroy_int(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* unsigned int */
static void _type_copy_uint(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(unsigned int*)cpv_first = *(unsigned int*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_uint(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(unsigned int*)cpv_first < *(unsigned int*)cpv_second ?
                          true : false;
}
static void _type_destroy_uint(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* long */
static void _type_copy_long(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(long*)cpv_first = *(long*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_long(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(long*)cpv_first < *(long*)cpv_second ? true : false;
}
static void _type_destroy_long(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* unsigned long */
static void _type_copy_ulong(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(unsigned long*)cpv_first = *(unsigned long*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_ulong(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(unsigned long*)cpv_first < *(unsigned long*)cpv_second ?
                          true : false;
}
static void _type_destroy_ulong(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* float */
static void _type_copy_float(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(float*)cpv_first = *(float*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_float(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(float*)cpv_first - *(float*)cpv_second < -FLT_EPSILON ?
                          true : false;
}
static void _type_destroy_float(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* double */
static void _type_copy_double(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(double*)cpv_first = *(double*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_double(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(double*)cpv_first - *(double*)cpv_second < -DBL_EPSILON ?
                          true : false;
}
static void _type_destroy_double(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* long double */
static void _type_copy_long_double(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(long double*)cpv_first = *(long double*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_long_double(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(long double*)cpv_first - *(long double*)cpv_second < 
                          -LDBL_EPSILON ? true : false;
}
static void _type_destroy_long_double(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* bool_t */
static void _type_copy_bool(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)cpv_first = *(bool_t*)cpv_second;
    *(bool_t*)pv_output = true;
}
static void _type_less_bool(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = *(bool_t*)cpv_first < *(bool_t*)cpv_second ? true : false;
}
static void _type_destroy_bool(
    const void* cpv_input, void* pv_output)
{
    _type_destroy_default(cpv_input, pv_output);
}
/* char* */
/*
 * char* is specific c builtin type, the string_t is used for storing the 
 * char* or c_str type. 
 */
static void _type_copy_cstr(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    string_assign((string_t*)cpv_first, (string_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_cstr(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = string_less((string_t*)cpv_first, (string_t*)cpv_second);
}
static void _type_destroy_cstr(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    string_destroy((string_t*)cpv_input);
    *(bool_t*)pv_output = true;
}

/* cstl container */
/* vector_t */
static void _type_copy_vector(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    vector_assign((vector_t*)cpv_first, (vector_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_vector(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = vector_less((vector_t*)cpv_first, (vector_t*)cpv_second);
}
static void _type_destroy_vector(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    vector_destroy((vector_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* list_t */
static void _type_copy_list(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    list_assign((list_t*)cpv_first, (list_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_list(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = list_less((list_t*)cpv_first, (list_t*)cpv_second);
}
static void _type_destroy_list(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    list_destroy((list_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* slist_t */
static void _type_copy_slist(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    slist_assign((slist_t*)cpv_first, (slist_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_slist(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = slist_less((slist_t*)cpv_first, (slist_t*)cpv_second);
}
static void _type_destroy_slist(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    slist_destroy((slist_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* deque_t */
static void _type_copy_deque(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    deque_assign((deque_t*)cpv_first, (deque_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_deque(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = deque_less((deque_t*)cpv_first, (deque_t*)cpv_second);
}
static void _type_destroy_deque(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    deque_destroy((deque_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* stack_t */
static void _type_copy_stack(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    stack_assign((stack_t*)cpv_first, (stack_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_stack(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = stack_less((stack_t*)cpv_first, (stack_t*)cpv_second);
}
static void _type_destroy_stack(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    stack_destroy((stack_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* queue_t */
static void _type_copy_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    queue_assign((queue_t*)cpv_first, (queue_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = queue_less((queue_t*)cpv_first, (queue_t*)cpv_second);
}
static void _type_destroy_queue(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    queue_destroy((queue_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* priority_queue_t */
static void _type_copy_priority_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    priority_queue_assign((priority_queue_t*)cpv_first, (priority_queue_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_priority_queue(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = vector_less(
        &((priority_queue_t*)cpv_first)->_t_vector, 
        &((priority_queue_t*)cpv_second)->_t_vector);
}
static void _type_destroy_priority_queue(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    priority_queue_destroy((priority_queue_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* set_t */
static void _type_copy_set(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    set_assign((set_t*)cpv_first, (set_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_set(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = set_less((set_t*)cpv_first, (set_t*)cpv_second);
}
static void _type_destroy_set(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    set_destroy((set_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* map_t */
static void _type_copy_map(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    map_assign((map_t*)cpv_first, (map_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_map(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = map_less((map_t*)cpv_first, (map_t*)cpv_second);
}
static void _type_destroy_map(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    map_destroy((map_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* multiset_t */
static void _type_copy_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    multiset_assign((multiset_t*)cpv_first, (multiset_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = multiset_less((multiset_t*)cpv_first, (multiset_t*)cpv_second);
}
static void _type_destroy_multiset(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    multiset_destroy((multiset_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* multimap_t */
static void _type_copy_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    multimap_assign((multimap_t*)cpv_first, (multimap_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = multimap_less((multimap_t*)cpv_first, (multimap_t*)cpv_second);
}
static void _type_destroy_multimap(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    multimap_destroy((multimap_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* hash_set_t */
static void _type_copy_hash_set(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    hash_set_assign((hash_set_t*)cpv_first, (hash_set_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_hash_set(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = hash_set_less((hash_set_t*)cpv_first, (hash_set_t*)cpv_second);
}
static void _type_destroy_hash_set(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    hash_set_destroy((hash_set_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* hash_map_t */
static void _type_copy_hash_map(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    hash_map_assign((hash_map_t*)cpv_first, (hash_map_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_hash_map(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = hash_map_less((hash_map_t*)cpv_first, (hash_map_t*)cpv_second);
}
static void _type_destroy_hash_map(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    hash_map_destroy((hash_map_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* hash_multiset_t */
static void _type_copy_hash_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    hash_multiset_assign((hash_multiset_t*)cpv_first, (hash_multiset_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_hash_multiset(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = hash_multiset_less(
        (hash_multiset_t*)cpv_first, (hash_multiset_t*)cpv_second);
}
static void _type_destroy_hash_multiset(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    hash_multiset_destroy((hash_multiset_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* hash_multimap_t */
static void _type_copy_hash_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    hash_multimap_assign((hash_multimap_t*)cpv_first, (hash_multimap_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_hash_multimap(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = hash_multimap_less(
        (hash_multimap_t*)cpv_first, (hash_multimap_t*)cpv_second);
}
static void _type_destroy_hash_multimap(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    hash_multimap_destroy((hash_multimap_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* pair_t */
static void _type_copy_pair(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    pair_assign((pair_t*)cpv_first, (pair_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_pair(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = pair_less((pair_t*)cpv_first, (pair_t*)cpv_second);
}
static void _type_destroy_pair(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    pair_destroy((pair_t*)cpv_input);
    *(bool_t*)pv_output = true;
}
/* string_t */
static void _type_copy_string(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    string_assign((string_t*)cpv_first, (string_t*)cpv_second);
    *(bool_t*)pv_output = true;
}
static void _type_less_string(
    const void* cpv_first, const void* cpv_second, void* pv_output)
{
    assert(cpv_first != NULL && cpv_second != NULL && pv_output != NULL);
    *(bool_t*)pv_output = string_less((string_t*)cpv_first, (string_t*)cpv_second);
}
static void _type_destroy_string(
    const void* cpv_input, void* pv_output)
{
    assert(cpv_input != NULL && pv_output != NULL);
    string_destroy((string_t*)cpv_input);
    *(bool_t*)pv_output = true;
}

/************************** destroy in the future ******************************/
void _unify_types(size_t t_typesize, char* sz_typename)
{
    char*  sz_avoidwarning = NULL;
    size_t t_avoidwarning = 0;
    t_avoidwarning = t_typesize;
    sz_avoidwarning = sz_typename;

#if CSTL_TYPES_UNIFICATION == 1
    /* do nothing */
#elif CSTL_TYPES_UNIFICATION == 2
    /* char type */
    if(strncmp(sz_typename, _SIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* short type */
    else if(
        strncmp(sz_typename, _SIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* unsigned short */
    else if(
        strncmp(sz_typename, _UNSIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _UNSIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* int type */
    else if(
        strncmp(sz_typename, _SIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _INT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* unsigned */
    else if(
        strncmp(sz_typename, _UNSIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _UNSIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* long type */
    else if(
        strncmp(sz_typename, _SIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* unsigne long */
    else if(
        strncmp(sz_typename, _UNSIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _UNSIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
#elif CSTL_TYPES_UNIFICATION == 3
    /* char type */
    if(strncmp(sz_typename, _SIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
       strncmp(sz_typename, _UNSIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* short type */
    else if(
        strncmp(sz_typename, _SIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* int type */
    else if(
        strncmp(sz_typename, _SIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) ==0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _INT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* long type */
    else if(
        strncmp(sz_typename, _SIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
#elif CSTL_TYPES_UNIFICATION == 4
    /* char type */
    if(strncmp(sz_typename, _SIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
       strncmp(sz_typename, _UNSIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
       strncmp(sz_typename, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE);

        if(t_typesize == sizeof(short))
        {
            memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
            strncpy(sz_typename, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE);
        }
    }
    /* short type */
    else if(
        strncmp(sz_typename, _SIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE);

        if(t_typesize == sizeof(int))
        {
            memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
            strncpy(sz_typename, _INT_TYPE, _ELEM_TYPE_NAME_SIZE);
        }
    }
    /* int type */
    else if(
        strncmp(sz_typename, _SIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) ==0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _INT_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
    /* long type */
    else if(
        strncmp(sz_typename, _SIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _SIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _UNSIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
        strncmp(sz_typename, _LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE);

        if(t_typesize == sizeof(int))
        {
            memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
            strncpy(sz_typename, _INT_TYPE, _ELEM_TYPE_NAME_SIZE);
        }
    }
    else if(
        strncmp(sz_typename, _FLOAT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 &&
        t_typesize == sizeof(double))
    {
        memset(sz_typename, '\0', _ELEM_TYPE_NAME_SIZE+1);
        strncpy(sz_typename, _DOUBLE_TYPE, _ELEM_TYPE_NAME_SIZE);
    }
#else
#   error invalid CSTL_TYPES_UNIFICATION macro.
#endif
}

void _get_varg_value(
    void* pv_output,
    va_list val_elemlist,
    size_t t_typesize,
    const char* s_typename)
{
    char sz_builtin[_ELEM_TYPE_NAME_SIZE + 1];

    assert(pv_output != NULL && t_typesize > 0);

    _get_builtin_type(s_typename, sz_builtin);
    /* char */
    if(strncmp(sz_builtin, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
       strncmp(sz_builtin, _SIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(char));
        *(char*)pv_output = va_arg(val_elemlist, int);
    }
    /* unsigned char */
    else if(strncmp(sz_builtin, _UNSIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(unsigned char));
        *(unsigned char*)pv_output = va_arg(val_elemlist, int);
    }
    /* short */
    else if(strncmp(sz_builtin, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(short));
        *(short*)pv_output = va_arg(val_elemlist, int);
    }
    /* unsigned short */
    else if(strncmp(sz_builtin, _UNSIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(unsigned short));
        *(unsigned short*)pv_output = va_arg(val_elemlist, int);
    }
    /* int */
    else if(strncmp(sz_builtin, _INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(int));
        *(int*)pv_output = va_arg(val_elemlist, int);
    }
    /* unsigned int */
    else if(strncmp(sz_builtin, _UNSIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(unsigned int));
        *(unsigned int*)pv_output = va_arg(val_elemlist, unsigned int);
    }
    /* long */
    else if(strncmp(sz_builtin, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(long));
        *(long*)pv_output = va_arg(val_elemlist, long);
    }
    /* unsigned long */
    else if(strncmp(sz_builtin, _UNSIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(unsigned long));
        *(unsigned long*)pv_output = va_arg(val_elemlist, unsigned long);
    }
    /* float */
    else if(strncmp(sz_builtin, _FLOAT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(float));
        *(float*)pv_output = va_arg(val_elemlist, double);
    }
    /* double */
    else if(strncmp(sz_builtin, _DOUBLE_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(double));
        *(double*)pv_output = va_arg(val_elemlist, double);
    }
    /* bool_t */
    else if(strncmp(sz_builtin, _BOOL_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        assert(t_typesize == sizeof(bool_t));
        *(bool_t*)pv_output = va_arg(val_elemlist, int);
    }
    else
    {
        memcpy(pv_output, val_elemlist, t_typesize);
    }
}

void _get_builtin_type(const char* s_typename, char* s_builtin)
{
    assert(s_typename != NULL && s_builtin != NULL);

    memset(s_builtin, '\0', _ELEM_TYPE_NAME_SIZE + 1);

    /* set<...> */
    if(strncmp(s_typename, _SET_IDENTIFY, strlen(_SET_IDENTIFY)) == 0 &&
       s_typename[strlen(_SET_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
       s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET)
    {
        strncpy(
            s_builtin, 
            s_typename + strlen(_SET_IDENTIFY) + 1, 
            strlen(s_typename) - strlen(_SET_IDENTIFY) - 2);
    }
    /* multiset<...> */
    else if(strncmp(s_typename, _MULTISET_IDENTIFY, strlen(_MULTISET_IDENTIFY)) == 0 &&
            s_typename[strlen(_MULTISET_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_MULTISET_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_MULTISET_IDENTIFY) - 2);
    }
    /* map<...,...> */
    else if(strncmp(s_typename, _MAP_IDENTIFY, strlen(_MAP_IDENTIFY)) == 0 &&
            s_typename[strlen(_MAP_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET &&
            strchr(s_typename, _CSTL_COMMA) != NULL)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_MAP_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_MAP_IDENTIFY) - 2);
    }
    /* multimap<...,...> */
    else if(strncmp(s_typename, _MULTIMAP_IDENTIFY, strlen(_MULTIMAP_IDENTIFY)) == 0 &&
            s_typename[strlen(_MULTIMAP_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET &&
            strchr(s_typename, _CSTL_COMMA) != NULL)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_MULTIMAP_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_MULTIMAP_IDENTIFY) - 2);
    }
    /* hash_set<...> */
    else if(strncmp(s_typename, _HASH_SET_IDENTIFY, strlen(_HASH_SET_IDENTIFY)) == 0 &&
            s_typename[strlen(_HASH_SET_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_HASH_SET_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_HASH_SET_IDENTIFY) - 2);
    }
    /* hash_multiset<...> */
    else if(strncmp(
            s_typename, _HASH_MULTISET_IDENTIFY, strlen(_HASH_MULTISET_IDENTIFY)) == 0 &&
            s_typename[strlen(_HASH_MULTISET_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_HASH_MULTISET_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_HASH_MULTISET_IDENTIFY) - 2);
    }
    /* hash_map<...,...> */
    else if(strncmp(s_typename, _HASH_MAP_IDENTIFY, strlen(_HASH_MAP_IDENTIFY)) == 0 &&
            s_typename[strlen(_HASH_MAP_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET &&
            strchr(s_typename, _CSTL_COMMA) != NULL)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_HASH_MAP_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_HASH_MAP_IDENTIFY) - 2);
    }
    /* hash_multimap<...,...> */
    else if(strncmp(
            s_typename, _HASH_MULTIMAP_IDENTIFY, strlen(_HASH_MULTIMAP_IDENTIFY)) == 0 &&
            s_typename[strlen(_HASH_MULTIMAP_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET &&
            strchr(s_typename, _CSTL_COMMA) != NULL)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_HASH_MULTIMAP_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_HASH_MULTIMAP_IDENTIFY) - 2);
    }
    /* basic_string<...> */
    else if(strncmp(
            s_typename, _BASIC_STRING_IDENTIFY, strlen(_BASIC_STRING_IDENTIFY)) == 0 &&
            s_typename[strlen(_BASIC_STRING_IDENTIFY)] == _CSTL_LEFT_BRACKET &&
            s_typename[strlen(s_typename) - 1] == _CSTL_RIGHT_BRACKET)
    {
        strncpy(
            s_builtin,
            s_typename + strlen(_BASIC_STRING_IDENTIFY) + 1,
            strlen(s_typename) - strlen(_BASIC_STRING_IDENTIFY) - 2);
    }
    else
    {
        strncpy(s_builtin, s_typename, _ELEM_TYPE_NAME_SIZE);
    }
}

int (*_get_cmp_function(const char* s_typename))(const void*, const void*)
{
    char sz_builtin[_ELEM_TYPE_NAME_SIZE + 1];

    _get_builtin_type(s_typename, sz_builtin);
    /* char */
    if(strncmp(sz_builtin, _CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
       strncmp(sz_builtin, _SIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_char;
    }
    /* unsigned char */
    else if(strncmp(sz_builtin, _UNSIGNED_CHAR_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_uchar;
    }
    /* short */
    else if(strncmp(sz_builtin, _SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_short;
    }
    /* unsigned short */
    else if(strncmp(sz_builtin, _UNSIGNED_SHORT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_SHORT_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_ushort;
    }
    /* int */
    else if(strncmp(sz_builtin, _INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_int;
    }
    /* unsigned int */
    else if(strncmp(sz_builtin, _UNSIGNED_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_uint;
    }
    /* long */
    else if(strncmp(sz_builtin, _LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _SIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_long;
    }
    /* unsigned long */
    else if(strncmp(sz_builtin, _UNSIGNED_LONG_TYPE, _ELEM_TYPE_NAME_SIZE) == 0 ||
            strncmp(sz_builtin, _UNSIGNED_LONG_INT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_ulong;
    }
    /* float */
    else if(strncmp(sz_builtin, _FLOAT_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_float;
    }
    /* double */
    else if(strncmp(sz_builtin, _DOUBLE_TYPE, _ELEM_TYPE_NAME_SIZE) == 0)
    {
        return _cmp_double;
    }
    else
    {
        return NULL;
    }
}

/** local function implementation section **/
static int _cmp_char(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(char*)cpv_first <= *(char*)cpv_second ?
           *(char*)cpv_first == *(char*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_uchar(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(unsigned char*)cpv_first <= *(unsigned char*)cpv_second ?
           *(unsigned char*)cpv_first == *(unsigned char*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_short(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(short*)cpv_first <= *(short*)cpv_second ?
           *(short*)cpv_first == *(short*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_ushort(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(unsigned short*)cpv_first <= *(unsigned short*)cpv_second ?
           *(unsigned short*)cpv_first == *(unsigned short*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_int(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(int*)cpv_first <= *(int*)cpv_second ?
           *(int*)cpv_first == *(int*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_uint(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(unsigned int*)cpv_first <= *(unsigned int*)cpv_second ?
           *(unsigned int*)cpv_first == *(unsigned int*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_long(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(long*)cpv_first <= *(long*)cpv_second ?
           *(long*)cpv_first == *(long*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_ulong(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(unsigned long*)cpv_first <= *(unsigned long*)cpv_second ?
           *(unsigned long*)cpv_first == *(unsigned long*)cpv_second ?
           0 : -1 : 1;
}

static int _cmp_float(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(float*)cpv_first - *(float*)cpv_second < FLT_EPSILON ?
           *(float*)cpv_first - *(float*)cpv_second > -FLT_EPSILON ?
           0 : -1 : 1;
}

static int _cmp_double(const void* cpv_first, const void* cpv_second)
{
    assert(cpv_first != NULL && cpv_second != NULL);

    return *(double*)cpv_first - *(double*)cpv_second < DBL_EPSILON ?
           *(double*)cpv_first - *(double*)cpv_second > -DBL_EPSILON ?
           0 : -1 : 1;
}

/** eof **/

