/*
    lex.c - contains lexer test case
    Copyright 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/// @file lex.c

#include "../internal.h"
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#define NEXTEST_NAME "lex"
#include <libnex/progname.h>
#include <libnex/stringref.h>
#include <nextest.h>
#include <stdlib.h>

void _confSetFileName (const char* file);

int main()
{
    // Set up locale stuff
    setlocale (LC_ALL, "");
    setprogname ("lex");
    _confSetFileName ("testLex.testxt");
    lexState_t* state = _confLexInit ("testLex.testxt");
    _confToken_t* tok = NULL;
    tok = _confLex (state);
    TEST_ANON (tok->type, 4);
    TEST_ANON (tok->line, 10);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 5);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 7);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 6);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 14);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 9);
    TEST_ANON (tok->num, 25);
    TEST_ANON (tok->line, 12);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 9);
    TEST_ANON (tok->num, 0xAD8B2);
    TEST_ANON (tok->line, 14);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 9);
    TEST_ANON (tok->num, -34);
    TEST_ANON (tok->line, 16);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 8);
    TEST_ANON (tok->line, 18);
    TEST_BOOL_ANON (!c32cmp (StrRefGet (tok->semVal), U"test2-test3_"));
    StrRefDestroy (tok->semVal);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 11);
    TEST_ANON (tok->line, 20);
    TEST_BOOL_ANON (!c32cmp (StrRefGet (tok->semVal), U"test t \\ '"));
    StrRefDestroy (tok->semVal);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 11);
    TEST_ANON (tok->line, 22);
    TEST_BOOL_ANON (
        !c32cmp (StrRefGet (tok->semVal), U"test string en_US.UTF-8 $ \" \ntest"));
    StrRefDestroy (tok->semVal);
    free (tok);
    tok = _confLex (state);
    TEST_ANON (tok->type, 12);
    StrRefDestroy (tok->semVal);
    free (tok);
    _confLexDestroy (state);
    return 0;
}
