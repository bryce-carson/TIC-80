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

#if !defined R_INTERNALS_H_
#include <Rinternals.h> /* defines LibExtern SEXP R_GlobalEnv */
#endif

#include <Rembedded.h>
#include <Rinterface.h>
#include <Rconfig.h>
#include <Rdefines.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/renv.h"

bool R_initialized_once = false;
static bool initR(tic_mem *tic, const char *code);

void evalR(tic_mem *memory, const char *code) {
	setEnvironmentVariablesIfUnset();
	if (!R_initialized_once) {
		initR(memory, code);
		R_initialized_once = true;
	}
	SEXP RESULT;
	if (R_initialized_once) RESULT = Rf_eval(Rf_mkString(code), R_GlobalEnv);
	#if defined ebug /* -DEBUG=ON */
	Rprintf("%s", RESULT);
	#endif
}
SEXP r_trace(tic_mem *memory, SEXP *args) {
  const char *text = "Hello, woRld!";
	u8 color;
	((tic_core *) memory)->api.trace(memory, text, color ? color : 15);
	return R_NilValue;
}

extern int R_running_as_main_program;   /* location within The R sources: ../unix/system.c */

void R_CleanUp(Rboolean saveact, int status, int RunLast) { ; }

void R_Suicide(const char *message)
{
	char  pp[1024];
	snprintf(pp, 1024, "Fatal error, but can't kermit: %s\nExecute me, please.\n", message);
	R_ShowMessage(pp);
	exit(1);

	while(1);
}

static tic_mem *tic_memory_static;

void R_ShowMessage(const char *s) {
/* Always use the sixteenth color. */
	((tic_core *) tic_memory_static)->api.trace(tic_memory_static, s, 15);
}

/* This function is called with code, which is the entirety of the studio
 * editor's code buffer (i.e. the entire game code as one string). */
static bool initR(tic_mem *tic, const char *code) {
	tic_memory_static = tic;

  tic_core *core;
  if ((core = (((tic_core *) tic))->currentVM) != NULL) {
  	Rf_endEmbeddedR(0);
  	core->currentVM = NULL;
  }

	/* TODO: it makes sense to write the code to a file and initialize R with its
	 * input taken from that file, I think. */

	/* embdRAV: embedded R argument vector. */
	static char *embdRAV[] = { "TIC-80", "--quiet", "--vanilla" };

  /* Without this nothing in R will work. */
	setEnvironmentVariablesIfUnset();
  static bool R_Initialized = false;

	R_running_as_main_program = 0;

	if (!R_Initialized) {
		R_Initialized = (bool) Rf_initEmbeddedR(sizeof(embdRAV)/sizeof(embdRAV[0]), embdRAV);
		R_running_as_main_program = 0;
		/* Declared in Rinterface.h, defined in Rf_initEmbeddedR. */
		R_Interactive = false;
	}

	/* TODO: I think it makes sense to evaluate the code, then check if this
	 * exists, then end the init function with evaluating the TIC-80 function. */
	Rf_eval(Rf_mkString("if (exists(\"BOOT\") && is.function(BOOT) BOOT() else BOOT <- function() NULL;;"),
					R_GlobalEnv);

	tic_script *config = (tic_get_script(tic));
	for (int kiwi = 0; kiwi <= config->api_keywordsCount; kiwi++) {
#define defineRFn_(keyword) Rf_eval(Rf_mkString("t80."#keyword" <- function(...) .C(r_"#keyword", ...);"), R_GlobalEnv);
#define doDefineRFunction(keyword) defineRFn_(keyword)
		doDefineRFunction(config->api_keywords[kiwi]);
#undef doDefineRFunction
#undef defineRFn_
	}

	return R_Initialized;
}

static void closeR(tic_mem *tic) {
  tic_core *core;
  if ((core = (((tic_core *) tic))->currentVM) != NULL) {
  	Rf_endEmbeddedR(0);
  	core->currentVM = NULL;
  }
}
static void callRFn_TIC80(tic_mem* tic) {
#if !defined R_INTERNALS_H_
#error "R_GlobalEnv not defined because Rinternals.h not properly included... somehow."
#endif
	/* if (exists("TIC-80") && is.function(`TIC-80`)) `TIC-80`() */
	Rf_eval(Rf_mkString("if (exists(\"TIC-80\") && is.function(`TIC-80`)) "\
											"`TIC-80`()"),
					R_GlobalEnv);
}
#define defineCallRFnInEnvironment_(f, e, ...)                          \
  static void callRFn_##f(tic_mem *tic, ##__VA_ARGS__) {                \
    Rf_eval(Rf_mkString("if (exists(\""#f"\") && is.function(`"#f"`)) " \
                        "`"#f"`() else stop(\""#f" is not a defined function!\")"),\
            e);                                                         \
  }
/* i.e., if (exists("f") && is.function(`f`)) `f`(), allowing call of syntactic
 * and non-syntactic names. */
#define defineCallRFn_(f, ...) defineCallRFnInEnvironment_(f, R_GlobalEnv, ...)
defineCallRFn_(BOOT)
/* s32 row/index, void *data as well as the tic_mem *tic parameters. */
defineCallRFn_(MENU, s32 index, void *data)
defineCallRFn_(BDR, s32 row, void *data)
defineCallRFn_(SCN, s32 row, void *data)
#undef defineCallRFn_
#undef defineCallRFnInEnvironment_

bool initR(tic_mem *tic, const char *code);

tic_core *getTICCore(tic_mem* tic, const char* code) {
  /* tic_core *core = NULL; */
  /* while (core == NULL && !initR(tic, code)) { */
  /*   core = (((tic_core *) tic))->currentVM; */
  /* } */
	/* return core; */
  return (tic_core *) tic;
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

/* DEFAULT visibility*/
/* EXPORT_SCRIPT -> RScriptConfig if static, else ScriptConfig */
TIC_EXPORT const tic_script EXPORT_SCRIPT(R) =
{
	/* The first five members of the struct have the sum total following
	 * size. */
	/* sizeof(u8) + 3 * sizeof(char *)  */
	/* R's id is determined by counting up from 10, beginning with Lua, all of
		 the other languages TIC-80 supports. Python was the 10th langauge supported,
		 with .id 20. */
	.id                     = 21,
	.name                   = "r",
	.fileExtension          = ".r",
	.projectComment         = "##",
	{
		.init                 = initR,

		.close                = closeR,
		.tick                 = callRFn_TIC80,
		.boot                 = callRFn_BOOT,

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
