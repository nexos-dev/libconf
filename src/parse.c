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
    lexState_t* lex;            // Underlying lexer of this parser
    ListHead_t* head;           // Linked list for configuration block
    _confToken_t* lastToken;    // So we can backtrack a little during errors
} parseState_t;

// Parser error states
#define PARSE_ERROR_UNEXPECTED_TOKEN 1
#define PARSE_ERROR_INTERNAL         2
#define PARSE_ERROR_OVERFLOW         3
#define PARSE_ERROR_TOO_MANY_PROPS   4

void _confSetFileName (const char* file);

static inline _confToken_t* _parseInclude (parseState_t*, _confToken_t*);

// Reports a diagnostic message
static void _parseError (parseState_t* parser,
                         _confToken_t* tok,
                         int err,
                         void* extra)
{
    // Prepare error buffer
    char bufData[2048];

    char* obuf = bufData;
    char* buf = bufData;
    buf += snprintf (buf, 2048 - (buf - obuf), "error: %s:", ConfGetFileName());
    buf += snprintf (buf, 2048 - (buf - obuf), "%d: ", tok->line);
    // Decide how to handle the error
    switch (err)
    {
        case PARSE_ERROR_UNEXPECTED_TOKEN:
            if (parser->lastToken)
            {
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 "unexpected token %s after token %s",
                                 _confLexGetTokenName (tok),
                                 _confLexGetTokenName (parser->lastToken));
            }
            else
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 "unexpected token %s",
                                 _confLexGetTokenName (tok));
            // Add some context
            if (extra)
            {
                buf += snprintf (buf,
                                 2048 - (buf - obuf),
                                 " (expected %s)",
                                 _confLexGetTokenNameType (*((int*) extra)));
            }
            break;
        case PARSE_ERROR_OVERFLOW:
            buf += snprintf (buf,
                             2048 - (buf - obuf),
                             "string too long on token %s",
                             _confLexGetTokenName (tok));
            break;
        case PARSE_ERROR_TOO_MANY_PROPS:
            buf += snprintf (buf,
                             2048 - (buf - obuf),
                             "too many properties on property '%s'",
                             (char*) extra);
            break;
        case PARSE_ERROR_INTERNAL:
            buf += snprintf (buf,
                             2048 - (buf - obuf),
                             "internal error: %s",
                             (char*) extra);
            break;
    }
    // Silence clang-tidy warnings about buf being unused
    (void) buf;
    error (obuf);
}

// Accepts a new token, saving last one
_confToken_t* _parseToken (parseState_t* state, _confToken_t* lastTok)
{
    // Free token
    if (state->lastToken)
        free (state->lastToken);
    state->lastToken = lastTok;
    _confToken_t* tok = _confLex (state->lex);
    if (tok->type == LEX_TOKEN_ERROR)
        return NULL;
    return tok;
}

// Expects a specified token to exist
_confToken_t* _parseExpect (parseState_t* state, _confToken_t* lastTok, int tokType)
{
    _confToken_t* tok = _parseToken (state, lastTok);
    // Ensure this token is the one expected
    if (!tok)
        return NULL;
    else if (tok->type != tokType)
    {
        _parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, &tokType);
        return NULL;
    }
    return tok;
}

// Parses a block in the configuration file
static _confToken_t* _parseBlock (parseState_t* state, _confToken_t* tok)
{
    // Create a new block and add it to list
    ConfBlock_t* block = (ConfBlock_t*) malloc_s (sizeof (ConfBlock_t));
    if (!block)
        return NULL;
    if (!ListAddBack (state->head, block, 0))
        return NULL;
    // Initialize it
    block->lineNo = tok->line;
    block->props = ListCreate ("ConfProperty", false, 0);
    // Set type of block
    if (c32lcpy (block->blockType, tok->semVal, 256) >= 256)
    {
        _parseError (state, tok, PARSE_ERROR_OVERFLOW, NULL);
        return NULL;
    }
    // Check if block has a name
    tok = _parseToken (state, tok);
    if (!tok)
        return NULL;
    if (tok->type == LEX_TOKEN_ID)
    {
        // Set name of block
        if (c32lcpy (block->blockName, tok->semVal, BLOCK_BUFSZ) >= BLOCK_BUFSZ)
        {
            _parseError (state, tok, PARSE_ERROR_OVERFLOW, NULL);
            return NULL;
        }
        // Get a opening brace
        tok = _parseExpect (state, tok, LEX_TOKEN_OBRACE);
        if (!tok)
            return NULL;
    }
    else if (tok->type == LEX_TOKEN_OBRACE)
        block->blockName[0] = 0;
    else
    {
        _parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
        return NULL;
    }

    // Begin reading in tokens for properties
    while (1)
    {
        tok = _parseToken (state, tok);
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
                (ConfProperty_t*) malloc_s (sizeof (ConfProperty_t));
            if (!prop)
                return NULL;
            ListAddBack (block->props, prop, 0);
            prop->lineNo = tok->line;
            if (c32lcpy (prop->name, tok->semVal, BLOCK_BUFSZ) >= BLOCK_BUFSZ)
            {
                _parseError (state, tok, PARSE_ERROR_OVERFLOW, NULL);
                return NULL;
            }
            prop->nextVal = 0;
            // Expect a colon
            tok = _parseExpect (state, tok, LEX_TOKEN_COLON);
            if (!tok)
                return NULL;
            // Now parse all the values
            while (1)
            {
                tok = _parseToken (state, tok);
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
                    if (c32lcpy (prop->vals[valLoc].str, tok->semVal, BLOCK_BUFSZ) >=
                        BLOCK_BUFSZ)
                    {
                        _parseError (state, tok, PARSE_ERROR_OVERFLOW, NULL);
                        return NULL;
                    }
                    ++prop->nextVal;
                    if (prop->nextVal >= MAX_PROPVAR)
                    {
                        _parseError (state,
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
                    if (c32lcpy (prop->vals[valLoc].id, tok->semVal, BLOCK_BUFSZ) >=
                        BLOCK_BUFSZ)
                    {
                        _parseError (state, tok, PARSE_ERROR_OVERFLOW, NULL);
                        return NULL;
                    }
                    ++prop->nextVal;
                    if (prop->nextVal >= MAX_PROPVAR)
                    {
                        _parseError (state,
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
                        _parseError (state,
                                     tok,
                                     PARSE_ERROR_TOO_MANY_PROPS,
                                     prop->name);
                        return NULL;
                    }
                }
                else
                {
                    _parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
                    return NULL;
                }

                // Check if there is another property
                tok = _parseToken (state, tok);
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
                    _parseError (state, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
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
static bool _parseInternal (parseState_t* parser)
{
    bool res = true;
    // Start parsing
    _confToken_t* tok = _parseToken (parser, NULL);
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
            tok = _parseInclude (parser, tok);
            ERROR_OUT_MAYBE
        }
        // ... or it has to be a block
        else if (tok->type == LEX_TOKEN_ID)
        {
            tok = _parseBlock (parser, tok);
            ERROR_OUT_MAYBE
        }
        else
        {
            _parseError (parser, tok, PARSE_ERROR_UNEXPECTED_TOKEN, NULL);
            res = false;
            goto end;
        }
        tok = _parseToken (parser, tok);
        ERROR_OUT_MAYBE
    }
    // Destroy the lexer
    _confLexDestroy (parser->lex);
// Free up unfreed tokens
end:
    free (parser->lastToken);
    free (tok);
    return res;
}

// Includes another file to parse
static inline _confToken_t* _parseInclude (parseState_t* state, _confToken_t* tok)
{
    _confToken_t* pathTok = _parseExpect (state, tok, LEX_TOKEN_STR);
    if (!pathTok)
        return NULL;
    // Convert string value to multibyte
    size_t len = c32len (pathTok->semVal);
    char* mbPath = malloc_s ((len * MB_CUR_MAX) + 1);
    mbstate_t mbState = {0};
    if (c32stombs (mbPath, pathTok->semVal, len, &mbState) < 0)
    {
        _parseError (state, pathTok, PARSE_ERROR_INTERNAL, strerror (errno));
        return NULL;
    }
    // Set file name
    const char* oldFile = ConfGetFileName();
    _confSetFileName (mbPath);
    // Create a new parser context
    parseState_t newState;
    newState.lex = _confLexInit (mbPath);
    if (!newState.lex)
        return NULL;
    newState.lastToken = NULL;
    newState.head = state->head;
    // Start parsing the include
    if (!_parseInternal (&newState))
        return NULL;
    _confSetFileName (oldFile);
    return pathTok;
}

ListHead_t* _confParse (const char* file)
{
    // Initialize the lexer
    lexState_t* lexState = _confLexInit (file);
    if (!lexState)
        return NULL;
    // Start parsing
    ConfBlock_t* block = NULL;
    parseState_t state = {0};
    state.lex = lexState;
    state.head = ListCreate ("ConfBlock", false, 9);
    if (!_parseInternal (&state))
    {
        ConfFreeParseTree (state.head);
        return NULL;
    }
    return state.head;
}
