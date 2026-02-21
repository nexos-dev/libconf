/*
    conf.c - contains configuration file parser core
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

/// @file conf.c

#include "internal.h"
#include <assert.h>
#include <libconf.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LIBCONF_PUBLIC LibConf_t* ConfInit (const char* file, FILE* log)
{
    if (!file)
        return NULL;
    LibConf_t* ctx = (LibConf_t*) calloc (1, sizeof (LibConf_t));
    if (!ctx)
        return NULL;
    ctx->log = log;
    if (!ctx->log)
        ctx->log = stderr;
    ctx->fileName = file;
    ctx->blocks = confParse (ctx, file);
    return ctx;
}

LIBCONF_PUBLIC void ConfFreeParseTree (LibConf_t* ctx)
{
    if (!ctx)
        return;
    // Destroy the list
    ListDestroy (ctx->blocks);
    if (ctx->log != stderr)
        fclose (ctx->log);
}

LIBCONF_PUBLIC ConfBlock_t* ConfNextBlock (LibConf_t* ctx, ConfIter_t* iter)
{
    if (!ctx || !iter)
        return NULL;
    // Check if iter is started
    if (!iter->cur)
        iter->cur = ListFront (ctx->blocks);
    else
        iter->cur = ListIterate (iter->cur);
    return (iter->cur) ? ListEntryData (iter->cur) : NULL;
}

LIBCONF_PUBLIC ConfProperty_t* ConfNextProp (LibConf_t* ctx,
                                             ConfBlock_t* block,
                                             ConfIter_t* iter)
{
    if (!ctx || !block || !iter)
        return NULL;
    // Check if iter is started
    if (!iter->cur)
        iter->cur = ListFront (block->props);
    else
        iter->cur = ListIterate (iter->cur);
    return (iter->cur) ? ListEntryData (iter->cur) : NULL;
}

LIBCONF_PUBLIC ConfBlock_t* ConfFindBlock (LibConf_t* ctx,
                                           const char* type,
                                           const char* name)
{
    if (!ctx || !type || !name)
        return NULL;
    ListEntry_t* iter = ListFront (ctx->blocks);
    do
    {
        ConfBlock_t* block = ListEntryData (iter);
        if (!strcmp (StrRefGet (block->blockName), name) &&
            !strcmp (StrRefGet (block->blockType), type))
        {
            return block;
        }
        iter = ListIterate (iter);
    } while (iter);
    return NULL;
}

LIBCONF_PUBLIC ConfProperty_t* ConfFindProp (LibConf_t* ctx,
                                             ConfBlock_t* block,
                                             const char* propName)
{
    if (!ctx || !block || !propName)
        return NULL;
    ListEntry_t* iter = ListFront (block->props);
    do
    {
        ConfProperty_t* prop = ListEntryData (iter);
        if (!strcmp (StrRefGet (prop->name), propName))
        {
            return prop;
        }
        iter = ListIterate (iter);
    } while (iter);
    return NULL;
}
