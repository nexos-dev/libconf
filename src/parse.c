/*
    parse.c - contains recursive descent parser for confparse
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

#include "internal.h"
#include <errno.h>
#include <libconf.h>
#include <libnex/error.h>
#include <libnex/list.h>
#include <libnex/safemalloc.h>
#include <libnex/safestring.h>
#include <stdlib.h>
#include <string.h>

// State of the parser
typedef struct _parser
{
    lexState_t* lex;           // Underlying lexer of this parser
    ListHead_t* head;          // Linked list for configuration block
    confToken_t* lastToken;    // So we can backtrack a little during errors
    LibConf_t* ctx;            // Libconf context
} parseState_t;

// Parser error states
#define PARSE_ERROR_UNEXPECTED_TOKEN 1
#define PARSE_ERROR_INTERNAL         2
#define PARSE_ERROR_OVERFLOW         3
#define PARSE_ERROR_TOO_MANY_PROPS   4

static inline confToken_t* parseInclude (parseState_t*, confToken_t*);

// Destroys a token
static void parseDestroyBlock (const void* data)
{
    ConfBlock_t* block = (ConfBlock_t*) data;
    StrRefDestroy (block->blockType);
    if (block->blockName)
        StrRefDestroy (block->blockName);
    ListDestroy (block->props);
    free (block);
}

// Destroys a property
static void parseDestroyProp (const void* data)
{
    ConfProperty_t* prop = (ConfProperty_t*) data;
    StrRefDestroy (prop->name);
    for (int i = 0; i < prop->nextVal; ++i)
    {
        if (prop->vals[i].type == DATATYPE_STRING)
            StrRefDestroy (prop->vals[i].str);
        else if (prop->vals[i].type == DATATYPE_IDENTIFIER)
            StrRefDestroy (prop->vals[i].id);
    }
    free (prop);
}

// Reports a diagnostic message
static void parseDiagnostic (parseState_t* parser,
                             confToken_t* tok,
                             int err,
                             void* extra)
{
    // Prepare error buffer
    char header[64];
    char* obuf = parser->ctx->errBuf;
    char* buf = parser->ctx->errBuf;
    const char* file = parser->ctx->fileName;

    if (err != PARSE_ERROR_INTERNAL)
    {
        sprintf (header, "libconf: error: %s:", file);
        buf += snprintf (buf, 2048 - (buf - obuf), "%d: ", tok->line);
    }
    else
        sprintf (header, "libconf: internal error: ");

    FILE* logFile = parser->ctx->log;

    // Decide how to handle the error
    switch (err)
    {
        case PARSE_ERROR_UNEXPECTED_TOKEN:
            if (parser->lastToken)
            {
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 "unexpected token %s after token %s",
                                 confLexGetTokenName (tok),
                                 confLexGetTokenName (parser->lastToken));
            }
            else
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 "unexpected token %s",
                                 confLexGetTokenName (tok));
            // Add some context
            if (extra)
            {
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 " (expected %s)",
                                 confLexGetTokenNameType (*((int*) extra)));
            }
            break;
        case PARSE_ERROR_OVERFLOW:
            buf += snprintf (buf,
                             2048 - (buf - obuf),
                             "string too long on token %s",
                             confLexGetTokenName (tok));
            break;
        case PARSE_ERROR_TOO_MANY_PROPS:
            buf += snprintf (buf,
                             2048 - (buf - obuf),
                             "too many values on property '%s'",
                             (char*) extra);
            break;
        case PARSE_ERROR_INTERNAL:
            buf += snprintf (buf, 2048 - (buf - obuf), "%s", (char*) extra);
            break;
    }
    fprintf (logFile, "%s", header);
    fprintf (logFile, "%s\n", obuf);
}

// Parse internal error
static void parseErrorInternal (parseState_t* parser, const char* str)
{
    LibConf_t* ctx = parser->ctx;
    ctx->error = LIBCONF_ERROR_PARSE;
    parseDiagnostic (parser, NULL, PARSE_ERROR_INTERNAL, (void*) str);
}

// Parse system error
static void parseErrorSys (parseState_t* parser)
{
    LibConf_t* ctx = parser->ctx;
    ctx->error = LIBCONF_ERROR_SYS;
    parseDiagnostic (parser, NULL, PARSE_ERROR_INTERNAL, strerror (errno));
}

// Parse oom
static void parseErrorOom (parseState_t* parser)
{
    LibConf_t* ctx = parser->ctx;
    ctx->error = LIBCONF_ERROR_OOM;
    parseDiagnostic (parser, NULL, PARSE_ERROR_INTERNAL, "out of memory");
}

// General parse error
static void parseError (parseState_t* parser, confToken_t* tok, int err, void* extra)
{
    LibConf_t* ctx = parser->ctx;
    ctx->error = LIBCONF_ERROR_PARSE;
    parseDiagnostic (parser, tok, err, extra);
}

// Accepts a new token, saving last one
confToken_t* parseToken (parseState_t* state, confToken_t* lastTok)
{
    // Free token
    if (state->lastToken)
    {
        if (state->lastToken->semVal)
            StrRefDestroy (state->lastToken->semVal);
        free (state->lastToken);
    }
    state->lastToken = lastTok;
    confToken_t* tok = confLex (state->lex);
    if (tok->type == LEX_TOKEN_ERROR)
        return NULL;
    return tok;
}

// Expects a specified token to exist
confToken_t* parseExpect (parseState_t* state, confToken_t* lastTok, int tokType)
{
    confToken_t* tok = parseToken (state, lastTok);
    // Ensure this token is the one expected
    if (!tok)
        return NULL;
    else if (tok->type != tokType)
    {
        parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, &tokType);
        return NULL;
    }
    return tok;
}

// Parses a block in the configuration file
static confToken_t* parseBlock (parseState_t* state, confToken_t* tok)
{
    // Create a new block and add it to list
    ConfBlock_t* block = (ConfBlock_t*) malloc (sizeof (ConfBlock_t));
    if (!block)
        return NULL;
    if (!ListAddBack (state->head, block, 0))
        return NULL;
    // Initialize it
    block->lineNo = tok->line;
    block->props = ListCreate ("ConfProperty", NULL, parseDestroyProp, 0);
    // Set type of block
    block->blockType = StrRefNew (tok->semVal);
    // Check if block has a name
    tok = parseToken (state, tok);
    if (!tok)
        return NULL;
    if (tok->type == LEX_TOKEN_ID)
    {
        // Set name of block
        block->blockName = StrRefNew (tok->semVal);
        // Get a opening brace
        tok = parseExpect (state, tok, LEX_TOKEN_OBRACE);
        if (!tok)
            return NULL;
    }
    else if (tok->type == LEX_TOKEN_OBRACE)
        block->blockName = NULL;
    else
    {
        parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
        return NULL;
    }

    // Begin reading in tokens for properties
    while (1)
    {
        tok = parseToken (state, tok);
        if (!tok)
            return NULL;
        // Is this the end of the block?
        if (tok->type == LEX_TOKEN_EBRACE)
            break;
        // Or a property ID?
        if (tok->type == LEX_TOKEN_ID)
        {
            // Create a new property
            ConfProperty_t* prop =
                (ConfProperty_t*) malloc (sizeof (ConfProperty_t));
            if (!prop)
                return NULL;
            ListAddBack (block->props, prop, 0);
            prop->lineNo = tok->line;
            prop->name = StrRefNew (tok->semVal);
            prop->nextVal = 0;
            // Expect a colon
            tok = parseExpect (state, tok, LEX_TOKEN_COLON);
            if (!tok)
                return NULL;
            // Now parse all the values
            while (1)
            {
                tok = parseToken (state, tok);
                if (!tok)
                    return NULL;
                // It this a string?
                if (tok->type == LEX_TOKEN_STR)
                {
                    // Initialize values
                    int valLoc = prop->nextVal;
                    prop->vals[valLoc].lineNo = tok->line;
                    prop->vals[valLoc].type = DATATYPE_STRING;
                    // Copy string value
                    prop->vals[valLoc].str = StrRefNew (tok->semVal);
                    ++prop->nextVal;
                    if (prop->nextVal >= MAX_PROPVAR)
                    {
                        parseError (state,
                                    tok,
                                    PARSE_ERROR_TOO_MANY_PROPS,
                                    prop->name);
                        return NULL;
                    }
                }
                // .. or an identifier?
                else if (tok->type == LEX_TOKEN_ID)
                {
                    // Same thing
                    int valLoc = prop->nextVal;
                    prop->vals[valLoc].lineNo = tok->line;
                    prop->vals[valLoc].type = DATATYPE_IDENTIFIER;
                    // Copy string value
                    prop->vals[valLoc].id = StrRefNew (tok->semVal);
                    ++prop->nextVal;
                    if (prop->nextVal >= MAX_PROPVAR)
                    {
                        parseError (state,
                                    tok,
                                    PARSE_ERROR_TOO_MANY_PROPS,
                                    prop->name);
                        return NULL;
                    }
                }
                // ... or a number?
                else if (tok->type == LEX_TOKEN_NUM)
                {
                    int valLoc = prop->nextVal;
                    prop->vals[valLoc].lineNo = tok->line;
                    prop->vals[valLoc].type = DATATYPE_NUMBER;
                    prop->vals[valLoc].numVal = tok->num;
                    ++prop->nextVal;
                    if (prop->nextVal >= MAX_PROPVAR)
                    {
                        parseError (state,
                                    tok,
                                    PARSE_ERROR_TOO_MANY_PROPS,
                                    prop->name);
                        return NULL;
                    }
                }
                else
                {
                    parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
                    return NULL;
                }

                // Check if there is another property
                tok = parseToken (state, tok);
                if (!tok)
                    return NULL;
                // Should we continue?
                if (tok->type == LEX_TOKEN_COMMA)
                    continue;
                // Should we stop?
                else if (tok->type == LEX_TOKEN_SEMICOLON)
                    break;
                else
                {
                    parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
                    return NULL;
                }
            }
        }
    }
    return tok;
}

#define ERROR_OUT_MAYBE \
    if (!tok)           \
    {                   \
        res = false;    \
        goto end;       \
    }

// Internal parser. Performance critical
static bool parseInternal (parseState_t* parser)
{
    bool res = true;
    // Start parsing
    confToken_t* tok = parseToken (parser, NULL);
    if (!tok)
    {
        res = false;
        goto end;
    }
    while (tok->type != LEX_TOKEN_NONE)
    {
        // Is it an include statement?
        if (tok->type == LEX_TOKEN_INCLUDE)
        {
            tok = parseInclude (parser, tok);
            ERROR_OUT_MAYBE
        }
        // ... or it has to be a block
        else if (tok->type == LEX_TOKEN_ID)
        {
            tok = parseBlock (parser, tok);
            ERROR_OUT_MAYBE
        }
        else
        {
            parseError (parser, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
            res = false;
            goto end;
        }
        tok = parseToken (parser, tok);
        ERROR_OUT_MAYBE
    }
    // Destroy the lexer
    confLexDestroy (parser->lex);
// Free up unfreed tokens
end:
    free (parser->lastToken);
    free (tok);
    return res;
}

// Includes another file to parse
static inline confToken_t* parseInclude (parseState_t* state, confToken_t* tok)
{
    confToken_t* pathTok = parseExpect (state, tok, LEX_TOKEN_STR);
    if (!pathTok)
        return NULL;
    // Set file name
    const char* oldFile = state->ctx->fileName;
    state->ctx->fileName = StrRefGet (pathTok->semVal);
    // Create a new parser context
    parseState_t newState;
    newState.lex = confLexInit (state->ctx, StrRefGet (pathTok->semVal));
    if (!newState.lex)
        return NULL;
    newState.lastToken = NULL;
    newState.head = state->head;
    // Start parsing the include
    if (!parseInternal (&newState))
        return NULL;
    state->ctx->fileName = oldFile;
    return pathTok;
}

ListHead_t* confParse (LibConf_t* ctx, const char* file)
{
    // Initialize the lexer
    lexState_t* lexState = confLexInit (ctx, file);
    if (!lexState)
        return NULL;
    // Start parsing
    ConfBlock_t* block = NULL;
    parseState_t state = {0};
    state.lex = lexState;
    state.head = ListCreate ("ConfBlock", NULL, parseDestroyBlock, 0);
    state.ctx = ctx;
    if (!parseInternal (&state))
    {
        ListDestroy (state.head);
        return NULL;
    }
    return state.head;
}
