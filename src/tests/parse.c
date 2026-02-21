/*
    parse.c - contains parser test cases
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

/// @file parse.c

#include "../internal.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>
#define NEXTEST_NAME "parse"
#include <libnex/progname.h>
#include <libnex/stringref.h>
#include <nextest.h>

int main()
{
    // Set up locale stuff
    setlocale (LC_ALL, "");
    setprogname ("parse");
    LibConf_t* ctx = ConfInit ("testParse.testxt", NULL);
    if (!ctx || ctx->error)
        return 1;
    ListHead_t* list = ctx->blocks;
    ListEntry_t* entry = NULL;
    ConfBlock_t* block = NULL;
    entry = ListFront (list);
    ListEntry_t* mainEnt = entry;
    block = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockName), "test"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockType), "package"));
    entry = ListFront (block->props);
    ConfProperty_t* prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "test"));
    for (int i = 0; i < prop->nextVal; ++i)
    {
        if (prop->vals[i].type == DATATYPE_IDENTIFIER)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].id), "one"));
        }
        else if (prop->vals[i].type == DATATYPE_NUMBER)
        {
            TEST_ANON (prop->vals[i].numVal, 3);
        }
        else if (prop->vals[i].type == DATATYPE_STRING)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].str), "test"));
        }
    }
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].id), "propVal"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].str), "string"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_ANON (prop->vals[0].numVal, 0x20);
    entry = ListIterate (mainEnt);
    mainEnt = entry;
    block = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockName), "test"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockType), "package"));
    entry = ListFront (block->props);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "test"));
    for (int i = 0; i < prop->nextVal; ++i)
    {
        if (prop->vals[i].type == DATATYPE_IDENTIFIER)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].id), "one"));
        }
        else if (prop->vals[i].type == DATATYPE_NUMBER)
        {
            TEST_ANON (prop->vals[i].numVal, 3);
        }
        else if (prop->vals[i].type == DATATYPE_STRING)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].str), "test"));
        }
    }
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].id), "propVal"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].str), "string"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_ANON (prop->vals[0].numVal, 0x20);
    entry = ListIterate (mainEnt);
    block = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockName), "test"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (block->blockType), "block"));
    entry = ListFront (block->props);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "test"));
    for (int i = 0; i < prop->nextVal; ++i)
    {
        if (prop->vals[i].type == DATATYPE_IDENTIFIER)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].id), "one"));
        }
        else if (prop->vals[i].type == DATATYPE_NUMBER)
        {
            TEST_ANON (prop->vals[i].numVal, 3);
        }
        else if (prop->vals[i].type == DATATYPE_STRING)
        {
            TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[i].str), "test"));
        }
    }
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].id), "propVal"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->vals[0].str), "string"));
    entry = ListIterate (entry);
    prop = ListEntryData (entry);
    TEST_BOOL_ANON (!strcmp (StrRefGet (prop->name), "prop"));
    TEST_ANON (prop->vals[0].numVal, 0x20);
    ConfFreeParseTree (ctx);
    return 0;
}
