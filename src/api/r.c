/* MIT License
 *
 * Copyright © 2024 Bryce Carson
 *
 * GitHub: bryce-carson
 * Email: bryce.a.carson@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "core/core.h"

#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>
#include <Rembedded.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void evalR(tic_mem *memory, const char *code) {
  SEXP result = Rf_eval(Rf_mkString(code), R_GlobalEnv);
}

#define killer																						\
  tic_core *core;																					\
  if ((core = (((tic_core *) tic))->currentVM) != NULL) {	\
    Rf_endEmbeddedR(0);																		\
    core->currentVM = NULL;																\
  }

tic_core *getTICCore(tic_mem* tic, const char* code);

static bool initR(tic_mem *tic, const char *code) {
  killer;

  int tries = 1;
  core = getTICCore(tic, code);

tryOnceMoreOnly:
  /* embdRAV: embedded R argument vector. */
  char *embdRAV[]= { "REmbeddedInTIC80", "--silent" };
  bool rc = (bool) Rf_initEmbeddedR(sizeof(embdRAV)/sizeof(embdRAV[0]), embdRAV);
  if (rc) return rc;
  else if (tries--) goto tryOnceMoreOnly;

  return false;
}

static void closeR(tic_mem *tic) {
  killer;
}
static void callRFn_TIC80() {
  /* if (exists("TIC-80") && is.function(`TIC-80`)) `TIC-80`() */
  Rf_eval(Rf_mkString("if (exists(\"TIC-80\") && is.function(`TIC-80`)) "\
                      "`TIC-80`()"),
    R_GlobalEnv);
}
#define defineCallRFn_(x)																								\
  static void callRFn_##x(tic_mem *tic) {																\
    Rf_eval(Rf_mkString("if (exists(\""#x"\") && is.function("#x")) "#x"()"), \
            R_GlobalEnv);																								\
  }
/* if (exists("x") && is.function(x)) x() */
defineCallRFn_(MENU)
defineCallRFn_(BDR)
defineCallRFn_(BOOT)
defineCallRFn_(SCN)
#undef defineCallRFn_

bool initR(tic_mem *tic, const char *code);

tic_core *getTICCore(tic_mem* tic, const char* code) {
  tic_core *core;
  while (core == NULL && (!initR(tic, code))) {
    core = (((tic_core *) tic))->currentVM;
  }
  return core;
}

static const char* const RKeywords [] =
{
	"if", "else", "repeat", "while", "function", "for", "in", "next", "break",
	"TRUE", "FALSE", "NULL", "Inf", "NaN", "NA", "NA_integer_", "NA_real_",
	"NA_complex_", "NA_character_",
	/* et cetera, see ?dots */
	"...", "..1", "..2", "..3", "..4", "..5", "..6", "..7", "..8", "..9",
};

/* A naive edit of the Python function to check if a character is a valid
 * character within an identifier. */
static bool r_isalnum(char c) {
  return (
    (c >= 'a' && c <= 'z')
    || (c >= 'A' && c <= 'Z')
    || (c >= '0' && c <= '9')
    || (c == '_') || (c == '.')
    );
}

static const tic_outline_item* getROutline(const char* code, s32* size)
{
  enum{Size = sizeof(tic_outline_item)};
  *size = 0;

  static tic_outline_item* items = NULL;

  if(items)
  {
    free(items);
    items = NULL;
  }

  const char* ptr = code;

  while(true)
  {
    static const char FuncString[] = "<- function(";

    ptr = strstr(ptr, FuncString);

    if(ptr)
    {
      ptr += sizeof FuncString - 1;

      const char* start = ptr;
      const char* end = start;

      while(*ptr)
      {
        char c = *ptr;

        if(r_isalnum(c));
        else
        {
          end = ptr;
          break;
        }
        ptr++;
      }

      if(end > start)
      {
        items = realloc(items, (*size + 1) * Size);

        items[*size].pos = start;
        items[*size].size = (s32)(end - start);

        (*size)++;
      }
    }
    else break;
  }

  return items;
}

static const char* RAPIKeywords[] = {
#define TIC_CALLBACK_DEF(name, ...) #name,
  TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef  TIC_CALLBACK_DEF

#define API_KEYWORD_DEF(name, ...) #name,
  TIC_API_LIST(API_KEYWORD_DEF)
#undef  API_KEYWORD_DEF
};

static const u8 DemoRom[] =
{
  /* Automatically built from ../../demos/rdemo.r */
#include "../build/assets/rdemo.tic.dat"
};

static const u8 MarkRom[] =
{
  /* Automatically built from ../../demos/bunny/rbenchmark.r */
#include "../build/assets/rmark.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(R) =
{
	/* The first five members of the struct have the sum total following
	 * size. */
	/* sizeof(u8) + 3 * sizeof(char *)  */
	.id                     = 42,
	.name                   = "r",
	.fileExtension          = ".r",
	.projectComment         = "##",
	{
		.init                 = initR,
		.close                = closeR,
		.tick                 = callRFn_TIC80,
		.boot                 = callRFn_BOOT,

		/* In the Scheme integration these have additional argument types s32 and
		 * void * (row and data, respectively). */
		.callback             =
		{
			.scanline           = callRFn_SCN,
			.border             = callRFn_BDR,
			.menu               = callRFn_MENU,
		},
	},

	.getOutline             = getROutline,
	.eval                   = evalR,

	.blockCommentStart      = NULL,
	.blockCommentEnd        = NULL,
	.blockCommentStart2     = NULL,
	.blockCommentEnd2       = NULL,
	.singleComment          = "##",
	.blockStringStart       = "\"",
	.blockStringEnd         = "\"",
	.stdStringStartEnd      = "\"",
	.blockEnd               = NULL,
	.lang_isalnum           = r_isalnum,
	.api_keywords           = RAPIKeywords,
	.api_keywordsCount      = COUNT_OF(RAPIKeywords),
	.useStructuredEdition   = false,

	.keywords               = RKeywords,
	.keywordsCount          = COUNT_OF(RKeywords),

	.demo = {DemoRom, sizeof DemoRom},
	.mark = {MarkRom, sizeof MarkRom, "rmark.tic"},
};