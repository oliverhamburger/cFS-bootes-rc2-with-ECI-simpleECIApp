/*
**  GSC-18128-1, "Core Flight Executive Version 6.7"
**
**  Copyright (c) 2006-2019 United States Government as represented by
**  the Administrator of the National Aeronautics and Space Administration.
**  All Rights Reserved.
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
*/

/*
**  File:
**    cfe_es_apps.c
**
**  Purpose:
**    This file contains functions for starting cFE applications from a filesystem.
**
**  References:
**     Flight Software Branch C Coding Standard Version 1.0a
**     cFE Flight Software Application Developers Guide
**
**  Notes:
**
*/

/*
** Includes
*/
#include "private/cfe_private.h"
#include "cfe_es.h"
#include "cfe_psp.h"
#include "cfe_es_global.h"
#include "cfe_es_task.h"
#include "cfe_es_apps.h"
#include "cfe_es_log.h"

#include <stdio.h>
#include <string.h> /* memset() */
#include <fcntl.h>

/*
** Defines
*/
#define ES_START_BUFF_SIZE 128

/*
**
**  Global Variables
**
*/

/*
****************************************************************************
** Functions
***************************************************************************
*/

/*
** Name:
**   CFE_ES_StartApplications
**
** Purpose:
**   This routine loads/starts cFE applications.
**
*/
void CFE_ES_StartApplications(uint32 ResetType, const char *StartFilePath )
{
   char ES_AppLoadBuffer[ES_START_BUFF_SIZE];  /* A buffer of for a line in a file */
   const char *TokenList[CFE_ES_STARTSCRIPT_MAX_TOKENS_PER_LINE];
   uint32      NumTokens;
   uint32      BuffLen = 0;                            /* Length of the current buffer */
   int32       AppFile = 0;
   char        c;
   int32       ReadStatus;
   bool        LineTooLong = false;
   bool        FileOpened = false;

   /*
   ** Get the ES startup script filename.
   ** If this is a Processor Reset, try to open the file in the volatile disk first.
   */
   if ( ResetType == CFE_PSP_RST_TYPE_PROCESSOR )
   {
      /*
      ** Open the file in the volatile disk.
      */
      AppFile = OS_open( CFE_PLATFORM_ES_VOLATILE_STARTUP_FILE, OS_READ_ONLY, 0);

      if ( AppFile >= 0 )
      {
         CFE_ES_WriteToSysLog ("ES Startup: Opened ES App Startup file: %s\n",
                                CFE_PLATFORM_ES_VOLATILE_STARTUP_FILE);
         FileOpened = true;
      }
      else
      {
         CFE_ES_WriteToSysLog ("ES Startup: Cannot Open Volatile Startup file, Trying Nonvolatile.\n");
         FileOpened = false;
      }

   } /* end if */

   /*
   ** This if block covers two cases: A Power on reset, and a Processor reset when
   ** the startup file on the volatile file system could not be opened.
   */
   if ( FileOpened == false )
   {
      /*
      ** Try to Open the file passed in to the cFE start.
      */
      AppFile = OS_open( (const char *)StartFilePath, OS_READ_ONLY, 0);

      if ( AppFile >= 0 )
      {
         CFE_ES_WriteToSysLog ("ES Startup: Opened ES App Startup file: %s\n",StartFilePath);
         FileOpened = true;
      }
      else
      {
         CFE_ES_WriteToSysLog ("ES Startup: Error, Can't Open ES App Startup file: %s EC = 0x%08X\n",
                              StartFilePath, (unsigned int)AppFile );
         FileOpened = false;
      }

   }

   /*
   ** If the file is opened in either the Nonvolatile or the Volatile disk, process it.
   */
   if ( FileOpened == true)
   {
      memset(ES_AppLoadBuffer,0x0,ES_START_BUFF_SIZE);
      BuffLen = 0;
      NumTokens = 0;
      TokenList[0] = ES_AppLoadBuffer;

      /*
      ** Parse the lines from the file. If it has an error
      ** or reaches EOF, then abort the loop.
      */
      while(1)
      {
         ReadStatus = OS_read(AppFile, &c, 1);
         if ( ReadStatus == OS_ERROR )
         {
            CFE_ES_WriteToSysLog ("ES Startup: Error Reading Startup file. EC = 0x%08X\n",(unsigned int)ReadStatus);
            break;
         }
         else if ( ReadStatus == 0 )
         {
            /*
            ** EOF Reached
            */
            break;
         }
         else if(c != '!')
         {
             if ( c <= ' ')
             {
                /*
                ** Skip all white space in the file
                */
                ;
             }
             else if ( c == ',' )
             {
                /*
                ** replace the field delimiter with a null
                ** This is used to separate the tokens
                */
                if ( BuffLen < ES_START_BUFF_SIZE )
                {
                   ES_AppLoadBuffer[BuffLen] = 0;
                }
                else
                {
                   LineTooLong = true;
                }
                BuffLen++;

                if ( NumTokens < (CFE_ES_STARTSCRIPT_MAX_TOKENS_PER_LINE-1))
                {
                    /*
                     * NOTE: pointer never deferenced unless "LineTooLong" is false.
                     */
                    ++NumTokens;
                    TokenList[NumTokens] = &ES_AppLoadBuffer[BuffLen];
                }
             }
             else if ( c != ';' )
             {
                /*
                ** Regular data gets copied in
                */
                if ( BuffLen < ES_START_BUFF_SIZE )
                {
                   ES_AppLoadBuffer[BuffLen] = c;
                }
                else
                {
                   LineTooLong = true;
                }
                BuffLen++;
             }
             else
             {
                if ( LineTooLong == true )
                {
                   /*
                   ** The was too big for the buffer
                   */
                   CFE_ES_WriteToSysLog ("ES Startup: ES Startup File Line is too long: %u bytes.\n",(unsigned int)BuffLen);
                   LineTooLong = false;
                }
                else
                {
                   /*
                   ** Send the line to the file parser
                   ** Ensure termination of the last token and send it along
                   */
                   ES_AppLoadBuffer[BuffLen] = 0;
                   CFE_ES_ParseFileEntry(TokenList, 1 + NumTokens);
                }
                BuffLen = 0;
                NumTokens = 0;
             }
         }
         else
         {
           /*
           ** break when EOF character '!' is reached
           */
           break;
         }
      }
      /*
      ** close the file
      */
      OS_close(AppFile);

   }
}

/*
**---------------------------------------------------------------------------------------
** Name: CFE_ES_ParseFileEntry
**
**   Purpose: This function parses the startup file line for an individual
**            cFE application.
**---------------------------------------------------------------------------------------
*/
int32 CFE_ES_ParseFileEntry(const char **TokenList, uint32 NumTokens)
{
   const char   *FileName;
   const char   *AppName;
   const char   *EntryPoint;
   const char   *EntryType;
   unsigned int Priority;
   unsigned int StackSize;
   unsigned int ExceptionAction;
   uint32 ApplicationId;
   int32  CreateStatus = CFE_ES_ERR_APP_CREATE;

   /*
   ** Check to see if the correct number of items were parsed
   */
   if ( NumTokens < 8 )
   {
      CFE_ES_WriteToSysLog("ES Startup: Invalid ES Startup file entry: %u\n",(unsigned int)NumTokens);
      return (CreateStatus);
   }

   EntryType = TokenList[0];
   FileName = TokenList[1];
   EntryPoint = TokenList[2];
   AppName = TokenList[3];

   /*
    * NOTE: In previous CFE versions the sscanf() function was used to convert
    * these string values into integers.  This approach of using the pre-tokenized strings
    * and strtoul() is safer but the side effect is that it will also be more "permissive" in
    * what is accepted vs. rejected by this function.
    *
    * For instance if the startup script contains "123xyz", this will be converted to the value
    * 123 instead of triggering a validation failure as it would have in CFE <= 6.5.0.
    *
    * This permissive parsing should not be relied upon, as it may become more strict again in
    * future CFE revisions.
    */
   Priority = strtoul(TokenList[4], NULL, 0);
   StackSize = strtoul(TokenList[5], NULL, 0);
   ExceptionAction = strtoul(TokenList[7], NULL, 0);

   if(strcmp(EntryType,"CFE_APP")==0)
   {
      CFE_ES_WriteToSysLog("ES Startup: Loading file: %s, APP: %s\n",
                            FileName, AppName);

      /*
      ** Validate Some parameters
      ** Exception action should be 0 ( Restart App ) or
      ** 1 ( Processor reset ). If it's non-zero, assume it means
      ** reset CPU.
      */
      if ( ExceptionAction > CFE_ES_ExceptionAction_RESTART_APP )
          ExceptionAction = CFE_ES_ExceptionAction_PROC_RESTART;
      /*
      ** Now create the application
      */
      CreateStatus = CFE_ES_AppCreate(&ApplicationId, FileName,
                               EntryPoint, AppName, (uint32) Priority,
                               (uint32) StackSize, (uint32) ExceptionAction );
   }
   else if(strcmp(EntryType,"CFE_LIB")==0)
   {
      CFE_ES_WriteToSysLog("ES Startup: Loading shared library: %s\n",FileName);

      /*
      ** Now load the library
      */
      CreateStatus = CFE_ES_LoadLibrary(&ApplicationId, FileName,
                               EntryPoint, AppName);

   }
   else
   {
      CFE_ES_WriteToSysLog("ES Startup: Unexpected EntryType %s in startup file.\n",EntryType);
   }

   return (CreateStatus);

}

/*
**---------------------------------------------------------------------------------------
** Name: ES_AppCreate
**
**   Purpose: This function loads and creates a cFE Application.
**            This function can be called from the ES startup code when it
**            loads the cFE Applications from the disk using the startup script, or it
**            can be called when the ES Start Application command is executed.
**
**---------------------------------------------------------------------------------------
*/
int32 CFE_ES_AppCreate(uint32 *ApplicationIdPtr,
                       const char   *FileName,
                       const void   *EntryPointData,
                       const char   *AppName,
                       uint32  Priority,
                       uint32  StackSize,
                       uint32  ExceptionAction)
{
   cpuaddr StartAddr;
   int32   ReturnCode;
   uint32  i;
   bool    AppSlotFound;
   uint32  TaskId;
   uint32  ModuleId;

   /*
    * The FileName must not be NULL
    */
   if (FileName == NULL)
   {
       return CFE_ES_ERR_APP_CREATE;
   }

   /*
   ** Allocate an ES_AppTable entry
   */
   CFE_ES_LockSharedData(__func__,__LINE__);
   AppSlotFound = false;
   for ( i = 0; i < CFE_PLATFORM_ES_MAX_APPLICATIONS; i++ )
   {
      if ( CFE_ES_Global.AppTable[i].AppState == CFE_ES_AppState_UNDEFINED )
      {
         AppSlotFound = true;
         memset ( &(CFE_ES_Global.AppTable[i]), 0, sizeof(CFE_ES_AppRecord_t));
         /* set state EARLY_INIT for OS_TaskCreate below (indicates record is in use) */
         CFE_ES_Global.AppTable[i].AppState = CFE_ES_AppState_EARLY_INIT;
         break;
      }
   }
   CFE_ES_UnlockSharedData(__func__,__LINE__);

   /*
   ** If a slot was found, create the application
   */
   if ( AppSlotFound == true)
   {
      /*
      ** Load the module
      */
      ReturnCode = OS_ModuleLoad ( &ModuleId, AppName, FileName );

      /*
      ** If the Load was OK, then lookup the address of the entry point
      */
      if ( ReturnCode == OS_SUCCESS )
      {
         ReturnCode = OS_SymbolLookup( &StartAddr, (const char*)EntryPointData );
         if ( ReturnCode != OS_SUCCESS )
         {
             CFE_ES_WriteToSysLog("ES Startup: Could not find symbol:%s. EC = 0x%08X\n",
                     (const char*)EntryPointData, (unsigned int)ReturnCode);

             CFE_ES_LockSharedData(__func__,__LINE__);
             CFE_ES_Global.AppTable[i].AppState = CFE_ES_AppState_UNDEFINED; /* Release slot */
             CFE_ES_UnlockSharedData(__func__,__LINE__);

             /* Unload the module from memory, so that it does not consume resources */
             ReturnCode = OS_ModuleUnload(ModuleId);
             if ( ReturnCode != OS_SUCCESS ) /* There's not much we can do except notify */
             {
                CFE_ES_WriteToSysLog("ES Startup: Failed to unload APP: %s. EC = 0x%08X\n",
                        AppName, (unsigned int)ReturnCode);
             }

             return(CFE_ES_ERR_APP_CREATE);
         }
      }
      else /* load not successful */
      {
          CFE_ES_WriteToSysLog("ES Startup: Could not load cFE application file:%s. EC = 0x%08X\n",
                            FileName, (unsigned int)ReturnCode);

          CFE_ES_LockSharedData(__func__,__LINE__);
          CFE_ES_Global.AppTable[i].AppState = CFE_ES_AppState_UNDEFINED; /* Release slot */
          CFE_ES_UnlockSharedData(__func__,__LINE__);

          return(CFE_ES_ERR_APP_CREATE);
      }

      /*
      ** If the EntryPoint symbol was found, then start creating the App
      */
      CFE_ES_LockSharedData(__func__,__LINE__);
      /*
      ** Allocate and populate the ES_AppTable entry
      */
      CFE_ES_Global.AppTable[i].Type = CFE_ES_AppType_EXTERNAL;

      /*
      ** Fill out the parameters in the AppStartParams sub-structure
      */
      strncpy((char *)CFE_ES_Global.AppTable[i].StartParams.Name, AppName, OS_MAX_API_NAME);
      CFE_ES_Global.AppTable[i].StartParams.Name[OS_MAX_API_NAME - 1] = '\0';

      strncpy((char *)CFE_ES_Global.AppTable[i].StartParams.EntryPoint, (const char *)EntryPointData, OS_MAX_API_NAME);
      CFE_ES_Global.AppTable[i].StartParams.EntryPoint[OS_MAX_API_NAME - 1] = '\0';
      strncpy((char *)CFE_ES_Global.AppTable[i].StartParams.FileName, FileName, OS_MAX_PATH_LEN);
      CFE_ES_Global.AppTable[i].StartParams.FileName[OS_MAX_PATH_LEN - 1] = '\0';

      CFE_ES_Global.AppTable[i].StartParams.StackSize = StackSize;

      CFE_ES_Global.AppTable[i].StartParams.StartAddress = StartAddr;
      CFE_ES_Global.AppTable[i].StartParams.ModuleId = ModuleId;

      CFE_ES_Global.AppTable[i].StartParams.ExceptionAction = ExceptionAction;
      CFE_ES_Global.AppTable[i].StartParams.Priority = Priority;

      /*
      ** Fill out the Task Info
      */
      strncpy((char *)CFE_ES_Global.AppTable[i].TaskInfo.MainTaskName, AppName, OS_MAX_API_NAME);
      CFE_ES_Global.AppTable[i].TaskInfo.MainTaskName[OS_MAX_API_NAME - 1] = '\0';

      /*
      ** Fill out the Task State info
      */
      CFE_ES_Global.AppTable[i].ControlReq.AppControlRequest = CFE_ES_RunStatus_APP_RUN;
      CFE_ES_Global.AppTable[i].ControlReq.AppTimerMsec = 0;

      /*
      ** Create the primary task for the newly loaded task
      */
      ReturnCode = OS_TaskCreate(&CFE_ES_Global.AppTable[i].TaskInfo.MainTaskId,   /* task id */
                       AppName,             /* task name */
                       (osal_task_entry)StartAddr,   /* task function pointer */
                       NULL,                /* stack pointer */
                       StackSize,           /* stack size */
                       Priority,            /* task priority */
                       OS_FP_ENABLED);     /* task options */


      if(ReturnCode != OS_SUCCESS)
      {
         CFE_ES_SysLogWrite_Unsync("ES Startup: AppCreate Error: TaskCreate %s Failed. EC = 0x%08X!\n",
                       AppName,(unsigned int)ReturnCode);

         CFE_ES_Global.AppTable[i].AppState = CFE_ES_AppState_UNDEFINED;
         CFE_ES_UnlockSharedData(__func__,__LINE__);

         return(CFE_ES_ERR_APP_CREATE);
      }
      else
      {

         /*
         ** Record the ES_TaskTable entry
         */
         OS_ConvertToArrayIndex(CFE_ES_Global.AppTable[i].TaskInfo.MainTaskId, &TaskId);

         if ( CFE_ES_Global.TaskTable[TaskId].RecordUsed == true )
         {
            CFE_ES_SysLogWrite_Unsync("ES Startup: Error: ES_TaskTable slot in use at task creation!\n");
         }
         else
         {
            CFE_ES_Global.TaskTable[TaskId].RecordUsed = true;
         }
         CFE_ES_Global.TaskTable[TaskId].AppId = i;
         CFE_ES_Global.TaskTable[TaskId].TaskId = CFE_ES_Global.AppTable[i].TaskInfo.MainTaskId;
         strncpy((char *)CFE_ES_Global.TaskTable[TaskId].TaskName,
             (char *)CFE_ES_Global.AppTable[i].TaskInfo.MainTaskName,OS_MAX_API_NAME );
         CFE_ES_Global.TaskTable[TaskId].TaskName[OS_MAX_API_NAME - 1]='\0';
         CFE_ES_SysLogWrite_Unsync("ES Startup: %s loaded and created\n", AppName);
         *ApplicationIdPtr = i;

         /*
         ** Increment the registered App and Registered External Task variables.
         */
         CFE_ES_Global.RegisteredTasks++;
         CFE_ES_Global.RegisteredExternalApps++;

         CFE_ES_UnlockSharedData(__func__,__LINE__);

         return(CFE_SUCCESS);

      } /* End If OS_TaskCreate */
   }
   else /* appSlot not found */
   {
      CFE_ES_WriteToSysLog("ES Startup: No free application slots available\n");
      return(CFE_ES_ERR_APP_CREATE);
   }

} /* End Function */
/*
**---------------------------------------------------------------------------------------
** Name: CFE_ES_LoadLibrary
**
**   Purpose: This function loads and initializes a cFE Shared Library.
**
**---------------------------------------------------------------------------------------
*/
int32 CFE_ES_LoadLibrary(uint32       *LibraryIdPtr,
                         const char   *FileName,
                         const void   *EntryPointData,
                         const char   *LibName)
{
   CFE_ES_LibraryEntryFuncPtr_t FunctionPointer;
   CFE_ES_LibRecord_t *         LibSlotPtr;
   size_t                       StringLength;
   int32                        Status;
   uint32                       CheckSlot;
   uint32                       ModuleId;
   bool                         IsModuleLoaded;

   /*
    * First, should verify that the supplied "LibName" fits within the internal limit
    *  (currently sized to OS_MAX_API_NAME, but not assuming that will always be)
    */
   StringLength = strlen(LibName);
   if (StringLength >= sizeof(CFE_ES_Global.LibTable[0].LibName))
   {
       return CFE_ES_BAD_ARGUMENT;
   }

   /*
   ** Allocate an ES_LibTable entry
   */
   IsModuleLoaded = false;
   LibSlotPtr = NULL;
   FunctionPointer = NULL;
   ModuleId = 0;
   Status = CFE_ES_ERR_LOAD_LIB;    /* error that will be returned if no slots found */
   CFE_ES_LockSharedData(__func__,__LINE__);
   for ( CheckSlot = 0; CheckSlot < CFE_PLATFORM_ES_MAX_LIBRARIES; CheckSlot++ )
   {
      if (CFE_ES_Global.LibTable[CheckSlot].RecordUsed)
      {
          if (strcmp(CFE_ES_Global.LibTable[CheckSlot].LibName, LibName) == 0)
          {
              /*
               * Indicate to caller that the library is already loaded.
               * (This is when there was a matching LibName in the table)
               *
               * Do nothing more; not logging this event as it may or may
               * not be an error.
               */
              *LibraryIdPtr = CheckSlot;
              Status = CFE_ES_LIB_ALREADY_LOADED;
              break;
          }
      }
      else if (LibSlotPtr == NULL)
      {
         /* Remember list position as possible place for new entry. */
          LibSlotPtr = &CFE_ES_Global.LibTable[CheckSlot];
          *LibraryIdPtr = CheckSlot;
          Status = CFE_SUCCESS;
      }
      else
      {
         /* No action */
      }
   }

   if (Status == CFE_SUCCESS)
   {
       /* reserve the slot while still under lock */
       strcpy(LibSlotPtr->LibName, LibName);
       LibSlotPtr->RecordUsed = true;
   }

   CFE_ES_UnlockSharedData(__func__,__LINE__);

   /*
    * If any off-nominal condition exists, skip the rest of this logic.
    * Additionally write any extra information about what happened to syslog
    * Note - not logging "already loaded" conditions, as this is not necessarily an error.
    */
   if (Status != CFE_SUCCESS)
   {
       if (Status == CFE_ES_ERR_LOAD_LIB)
       {
           CFE_ES_WriteToSysLog("ES Startup: No free library slots available\n");
       }

       return Status;
   }

   /*
    * -------------------
    * IMPORTANT:
    *
    * there is now a reserved entry in the global library table,
    * which must be freed if something goes wrong hereafter.
    *
    * Avoid any inline "return" statements - all paths must proceed to
    * the end of this function where the cleanup will be done.
    *
    * Record sufficient breadcrumbs along the way, such that proper
    * cleanup can be done in case it is necessary.
    * -------------------
    */

   /*
    * STAGE 2:
    * Do the OS_ModuleLoad() if is called for (i.e. ModuleLoadFile is NOT null)
    */
   if (Status == CFE_SUCCESS && FileName != NULL)
   {
       Status = OS_ModuleLoad( &ModuleId, LibName, FileName );
       if (Status == OS_SUCCESS)
       {
           Status = CFE_SUCCESS; /* just in case CFE_SUCCESS is different than OS_SUCCESS */
           IsModuleLoaded = true;
       }
       else
       {
           /* load not successful.  Note OS errors are better displayed as decimal integers. */
           CFE_ES_WriteToSysLog("ES Startup: Could not load cFE Shared Library: %d\n", (int)Status);
           Status = CFE_ES_ERR_LOAD_LIB;    /* convert OS error to CFE error code */
       }
   }

   /*
    * STAGE 3:
    * Figure out the Entry point / Initialization function.
    *
    * This depends on whether it is a dynamically loaded or a statically linked library,
    * or it could be omitted altogether for libraries which do not require an init function.
    *
    * For dynamically loaded objects where FileName is non-NULL, the
    * "EntryPointData" is a normal C string (const char *) with the name of the function.
    *
    * If the name of the function is the string "NULL" -- then treat this as no function
    * needed and skip the lookup entirely (this is to support startup scripts where some
    * string must be in the entry point field).
    */
   if (Status == CFE_SUCCESS && EntryPointData != NULL)
   {
       if (strcmp(EntryPointData, "NULL") != 0)
       {
           /*
            * If the entry point is explicitly set as NULL,
            * this means the library has no init function - skip the lookup.
            * Otherwise lookup the address of the entry point
            */
           cpuaddr StartAddr;

           Status = OS_SymbolLookup( &StartAddr, EntryPointData );
           if (Status == OS_SUCCESS)
           {
               Status = CFE_SUCCESS; /* just in case CFE_SUCCESS is different than OS_SUCCESS */
               FunctionPointer = (CFE_ES_LibraryEntryFuncPtr_t)StartAddr;
           }
           else
           {
               /* could not find symbol.  Note OS errors are better displayed as decimal integers */
               CFE_ES_WriteToSysLog("ES Startup: Could not find Library Init symbol:%s. EC = %d\n",
                                      (const char *)EntryPointData, (int)Status);
               Status = CFE_ES_ERR_LOAD_LIB;    /* convert OS error to CFE error code */
           }
       }
   }

   /*
    * STAGE 4:
    * Call the Initialization function, if one was identified during the previous stage
    */
   if (Status == CFE_SUCCESS && FunctionPointer != NULL)
   {
       /*
       ** Call the library initialization routine
       */
       Status = (*FunctionPointer)(*LibraryIdPtr);
       if (Status != CFE_SUCCESS)
       {
           CFE_ES_WriteToSysLog("ES Startup: Load Shared Library Init Error = 0x%08x\n", (unsigned int)Status);
       }
   }

   /*
    * LAST STAGE:
    * Do final clean-up
    *
    * If fully successful, then increment the "RegisteredLibs" counter.
    * Otherwise in case of an error, do clean up based on the breadcrumbs
    */
   if(Status == CFE_SUCCESS)
   {
       /* Increment the counter, which needs to be done under lock */
       CFE_ES_LockSharedData(__func__,__LINE__);
       CFE_ES_Global.RegisteredLibs++;
       CFE_ES_UnlockSharedData(__func__,__LINE__);
   }
   else
   {
       /*
        * If the above code had loaded a module, then unload it
        */
       if (IsModuleLoaded)
       {
           OS_ModuleUnload( ModuleId );
       }

       /* Release Slot - No need to lock as it is resetting just a single bool value */
       LibSlotPtr->RecordUsed = false;
   }

   return(Status);

} /* End Function */

/*
**---------------------------------------------------------------------------------------
** Name: CFE_ES_RunAppTableScan
**
**   Purpose: This function scans the ES Application table and acts on the changes
**             in application states. This is where the external cFE Applications are
**             restarted, reloaded, or deleted.
**---------------------------------------------------------------------------------------
*/
bool CFE_ES_RunAppTableScan(uint32 ElapsedTime, void *Arg)
{
   uint32 i;
   CFE_ES_AppRecord_t *AppPtr;
   CFE_ES_AppTableScanState_t *State = (CFE_ES_AppTableScanState_t *)Arg;

   if (State->PendingAppStateChanges == 0)
   {
       /*
        * If the command count changes, then a scan becomes due immediately.
        */
       if (State->LastScanCommandCount == CFE_ES_TaskData.CommandCounter &&
               State->BackgroundScanTimer > ElapsedTime)
       {
           /* no action at this time, background scan is not due yet */
           State->BackgroundScanTimer -= ElapsedTime;
           return false;
       }
   }

   /*
    * Every time a scan is initiated (for any reason)
    * reset the background scan timer to the full value,
    * and take a snapshot of the the command counter.
    */
   State->BackgroundScanTimer = CFE_PLATFORM_ES_APP_SCAN_RATE;
   State->LastScanCommandCount = CFE_ES_TaskData.CommandCounter;
   State->PendingAppStateChanges = 0;

   /*
    * Scan needs to be done with the table locked,
    * as these state changes need to be done atomically
    * with respect to other tasks that also access/update
    * the state.
    */
   CFE_ES_LockSharedData(__func__,__LINE__);

   /*
   ** Scan the ES Application table. Skip entries that are:
   **  - Not in use, or
   **  - cFE Core apps, or
   **  - Currently running
   */
   for ( i = 0; i < CFE_PLATFORM_ES_MAX_APPLICATIONS; i++ )
   {
       AppPtr = &CFE_ES_Global.AppTable[i];

       if (AppPtr->Type == CFE_ES_AppType_EXTERNAL)
       {
           if (AppPtr->AppState > CFE_ES_AppState_RUNNING)
           {
               /*
                * Increment the "pending" counter which reflects
                * the number of apps that are in some phase of clean up.
                */
               ++State->PendingAppStateChanges;

               /*
                * Decrement the wait timer, if active.
                * When the timeout value becomes zero, take the action to delete/restart/reload the app
                */
               if ( AppPtr->ControlReq.AppTimerMsec > ElapsedTime )
               {
                   AppPtr->ControlReq.AppTimerMsec -= ElapsedTime;
               }
               else
               {
                   AppPtr->ControlReq.AppTimerMsec = 0;

                   /*
                    * Temporarily unlock the table, and invoke the
                    * control request function for this app.
                    */
                   CFE_ES_UnlockSharedData(__func__,__LINE__);
                   CFE_ES_ProcessControlRequest(i);
                   CFE_ES_LockSharedData(__func__,__LINE__);
               } /* end if */
           }
           else if (AppPtr->AppState == CFE_ES_AppState_RUNNING &&
                       AppPtr->ControlReq.AppControlRequest > CFE_ES_RunStatus_APP_RUN)
           {
               /* this happens after a command arrives to restart/reload/delete an app */
               /* switch to WAITING state, and set the timer for transition */
               AppPtr->AppState = CFE_ES_AppState_WAITING;
               AppPtr->ControlReq.AppTimerMsec = CFE_PLATFORM_ES_APP_KILL_TIMEOUT * CFE_PLATFORM_ES_APP_SCAN_RATE;
           }


       } /* end if */

   } /* end for loop */

   CFE_ES_UnlockSharedData(__func__,__LINE__);

   /*
    * This state machine is considered active if there are any
    * pending app state changes.  Returning "true" will cause this job
    * to be called from the background task at a faster interval.
    */
   return (State->PendingAppStateChanges != 0);

} /* End Function */


/*
**---------------------------------------------------------------------------------------
**   Name: CFE_ES_ProcessControlRequest
**
**   Purpose: This function will perform the requested control action for an application.
**---------------------------------------------------------------------------------------
*/
void CFE_ES_ProcessControlRequest(uint32 AppID)
{

   int32                   Status;
   CFE_ES_AppStartParams_t AppStartParams;
   uint32                  NewAppId;

   /*
   ** First get a copy of the Apps Start Parameters
   */
   memcpy(&AppStartParams, &(CFE_ES_Global.AppTable[AppID].StartParams), sizeof(CFE_ES_AppStartParams_t));

   /*
   ** Now, find out what kind of Application control is being requested
   */
   switch ( CFE_ES_Global.AppTable[AppID].ControlReq.AppControlRequest )
   {

      case CFE_ES_RunStatus_APP_EXIT:
         /*
         ** Kill the app, and dont restart it
         */
         Status = CFE_ES_CleanUpApp(AppID);

         if ( Status == CFE_SUCCESS )
         {
            CFE_EVS_SendEvent(CFE_ES_EXIT_APP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Exit Application %s Completed.",AppStartParams.Name);
         }
         else
         {
            CFE_EVS_SendEvent(CFE_ES_EXIT_APP_ERR_EID, CFE_EVS_EventType_ERROR,
                               "Exit Application %s Failed: CleanUpApp Error 0x%08X.",AppStartParams.Name, (unsigned int)Status);
         }
         break;

      case CFE_ES_RunStatus_APP_ERROR:
         /*
         ** Kill the app, and dont restart it
         */
         Status = CFE_ES_CleanUpApp(AppID);

         if ( Status == CFE_SUCCESS )
         {
            CFE_EVS_SendEvent(CFE_ES_ERREXIT_APP_INF_EID, CFE_EVS_EventType_INFORMATION,
                               "Exit Application %s on Error Completed.",AppStartParams.Name);
         }
         else
         {
            CFE_EVS_SendEvent(CFE_ES_ERREXIT_APP_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Exit Application %s on Error Failed: CleanUpApp Error 0x%08X.",AppStartParams.Name, (unsigned int)Status);
         }
         break;

      case CFE_ES_RunStatus_SYS_DELETE:
         /*
         ** Kill the app, and dont restart it
         */
         Status = CFE_ES_CleanUpApp(AppID);

         if ( Status == CFE_SUCCESS )
         {
            CFE_EVS_SendEvent(CFE_ES_STOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Stop Application %s Completed.",AppStartParams.Name);
         }
         else
         {
            CFE_EVS_SendEvent(CFE_ES_STOP_ERR3_EID, CFE_EVS_EventType_ERROR,
                              "Stop Application %s Failed: CleanUpApp Error 0x%08X.",AppStartParams.Name, (unsigned int)Status);
         }
         break;

      case CFE_ES_RunStatus_SYS_RESTART:
         /*
         ** Kill the app
         */
         Status = CFE_ES_CleanUpApp(AppID);

         if ( Status == CFE_SUCCESS )
         {
            /*
            ** And start it back up again
            */
            Status = CFE_ES_AppCreate(&NewAppId, (char *)AppStartParams.FileName,
                                           (char *)AppStartParams.EntryPoint,
                                           (char *)AppStartParams.Name,
                                           AppStartParams.Priority,
                                           AppStartParams.StackSize,
                                           AppStartParams.ExceptionAction);

            if ( Status == CFE_SUCCESS )
            {
               CFE_EVS_SendEvent(CFE_ES_RESTART_APP_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Restart Application %s Completed.", AppStartParams.Name);
            }
            else
            {
               CFE_EVS_SendEvent(CFE_ES_RESTART_APP_ERR3_EID, CFE_EVS_EventType_ERROR,
                                  "Restart Application %s Failed: AppCreate Error 0x%08X.", AppStartParams.Name, (unsigned int)Status);
            }
         }
         else
         {
               CFE_EVS_SendEvent(CFE_ES_RESTART_APP_ERR4_EID, CFE_EVS_EventType_ERROR,
                                  "Restart Application %s Failed: CleanUpApp Error 0x%08X.", AppStartParams.Name, (unsigned int)Status);
         }
         break;

      case CFE_ES_RunStatus_SYS_RELOAD:
         /*
         ** Kill the app
         */
         Status = CFE_ES_CleanUpApp(AppID);

         if ( Status == CFE_SUCCESS )
         {
            /*
            ** And start it back up again
            */
            Status = CFE_ES_AppCreate(&NewAppId, (char *)AppStartParams.FileName,
                                           (char *)AppStartParams.EntryPoint,
                                           (char *)AppStartParams.Name,
                                           AppStartParams.Priority,
                                           AppStartParams.StackSize,
                                           AppStartParams.ExceptionAction);
            if ( Status == CFE_SUCCESS )
            {
               CFE_EVS_SendEvent(CFE_ES_RELOAD_APP_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Reload Application %s Completed.", AppStartParams.Name);
            }
            else
            {
               CFE_EVS_SendEvent(CFE_ES_RELOAD_APP_ERR3_EID, CFE_EVS_EventType_ERROR,
                                  "Reload Application %s Failed: AppCreate Error 0x%08X.", AppStartParams.Name, (unsigned int)Status);
            }
         }
         else
         {
            CFE_EVS_SendEvent(CFE_ES_RELOAD_APP_ERR4_EID, CFE_EVS_EventType_ERROR,
                              "Reload Application %s Failed: CleanUpApp Error 0x%08X.", AppStartParams.Name, (unsigned int)Status);
         }

         break;

      case CFE_ES_RunStatus_SYS_EXCEPTION:

         CFE_EVS_SendEvent(CFE_ES_PCR_ERR1_EID, CFE_EVS_EventType_ERROR,
                            "ES_ProcControlReq: Invalid State (EXCEPTION) Application %s.",
                             AppStartParams.Name);
         /*
          * Bug #58: This message/event keeps repeating itself indefinitely.
          *
          * Change the request state to DELETE so the next scan will clean
          * up this table entry.
          */
         CFE_ES_Global.AppTable[AppID].ControlReq.AppControlRequest = CFE_ES_RunStatus_SYS_DELETE;
         break;

      default:

         CFE_EVS_SendEvent(CFE_ES_PCR_ERR2_EID, CFE_EVS_EventType_ERROR,
                            "ES_ProcControlReq: Unknown State ( %d ) Application %s.",
                            (int)CFE_ES_Global.AppTable[AppID].ControlReq.AppControlRequest, AppStartParams.Name);

         /*
          * Bug #58: This message/event keeps repeating itself indefinitely.
          *
          * Change the request state to DELETE so the next scan will clean
          * up this table entry.
          */
         CFE_ES_Global.AppTable[AppID].ControlReq.AppControlRequest = CFE_ES_RunStatus_SYS_DELETE;
         break;

   }

} /* End Function */

/*
**---------------------------------------------------------------------------------------
**   Name: CFE_ES_CleanUpApp
**
**   Purpose: Delete an application by cleaning up all of it's resources.
**---------------------------------------------------------------------------------------
*/
int32 CFE_ES_CleanUpApp(uint32 AppId)
{
   uint32    i;
   int32  Status;
   uint32 MainTaskId;
   int32  ReturnCode = CFE_SUCCESS;

   /*
   ** Call the Table Clean up function
   */
#ifndef EXCLUDE_CFE_TBL
   CFE_TBL_CleanUpApp(AppId);
#endif
   /*
   ** Call the Software Bus clean up function
   */
   CFE_SB_CleanUpApp(AppId);

   /*
   ** Call the TIME Clean up function
   */
   CFE_TIME_CleanUpApp(AppId);

   /*
   ** Call the EVS Clean up function
   */
   Status = CFE_EVS_CleanUpApp(AppId);
   if ( Status != CFE_SUCCESS )
   {
      CFE_ES_WriteToSysLog("CFE_ES_CleanUpApp: Call to CFE_EVS_CleanUpApp returned Error: 0x%08X\n",(unsigned int)Status);
      ReturnCode = CFE_ES_APP_CLEANUP_ERR;
   }


   /*
   ** Delete the ES Resources
   */
   CFE_ES_LockSharedData(__func__,__LINE__);

   /*
   ** Get Main Task ID
   */
   MainTaskId = CFE_ES_Global.AppTable[AppId].TaskInfo.MainTaskId;

   /*
   ** Delete any child tasks associated with this app
   */
   for ( i = 0; i < OS_MAX_TASKS; i++ )
   {
      /* delete only CHILD tasks - not the MainTaskId, which will be deleted later (below) */
      if ((CFE_ES_Global.TaskTable[i].RecordUsed == true) &&
          (CFE_ES_Global.TaskTable[i].AppId == AppId) &&
          (CFE_ES_Global.TaskTable[i].TaskId != MainTaskId))
      {
         Status = CFE_ES_CleanupTaskResources(CFE_ES_Global.TaskTable[i].TaskId);
         if ( Status != CFE_SUCCESS )
         {
            CFE_ES_SysLogWrite_Unsync("CFE_ES_CleanUpApp: CleanUpTaskResources for Task ID:%d returned Error: 0x%08X\n",
                                  (int)i, (unsigned int)Status);
            ReturnCode = CFE_ES_APP_CLEANUP_ERR;
         }
      } /* end if */
   } /* end for */

   /*
   ** Delete all of the OS resources, close files, and delete the main task
   */
   Status = CFE_ES_CleanupTaskResources(MainTaskId);
   if ( Status != CFE_SUCCESS )
   {
      CFE_ES_SysLogWrite_Unsync("CFE_ES_CleanUpApp: CleanUpTaskResources for Task ID:%d returned Error: 0x%08X\n",
                               (int)MainTaskId, (unsigned int)Status);
      ReturnCode = CFE_ES_APP_CLEANUP_ERR;

   }

   /*
   ** Remove the app from the AppTable
   */
   if ( CFE_ES_Global.AppTable[AppId].Type == CFE_ES_AppType_EXTERNAL )
   {
      /*
      ** Unload the module only if it is an external app
      */
      Status = OS_ModuleUnload(CFE_ES_Global.AppTable[AppId].StartParams.ModuleId);
      if ( Status == OS_ERROR )
      {
           CFE_ES_SysLogWrite_Unsync("CFE_ES_CleanUpApp: Module (ID:0x%08X) Unload failed. RC=0x%08X\n",
                                 (unsigned int)CFE_ES_Global.AppTable[AppId].StartParams.ModuleId, (unsigned int)Status);
           ReturnCode = CFE_ES_APP_CLEANUP_ERR;
      }
      CFE_ES_Global.RegisteredExternalApps--;
   }

   CFE_ES_Global.AppTable[AppId].AppState = CFE_ES_AppState_UNDEFINED;

   CFE_ES_UnlockSharedData(__func__,__LINE__);

   return(ReturnCode);

} /* end function */


/*
 * Simple state structure used when cleaning up objects associated with tasks
 *
 * This is used locally by CFE_ES_CleanupTaskResources
 */
typedef struct
{
    uint32 ErrorFlag;
    uint32 FoundObjects;
    uint32 PrevFoundObjects;
    uint32 DeletedObjects;
    int32  OverallStatus;
} CFE_ES_CleanupState_t;

/*
**---------------------------------------------------------------------------------------
**   Name: CFE_ES_CleanupObjectCallback
**
**   Purpose: Helper function clean up all objects.
**
**   NOTE: This is called while holding the ES global lock
**---------------------------------------------------------------------------------------
*/
void CFE_ES_CleanupObjectCallback(uint32 ObjectId, void *arg)
{
    CFE_ES_CleanupState_t   *CleanState;
    int32                   Status;
    uint32                  ObjType;
    bool                    ObjIsValid;

    CleanState = (CFE_ES_CleanupState_t *)arg;
    ObjIsValid = true;

    ObjType = OS_IdentifyObject(ObjectId);
    switch(ObjType)
    {
    case OS_OBJECT_TYPE_OS_TASK:
        Status = OS_TaskDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_QUEUE:
        Status = OS_QueueDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_BINSEM:
        Status = OS_BinSemDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_COUNTSEM:
        Status = OS_CountSemDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_MUTEX:
        Status = OS_MutSemDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_TIMECB:
        Status = OS_TimerDelete(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_STREAM:
        Status = OS_close(ObjectId);
        break;
    case OS_OBJECT_TYPE_OS_MODULE:
        Status = OS_ModuleUnload(ObjectId);
        break;
    default:
        ObjIsValid = false;
        Status = OS_ERROR;
        break;
    }

    if (ObjIsValid)
    {
        ++CleanState->FoundObjects;
        if (Status == OS_SUCCESS)
        {
            ++CleanState->DeletedObjects;
        }
        else
        {
            CFE_ES_SysLogWrite_Unsync("Call to OSAL Delete Object (ID:%d) failed. RC=0x%08X\n",
                         (int)ObjectId, (unsigned int)Status);
            if (CleanState->OverallStatus == CFE_SUCCESS)
            {
                /*
                 * Translate any OS failures into the appropriate CFE_ES return codes
                 * (Some object types have special return codes, depending on what type
                 * of object failed to delete)
                 */
                switch(ObjType)
                {
                case OS_OBJECT_TYPE_OS_TASK:
                    CleanState->OverallStatus = CFE_ES_ERR_CHILD_TASK_DELETE;
                    break;
                case OS_OBJECT_TYPE_OS_QUEUE:
                    CleanState->OverallStatus = CFE_ES_QUEUE_DELETE_ERR;
                    break;
                case OS_OBJECT_TYPE_OS_BINSEM:
                    CleanState->OverallStatus = CFE_ES_BIN_SEM_DELETE_ERR;
                    break;
                case OS_OBJECT_TYPE_OS_COUNTSEM:
                    CleanState->OverallStatus = CFE_ES_COUNT_SEM_DELETE_ERR;
                    break;
                case OS_OBJECT_TYPE_OS_MUTEX:
                    CleanState->OverallStatus = CFE_ES_MUT_SEM_DELETE_ERR;
                    break;
                case OS_OBJECT_TYPE_OS_TIMECB:
                    CleanState->OverallStatus = CFE_ES_TIMER_DELETE_ERR;
                    break;
                default:
                    /* generic failure */
                    CleanState->OverallStatus = CFE_ES_APP_CLEANUP_ERR;
                    break;
                }
            }
        }
    }
}

/*
**---------------------------------------------------------------------------------------
**   Name: CFE_ES_CleanupTaskResources
**
**   Purpose: Clean up the OS resources associated with an individual Task
**---------------------------------------------------------------------------------------
*/
int32 CFE_ES_CleanupTaskResources(uint32 TaskId)
{
    CFE_ES_CleanupState_t   CleanState;
    int32                   Result;

    /*
    ** Delete all OSAL resources that belong to this task
    */
    memset(&CleanState, 0, sizeof(CleanState));
    --CleanState.PrevFoundObjects;
    while (1)
    {
        OS_ForEachObject (TaskId, CFE_ES_CleanupObjectCallback, &CleanState);
        if (CleanState.FoundObjects == 0 || CleanState.ErrorFlag != 0)
        {
            break;
        }
        /*
         * The number of found objects should show a downward trend,
         * if not, then stop and do not loop here forever.  (This can
         * happen when using the UT stub functions, or if an object
         * cannot be fully deleted successfully).
         */
        CleanState.ErrorFlag = (CleanState.DeletedObjects == 0 ||
                CleanState.FoundObjects >= CleanState.PrevFoundObjects);
        CleanState.PrevFoundObjects = CleanState.FoundObjects;
        CleanState.FoundObjects = 0;
        CleanState.DeletedObjects = 0;
    }

    /*
    ** Delete the task itself
    */
    Result = OS_TaskDelete(TaskId);
    if (Result == OS_SUCCESS)
    {
        Result = CleanState.OverallStatus;
        if (Result == CFE_SUCCESS && CleanState.FoundObjects > 0)
        {
            /* Objects leftover after cleanup -- resource leak */
            Result = CFE_ES_APP_CLEANUP_ERR;
        }
    }
    else
    {
        Result = CFE_ES_TASK_DELETE_ERR;
    }

    /*
    ** Invalidate ES Task Table entry
    */
    if (OS_ConvertToArrayIndex(TaskId, &TaskId) == OS_SUCCESS)
    {
       CFE_ES_Global.TaskTable[TaskId].RecordUsed = false;
    }

    CFE_ES_Global.RegisteredTasks--;
    return(Result);

}

/*
**---------------------------------------------------------------------------------------
**   Name: CFE_ES_GetAppInfoInternal
**
**   Purpose: Populate the cFE_ES_AppInfo structure with the data for an app.
**---------------------------------------------------------------------------------------
*/
void CFE_ES_GetAppInfoInternal(uint32 AppId, CFE_ES_AppInfo_t *AppInfoPtr )
{

   int32              ReturnCode;
   OS_module_prop_t   ModuleInfo;
   uint32             TaskIndex;
   uint32             i;


   CFE_ES_LockSharedData(__func__,__LINE__);

   AppInfoPtr->AppId = AppId;
   AppInfoPtr->Type = CFE_ES_Global.AppTable[AppId].Type;
   strncpy((char *)AppInfoPtr->Name,
           CFE_ES_Global.AppTable[AppId].StartParams.Name,
           sizeof(AppInfoPtr->Name)-1);
   AppInfoPtr->Name[sizeof(AppInfoPtr->Name)-1] = '\0';

   strncpy((char *)AppInfoPtr->EntryPoint,
           CFE_ES_Global.AppTable[AppId].StartParams.EntryPoint,
           sizeof(AppInfoPtr->EntryPoint) - 1);
   AppInfoPtr->EntryPoint[sizeof(AppInfoPtr->EntryPoint) - 1] = '\0';

   strncpy((char *)AppInfoPtr->FileName, (char *)CFE_ES_Global.AppTable[AppId].StartParams.FileName,
           sizeof(AppInfoPtr->FileName) - 1);
   AppInfoPtr->FileName[sizeof(AppInfoPtr->FileName) - 1] = '\0';

   AppInfoPtr->ModuleId = CFE_ES_Global.AppTable[AppId].StartParams.ModuleId;
   AppInfoPtr->StackSize = CFE_ES_Global.AppTable[AppId].StartParams.StackSize;
   CFE_SB_SET_MEMADDR(AppInfoPtr->StartAddress, CFE_ES_Global.AppTable[AppId].StartParams.StartAddress);
   AppInfoPtr->ExceptionAction = CFE_ES_Global.AppTable[AppId].StartParams.ExceptionAction;
   AppInfoPtr->Priority = CFE_ES_Global.AppTable[AppId].StartParams.Priority;

   AppInfoPtr->MainTaskId = CFE_ES_Global.AppTable[AppId].TaskInfo.MainTaskId;
   strncpy((char *)AppInfoPtr->MainTaskName, (char *)CFE_ES_Global.AppTable[AppId].TaskInfo.MainTaskName,
           sizeof(AppInfoPtr->MainTaskName) - 1);
   AppInfoPtr->MainTaskName[sizeof(AppInfoPtr->MainTaskName) - 1] = '\0';

   /*
   ** Calculate the number of child tasks
   */
   AppInfoPtr->NumOfChildTasks = 0;
   for (i=0; i<OS_MAX_TASKS; i++ )
   {
      if ( CFE_ES_Global.TaskTable[i].AppId == AppId && CFE_ES_Global.TaskTable[i].RecordUsed == true
           && CFE_ES_Global.TaskTable[i].TaskId != AppInfoPtr->MainTaskId )
      {
         AppInfoPtr->NumOfChildTasks++;
      }
   }

   /*
   ** Get the execution counter for the main task
   */
   if (OS_ConvertToArrayIndex(AppInfoPtr->MainTaskId, &TaskIndex) == OS_SUCCESS)
   {
      AppInfoPtr->ExecutionCounter = CFE_ES_Global.TaskTable[TaskIndex].ExecutionCounter;
   }

   /*
   ** Get the address information from the OSAL
   */
   ReturnCode = OS_ModuleInfo ( AppInfoPtr->ModuleId, &ModuleInfo );
   if ( ReturnCode == OS_SUCCESS )
   {
      AppInfoPtr->AddressesAreValid =
              (sizeof(ModuleInfo.addr.code_address) <= sizeof(AppInfoPtr->CodeAddress)) &&
              ModuleInfo.addr.valid;
      CFE_SB_SET_MEMADDR(AppInfoPtr->CodeAddress, ModuleInfo.addr.code_address);
      CFE_SB_SET_MEMADDR(AppInfoPtr->CodeSize, ModuleInfo.addr.code_size);
      CFE_SB_SET_MEMADDR(AppInfoPtr->DataAddress, ModuleInfo.addr.data_address);
      CFE_SB_SET_MEMADDR(AppInfoPtr->DataSize, ModuleInfo.addr.data_size);
      CFE_SB_SET_MEMADDR(AppInfoPtr->BSSAddress, ModuleInfo.addr.bss_address);
      CFE_SB_SET_MEMADDR(AppInfoPtr->BSSSize, ModuleInfo.addr.bss_size);
   }
   else
   {
      AppInfoPtr->AddressesAreValid = false;
      AppInfoPtr->CodeAddress = 0;
      AppInfoPtr->CodeSize = 0;
      AppInfoPtr->DataAddress = 0;
      AppInfoPtr->DataSize = 0;
      AppInfoPtr->BSSAddress = 0;
      AppInfoPtr->BSSSize = 0;
   }



   CFE_ES_UnlockSharedData(__func__,__LINE__);

} /* end function */

