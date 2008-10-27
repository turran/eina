/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */
/* EINA - EFL data type library
 * Copyright (C) 2002-2008 Carsten Haitzler, Jorge Luis Zapata Muga, Cedric Bail
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (C) 2008 Peter Wehrfritz
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies of the Software and its Copyright notices. In addition publicly
 *  documented acknowledgment must be given that this software has been used if no
 *  source code of this software is made available publicly. This includes
 *  acknowledgments in either Copyright notices, Manuals, Publicity and Marketing
 *  documents or any documentation provided with any product containing this
 *  software. This License does not apply to any software that links to the
 *  libraries provided by this software (statically or dynamically), but only to
 *  the software provided.
 *
 *  Please see the OLD-COPYING.PLAIN for a plain-english explanation of this notice
 *  and it's intent.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 *  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/**
 * @page tutorial_stringshare_page Stringshare Tutorial
 *
 * to be written...
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#include "eina_stringshare.h"
#include "eina_hash.h"
#include "eina_rbtree.h"
#include "eina_error.h"
#include "eina_private.h"
#include "eina_magic.h"

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

/**
 * @cond LOCAL
 */

#define EINA_STRINGSHARE_BUCKETS 256
#define EINA_STRINGSHARE_MASK 0xFF

#define EINA_MAGIC_CHECK_STRINGSHARE_HEAD(d)			\
  do {								\
    if (!EINA_MAGIC_CHECK((d), EINA_MAGIC_STRINGSHARE_HEAD))	\
      EINA_MAGIC_FAIL((d), EINA_MAGIC_STRINGSHARE_HEAD);	\
  } while (0);

#define EINA_MAGIC_CHECK_STRINGSHARE_NODE(d)			\
  do {								\
    if (!EINA_MAGIC_CHECK((d), EINA_MAGIC_STRINGSHARE_NODE))	\
      EINA_MAGIC_FAIL((d), EINA_MAGIC_STRINGSHARE_NODE);	\
  } while (0);

typedef struct _Eina_Stringshare             Eina_Stringshare;
typedef struct _Eina_Stringshare_Node        Eina_Stringshare_Node;
typedef struct _Eina_Stringshare_Head        Eina_Stringshare_Head;

struct _Eina_Stringshare
{
   Eina_Stringshare_Head *buckets[EINA_STRINGSHARE_BUCKETS];

   EINA_MAGIC;
};

struct _Eina_Stringshare_Head
{
   EINA_RBTREE;
   EINA_MAGIC;

   int                    hash;

#ifdef EINA_STRINGSHARE_USAGE
   int                    population;
#endif

   Eina_Stringshare_Node *head;
};

struct _Eina_Stringshare_Node
{
   EINA_MAGIC;

   Eina_Stringshare_Node *next;

   int			  length;
   int                    references;

   Eina_Bool		  begin : 1;
};

static Eina_Stringshare *share = NULL;
static int _eina_stringshare_init_count = 0;

#ifdef EINA_STRINGSHARE_USAGE
static int population = 0;
static int max_population = 0;
static int max_node_population = 0;
#endif

static int
_eina_stringshare_cmp(const Eina_Stringshare_Head *ed, const int *hash, __UNUSED__ int length, __UNUSED__ void *data)
{
   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(ed);

   return ed->hash - *hash;
}

static Eina_Rbtree_Direction
_eina_stringshare_node(const Eina_Stringshare_Head *left, const Eina_Stringshare_Head *right, __UNUSED__ void *data)
{
   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(left);
   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(right);

   if (left->hash - right->hash < 0)
     return EINA_RBTREE_LEFT;
   return EINA_RBTREE_RIGHT;
}

static void
_eina_stringshare_head_free(Eina_Stringshare_Head *ed, __UNUSED__ void *data)
{
   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(ed);

   while (ed->head)
     {
	Eina_Stringshare_Node *el = ed->head;

	ed->head = ed->head->next;
	if (el->begin == EINA_FALSE)
	  MAGIC_FREE(el);
     }
   MAGIC_FREE(ed);
}

/**
 * @endcond
 */


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

/**
 * @addtogroup Eina_Data_Types_Group Data Types
 *
 * @{
 */

/**
 * @addtogroup Eina_Stringshare_Group Stringshare
 *
 * These functions allow you to store one copy of a string, and use it
 * throughout your program.
 *
 * This is a method to reduce the number of duplicated strings kept in
 * memory. It's pretty common for the same strings to be dynamically
 * allocated repeatedly between applications and libraries, especially in
 * circumstances where you could have multiple copies of a structure that
 * allocates the string. So rather than duplicating and freeing these
 * strings, you request a read-only pointer to an existing string and
 * only incur the overhead of a hash lookup.
 *
 * It sounds like micro-optimizing, but profiling has shown this can have
 * a significant impact as you scale the number of copies up. It improves
 * string creation/destruction speed, reduces memory use and decreases
 * memory fragmentation, so a win all-around.
 *
 * For more information, you can look at the @ref tutorial_stringshare_page.
 *
 * @{
 */

/**
 * @brief Initialize the eina stringshare internal structure.
 *
 * @return 1 or greater on success, 0 on error.
 *
 * This function allocates the memory needed by the stringshare
 * internal structure and sets up the error module of Eina. It is also
 * called by eina_init(). It returns 0 on failure, otherwise it
 * returns the number of times it has already been called.
 */
EAPI int
eina_stringshare_init()
{
   /*
    * No strings have been loaded at this point, so create the hash
    * table for storing string info for later.
    */
   if (!_eina_stringshare_init_count)
     {
	share = calloc(1, sizeof(Eina_Stringshare));
	if (!share)
	  return 0;

	eina_error_init();
	eina_magic_string_init();

	eina_magic_string_set(EINA_MAGIC_STRINGSHARE,
			      "Eina Stringshare");
	eina_magic_string_set(EINA_MAGIC_STRINGSHARE_HEAD,
			      "Eina Stringshare Head");
	eina_magic_string_set(EINA_MAGIC_STRINGSHARE_NODE,
			      "Eina Stringshare Node");
       	EINA_MAGIC_SET(share, EINA_MAGIC_STRINGSHARE);
     }

   return ++_eina_stringshare_init_count;
}

/**
 * @brief Shut down the eina stringshare internal structures
 *
 * @return 0 when the stringshare module is completely shut down, 1 or
 * greater otherwise.
 *
 * This function frees the memory allocated by eina_stringshare_init()
 * and shuts down the error module. It is also called by
 * eina_shutdown(). It returns 0 when it is called the same number of
 * times than eina_stringshare_init().
 */
EAPI int
eina_stringshare_shutdown()
{
#ifdef EINA_STRINGSHARE_USAGE
   fprintf(stderr, "eina stringshare statistic:\n");
   fprintf(stderr, " * maximum shared strings : %i\n", max_population);
   fprintf(stderr, " * maximum shared strings per node : %i\n", max_node_population);
#endif

   --_eina_stringshare_init_count;
   if (!_eina_stringshare_init_count)
     {
	int i;
	/* remove any string still in the table */
	for (i = 0; i < EINA_STRINGSHARE_BUCKETS; i++)
	  {
	     eina_rbtree_delete(EINA_RBTREE_GET(share->buckets[i]), EINA_RBTREE_FREE_CB(_eina_stringshare_head_free), NULL);
	     share->buckets[i] = NULL;
	  }
	MAGIC_FREE(share);

#ifdef EINA_STRINGSHARE_USAGE
	population = 0;
	max_node_population = 0;
	max_population = 0;
#endif

	eina_magic_string_shutdown();
	eina_error_shutdown();
     }

   return _eina_stringshare_init_count;
}

/**
 * @brief Retrieve an instance of a string for use in a program.
 *
 * @param   str The string to retrieve an instance of.
 * @return  A pointer to an instance of the string on success.
 *          @c NULL on failure.
 *
 * This function retrieves an instance of @p str. If @p str is
 * @c NULL, then @c NULL is returned. If @p str is already stored, it
 * is just returned and its reference counter is increased. Otherwise
 * it is added to the strings to be searched and a duplicated string
 * of @p str is returned.
 */
EAPI const char *
eina_stringshare_add(const char *str)
{
   Eina_Stringshare_Node *nel = NULL;
   Eina_Stringshare_Node *tmp;
   Eina_Stringshare_Head *ed;
   Eina_Stringshare_Node *el;
   char *el_str;
   int hash_num, slen, hash;

   if (!str) return NULL;
   hash = eina_hash_djb2_len(str, &slen);
   hash_num = hash & 0xFF;
   hash = (hash >> 8) & EINA_STRINGSHARE_MASK;

   ed = (Eina_Stringshare_Head*) eina_rbtree_inline_lookup((Eina_Rbtree*) share->buckets[hash_num],
							   &hash, 0,
							   EINA_RBTREE_CMP_KEY_CB(_eina_stringshare_cmp), NULL);
   if (!ed)
     {
	ed = malloc(sizeof (Eina_Stringshare_Head) + sizeof (Eina_Stringshare_Node) + slen);
	if (!ed) return NULL;
	EINA_MAGIC_SET(ed, EINA_MAGIC_STRINGSHARE_HEAD);

	ed->hash = hash;
	ed->head = NULL;

#ifdef EINA_STRINGSHARE_USAGE
	ed->population = 0;
#endif

	share->buckets[hash_num] = (Eina_Stringshare_Head*) eina_rbtree_inline_insert((Eina_Rbtree*) share->buckets[hash_num],
										      EINA_RBTREE_GET(ed),
										      EINA_RBTREE_CMP_NODE_CB(_eina_stringshare_node), NULL);

	nel = (Eina_Stringshare_Node*) (ed + 1);
	EINA_MAGIC_SET(nel, EINA_MAGIC_STRINGSHARE_NODE);

	nel->begin = EINA_TRUE;
     }

   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(ed);

   for (el = ed->head, tmp = NULL;
	el && (slen != el->length || memcmp(str, (const char*) (el + 1), slen) != 0);
	tmp = el, el = el->next)
     ;

   if (el)
     {
	if (tmp)
	  {
	     tmp->next = el->next;
	     el->next = ed->head;
	     ed->head = el;
	  }

	el->references++;
	return (const char*) (el + 1);
     }

   if (!nel)
     {
	nel = malloc(sizeof (Eina_Stringshare_Node) + slen);
	if (!nel) return NULL;
	EINA_MAGIC_SET(nel, EINA_MAGIC_STRINGSHARE_NODE);

	nel->begin = EINA_FALSE;
     }

   nel->references = 1;
   nel->length = slen;

   el_str = (char*) (nel + 1);
   memcpy(el_str, str, slen);

   nel->next = ed->head;
   ed->head = nel;

#ifdef EINA_STRINGSHARE_USAGE
   ed->population++;
   population++;
   if (population > max_population) max_population = population;
   if (ed->population > max_node_population) max_node_population = ed->population;
#endif

   return el_str;
}

/**
 * @brief Note that the given string has lost an instance.
 *
 * @param str string The given string.
 *
 * This function decreases the reference counter associated to @p str
 * if it exists. If that counter reaches 0, the memory associated to
 * @p str is freed. If @p str is NULL, the function returns
 * immediatly.
 */
EAPI void
eina_stringshare_del(const char *str)
{
   Eina_Stringshare_Head *ed;
   Eina_Stringshare_Node *el;
   Eina_Stringshare_Node *prev;
   int hash_num, slen, hash;

   if (!str) return;
   hash = eina_hash_djb2_len(str, &slen);
   hash_num = hash & 0xFF;
   hash = (hash >> 8) & EINA_STRINGSHARE_MASK;

   ed = (Eina_Stringshare_Head*) eina_rbtree_inline_lookup(EINA_RBTREE_GET(share->buckets[hash_num]),
							   &hash, 0,
							   EINA_RBTREE_CMP_KEY_CB(_eina_stringshare_cmp), NULL);
   if (!ed) goto on_error;

   EINA_MAGIC_CHECK_STRINGSHARE_HEAD(ed);

   for (prev = NULL, el = ed->head;
	el && (const char*) (el + 1) != str;
	prev = el, el = el->next)
     ;

   if (!el) goto on_error;

   EINA_MAGIC_CHECK_STRINGSHARE_NODE(el);

   el->references--;
   if (el->references == 0)
     {
	if (prev) prev->next = el->next;
	else ed->head = el->next;
	if (el->begin == EINA_FALSE)
	  MAGIC_FREE(el);

#ifdef EINA_STRINGSHARE_USAGE
	ed->population--;
	population--;
#endif

	if (ed->head == NULL)
	  {
	     share->buckets[hash_num] = (Eina_Stringshare_Head*) eina_rbtree_inline_remove(EINA_RBTREE_GET(share->buckets[hash_num]),
											   EINA_RBTREE_GET(ed),
											   EINA_RBTREE_CMP_NODE_CB(_eina_stringshare_node),
											   NULL);
	     MAGIC_FREE(ed);
	  }
     }
   return ;

 on_error:
   EINA_ERROR_PWARN("EEEK trying to del non-shared stringshare \"%s\"\n", str);
   if (getenv("EINA_ERROR_ABORT")) abort();
}

struct dumpinfo
{
   int used, saved, dups, unique;
};

static Eina_Bool
eina_iterator_array_check(const Eina_Rbtree *rbtree __UNUSED__, Eina_Stringshare_Head *head, struct dumpinfo *fdata)
{
   Eina_Stringshare_Node *node;
   
   fdata->used += sizeof(Eina_Stringshare_Head);
   for (node = head->head; node; node = node->next)
     {
	printf("DDD: %5i %5i ", node->length, node->references);
	printf("'%s'\n", ((char *)node) + sizeof(Eina_Stringshare_Node));
	fdata->used += sizeof(Eina_Stringshare_Node);
	fdata->used += node->length;
	fdata->saved += (node->references - 1) * node->length;
	fdata->dups += node->references - 1;
	fdata->unique++;
     }
   return EINA_TRUE;
}

/**
 * @brief Dump the contents of the stringshare.
 *
 * This function dumps all strings in the stringshare to stdout with a
 * DDD: prefix per line and a memory usage summary.
 */
EAPI void
eina_stringshare_dump(void)
{
   Eina_Iterator *it;
   int i;
   struct dumpinfo di;
   
   if (!share) return;
   di.used = 0;
   di.saved = 0;
   di.dups = 0;
   di.unique = 0;
   printf("DDD:   len   ref string\n");
   printf("DDD:-------------------\n");
   for (i = 0; i < EINA_STRINGSHARE_BUCKETS; i++)
     {
	if (!share->buckets[i]) continue;
//	printf("DDD: BUCKET # %i (HEAD=%i, NODE=%i)\n", i,
//	       sizeof(Eina_Stringshare_Head), sizeof(Eina_Stringshare_Node));
	it = eina_rbtree_iterator_prefix((Eina_Rbtree *)share->buckets[i]);
	eina_iterator_foreach(it, EINA_EACH(eina_iterator_array_check), &di);
	eina_iterator_free(it);
     }
   printf("DDD:-------------------\n");
   printf("DDD: usage (bytes) = %i, saved = %i (%i duplicates, %i unique)\n", 
	  di.used, di.saved, di.dups, di.unique);
}

/**
 * @}
 */

/**
 * @}
 */
