/*
 *  NASA Docket No. GSC-18,370-1, and identified as "Operating System Abstraction Layer"
 *
 *  Copyright (c) 2019 United States Government as represented by
 *  the Administrator of the National Aeronautics and Space Administration.
 *  All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * File: osapi-os-loader.h
 *
 * Author:  Alan Cudmore - Code 582
 *
 * Purpose: Contains functions prototype definitions and variables declarations
 *          for the OS Abstraction Layer, Object file loader API
 */

#ifndef _osapi_loader_
#define _osapi_loader_

/*
** Defines
*/


/*
** Typedefs
*/

/** @brief OSAL module address properties */
typedef struct
{
   uint32 valid;
   uint32 flags;  
   cpuaddr code_address;
   cpuaddr code_size;
   cpuaddr data_address;
   cpuaddr data_size;
   cpuaddr bss_address;
   cpuaddr bss_size;
} OS_module_address_t;

/** @brief OSAL module properties */
typedef struct
{
   cpuaddr             entry_point;
   cpuaddr             host_module_id;
   char                filename[OS_MAX_PATH_LEN];
   char                name[OS_MAX_API_NAME];
   OS_module_address_t addr;
} OS_module_prop_t;

/**
 * @brief Associates a single symbol name with a memory address.
 *
 * If the OS_STATIC_SYMBOL_TABLE feature is enabled, then
 * an array of these structures should be provided by the
 * application.  When the application needs to find a symbol
 * address, the static table will be checked in addition
 * to (or instead of) the OS/library-provided lookup function.
 *
 * This static symbol allows systems that do not implement
 * dynamic module loading to maintain the same semantics
 * as dynamically loaded modules.
 */
typedef const struct
{
   const char *Name;
   void (*Address)(void);
   const char *Module;
} OS_static_symbol_record_t;

#ifndef OSAL_OMIT_DEPRECATED
/*
 * Define the former "OS_module_record_t" type as equivalent
 * to the OS_module_prop_t.  This is what the OS_ModuleInfo()
 * will output.  It used to be the same as the internal record
 * just without all the fields filled in.  This has been changed
 * to make it a separate structure, which will allow the internal
 * implementation to change without further changing the API.
 *
 * Ideally OS_module_record_t type should be removed to avoid confusion,
 * but this would break existing code that calls OS_ModuleInfo().
 */
typedef OS_module_prop_t OS_module_record_t; /**< @deprecated Use OS_module_prop_t */
#endif

/** @defgroup OSAPILoader OSAL Dynamic Loader and Symbol APIs
 * @{
 */

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Find the Address of a Symbol
 *
 * This calls to the OS dynamic symbol lookup implementation,
 * and/or checks a static symbol table for a matching symbol name.
 *
 * The static table is intended to support embedded targets that do
 * not have module loading capability or have it disabled.
 *
 * @param[out] symbol_address Set to the address of the symbol
 * @param[in]  symbol_name    Name of the symbol to look up
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_ERROR if the symbol could not be found
 * @retval #OS_INVALID_POINTER if one of the pointers passed in are NULL
 */
int32 OS_SymbolLookup (cpuaddr *symbol_address, const char *symbol_name );

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Dumps the system symbol table to a file
 *
 * Dumps the system symbol table to the specified filename
 *
 * @param[in] filename  File to write to
 * @param[in] size_limit Maximum number of bytes to write
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS             @copybrief OS_SUCCESS
 * @retval #OS_ERR_NOT_IMPLEMENTED @copybrief OS_ERR_NOT_IMPLEMENTED
 * @retval #OS_ERROR if the symbol table could not be read or dumped
 */
int32 OS_SymbolTableDump ( const char *filename, uint32 size_limit );

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Loads an object file
 *
 * Loads an object file into the running operating system
 *
 * @param[out] module_id    Non-zero OSAL ID corresponding to the loaded module
 * @param[in]  module_name  Name of module
 * @param[in]  filename     File containing the object code to load
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_ERROR if the module cannot be loaded
 * @retval #OS_INVALID_POINTER if one of the parameters is NULL
 * @retval #OS_ERR_NO_FREE_IDS if the module table is full
 * @retval #OS_ERR_NAME_TAKEN if the name is in use
 */
int32 OS_ModuleLoad ( uint32 *module_id, const char *module_name, const char *filename );

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Unloads the module file
 *
 * Unloads the module file from the running operating system
 *
 * @param[in] module_id    OSAL ID of the previously the loaded module
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_ERROR if the module is invalid or cannot be unloaded
 */
int32 OS_ModuleUnload ( uint32 module_id );

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Obtain information about a module
 *
 * Returns information about the loadable module
 *
 * @param[in]  module_id    OSAL ID of the previously the loaded module
 * @param[out] module_info  Buffer to store module information
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_ERR_INVALID_ID if the module id invalid
 * @retval #OS_INVALID_POINTER if the pointer to the ModuleInfo structure is invalid
 */
int32 OS_ModuleInfo ( uint32 module_id, OS_module_prop_t *module_info );
/**@}*/

#endif
