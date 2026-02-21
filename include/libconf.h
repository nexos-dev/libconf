/*
    libconf.h - contains configuration file parser header
    Copyright 2021, 2022 The NexNix Project

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

/// @file conf.h

#ifndef CONF_H
#define CONF_H

#include <libconf/libconf_config.h>
#include <libnex/list.h>
#include <libnex/stringref.h>
#include <stdio.h>

#define MAX_PROPVAR 16    // The maximum amount of values in a property

#define DATATYPE_IDENTIFIER 0    ///< Value of property is a identifier
#define DATATYPE_STRING     1    ///< Value of property is a string
#define DATATYPE_NUMBER     2    ///< Value of property is a number

///< The value of a property
typedef struct tagPropertyValue
{
    int lineNo;    ///< The line number of this property value
    union          ///< The value of this property
    {
        StringRef_t* id;     ///< An identifier
        StringRef_t* str;    /// ... or a string
        int64_t numVal;      ///< ... or a number
    };
    int type;    ///< 0 = identifier, 1 = string, 2 = numeric
} ConfPropVal_t;

/// A property. Properties are what define characteristics of what is being
/// configured
typedef struct tagProperty
{
    ListEntry_t link;
    int lineNo;           ///< The line number of this property declaration
    StringRef_t* name;    ///< The property represented here
    ConfPropVal_t vals[MAX_PROPVAR];    ///< 64 comma seperated values
    int nextVal;                        ///< The next value to work with
} ConfProperty_t;

/**
 * @brief Contains a block for the parse tree
 *
 * A block is the top level data structure in confparse. It contains information
 * about individual keys
 */

typedef struct tagBlock
{
    ListEntry_t link;
    int lineNo;    ///< The line number of this block declaration in the source file
    StringRef_t* blockType;    ///< What this block specifies
    StringRef_t* blockName;    ///< The name of this block
    ListHead_t* props;         ///< The list of properties associated with this block
} ConfBlock_t;

/**
 * @brief Libconf main data structure
 */
typedef struct lconf
{
    ListHead_t* blocks;      // Blocks in the configuration file
    FILE* log;               // Log file
    const char* fileName;    // Name of file
    int error;               // Code of error
    char errBuf[256];        // Error message buffer
} LibConf_t;

#define LIBCONF_ERROR_OOM   1
#define LIBCONF_ERROR_LEX   2
#define LIBCONF_ERROR_PARSE 3
#define LIBCONF_ERROR_SYS   4

// Iterator structure
typedef struct _blkiter
{
    ListEntry_t* cur;    // Current place
} ConfIter_t;

/**
 * @brief Initializes configuration context
 * Takes a file name and parses the file, and returns the parse list
 * @param file the file to read configuration from
 * @param log log file to print errors to. NULL for stderr
 * @return The list of blocks
 */
LIBCONF_PUBLIC LibConf_t* ConfInit (const char* file, FILE* log);

/**
 * @brief Frees all memory associated with parse tree
 * @param ctx context of libconf
 */
LIBCONF_PUBLIC void ConfFreeParseTree (LibConf_t* ctx);

/**
 * @brief Iterates through every block in tree
 * @param ctx context of libconf
 * @param iter iterator
 * @return next block, null if end reached
 */
LIBCONF_PUBLIC ConfBlock_t* ConfNextBlock (LibConf_t* ctx, ConfIter_t* iter);

/**
 * @brief Iterate through every property in a block
 * @param ctx context of libconf
 * @param block block to iterate through
 * @param iter iterator
 * @return next property, null if end reached
 */
LIBCONF_PUBLIC ConfProperty_t* ConfNextProp (LibConf_t* ctx,
                                             ConfBlock_t* block,
                                             ConfIter_t* iter);

/**
 * @brief Finds a particlar block by name and type
 * @param ctx context of libconf
 * @param type block type
 * @param name block name
 * @return Found block, null if non-existant
 */
LIBCONF_PUBLIC ConfBlock_t* ConfFindBlock (LibConf_t* ctx,
                                           const char* type,
                                           const char* name);

/**
 * @brief Finds a particular property in a block
 * @param ctx context of libconf
 * @param block block to look in
 * @param prop name of property
 * @return found property, null in non-existant
 */
LIBCONF_PUBLIC ConfProperty_t* ConfFindProp (LibConf_t* ctx,
                                             ConfBlock_t* block,
                                             const char* prop);

#endif
