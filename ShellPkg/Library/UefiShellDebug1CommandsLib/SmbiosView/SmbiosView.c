/** @file
  Tools of clarify the content of the smbios table.

  Copyright (c) 2005 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "../UefiShellDebug1CommandsLib.h"
#include "LibSmbiosView.h"
#include "smbiosview.h"
#include "PrintInfo.h"
#include "QueryTable.h"

UINT8                       gShowType         = SHOW_DETAIL;
STATIC STRUCTURE_STATISTICS *mStatisticsTable = NULL;

UINT8  SmbiosMajorVersion;
UINT8  SmbiosMinorVersion;

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"-t", TypeValue},
  {L"-h", TypeValue},
  {L"-s", TypeFlag},
  {L"-a", TypeFlag},
  {NULL, TypeMax}
  };

SHELL_STATUS
EFIAPI
ShellCommandRunSmbiosView (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT8               StructType;
  UINT16              StructHandle;
  EFI_STATUS          Status;
  BOOLEAN             RandomView;
  LIST_ENTRY          *Package;
  CHAR16              *ProblemParam;
  SHELL_STATUS        ShellStatus;
  CONST CHAR16        *Temp;

  mStatisticsTable    = NULL;
  Package             = NULL;
  ShellStatus         = SHELL_SUCCESS;

  Status = ShellCommandLineParse (EmptyParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR(Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      ShellPrintHiiEx(-1, -1, NULL, STRING_TOKEN (STR_GEN_PROBLEM), gShellDebug1HiiHandle, ProblemParam);
      FreePool(ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      ASSERT(FALSE);
    }
  } else {
    if (ShellCommandLineGetCount(Package) > 1) {
      ShellPrintHiiEx(-1, -1, NULL, STRING_TOKEN (STR_GEN_TOO_MANY), gShellDebug1HiiHandle);
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {

      //
      // Init Lib
      //
      Status = LibSmbiosInit ();
      if (EFI_ERROR (Status)) {
        ShellStatus = SHELL_NOT_FOUND;
        goto Done;
      }
      //
      // build statistics table
      //
      Status = InitSmbiosTableStatistics ();
      if (EFI_ERROR (Status)) {
        ShellStatus = SHELL_NOT_FOUND;
        goto Done;
      }

      StructType    = STRUCTURE_TYPE_RANDOM;
      RandomView    = TRUE;
      //
      // Initialize the StructHandle to be the first handle
      //
      StructHandle  = STRUCTURE_HANDLE_INVALID;
      LibGetSmbiosStructure (&StructHandle, NULL, NULL);

      Temp          = ShellCommandLineGetValue(Package, L"-t");
      if (Temp != NULL) {
        StructType = (UINT8) ShellStrToUintn (Temp);
      }

      Temp = ShellCommandLineGetValue(Package, L"-h");
      if (Temp != NULL) {
        RandomView   = FALSE;
        StructHandle = (UINT16) ShellStrToUintn(Temp);
      }

      if (ShellCommandLineGetFlag(Package, L"-s")) {
        Status = DisplayStatisticsTable (SHOW_DETAIL);
        if (EFI_ERROR(Status)) {
          ShellStatus = SHELL_NOT_FOUND;
        }
        goto Done;
      }

      if (ShellCommandLineGetFlag(Package, L"-a")) {
        gShowType = SHOW_ALL;
      }
      //
      // Show SMBIOS structure information
      //
      Status = SMBiosView (StructType, StructHandle, gShowType, RandomView);
      if (EFI_ERROR(Status)) {
        ShellStatus = SHELL_NOT_FOUND;
      }
    }
  }
Done:
  //
  // Release resources
  //
  if (mStatisticsTable != NULL) {
    //
    // Release statistics table
    //
    FreePool (mStatisticsTable);
    mStatisticsTable = NULL;
  }

  if (Package != NULL) {
    ShellCommandLineFreeVarList (Package);
  }

  LibSmbiosCleanup ();

  return ShellStatus;
}

/**
  Query all structures Data from SMBIOS table and Display
  the information to users as required display option.

  @param[in] QueryType      Structure type to view.
  @param[in] QueryHandle    Structure handle to view.
  @param[in] Option         Display option: none,outline,normal,detail.
  @param[in] RandomView     Support for -h parameter.

  @retval EFI_SUCCESS           print is successful.
  @retval EFI_BAD_BUFFER_SIZE   structure is out of the range of SMBIOS table.
**/
EFI_STATUS
EFIAPI
SMBiosView (
  IN  UINT8   QueryType,
  IN  UINT16  QueryHandle,
  IN  UINT8   Option,
  IN  BOOLEAN RandomView
  )
{
  UINT16                    Handle;
  UINT8                     Buffer[1024];
  //
  // bigger than SMBIOS_STRUCTURE_TABLE.MaxStructureSize
  //
  UINT16                    Length;
  UINTN                     Index;
  UINT16                    Offset;
  //
  // address offset from structure table head.
  //
  UINT32                    TableHead;
  //
  // structure table head.
  //

  SMBIOS_STRUCTURE_POINTER  pStruct;
  SMBIOS_STRUCTURE_TABLE    *SMBiosTable;

  SMBiosTable = NULL;
  LibSmbiosGetEPS (&SMBiosTable);
  if (SMBiosTable == NULL) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_CANNOT_ACCESS_TABLE), gShellDebug1HiiHandle);
    return EFI_BAD_BUFFER_SIZE;
  }

  if (CompareMem (SMBiosTable->AnchorString, "_SM_", 4) == 0) {
    //
    // Have get SMBIOS table
    //
    SmbiosPrintEPSInfo (SMBiosTable, Option);

    SmbiosMajorVersion = SMBiosTable->MajorVersion;
    SmbiosMinorVersion = SMBiosTable->MinorVersion;

    ShellPrintEx(-1,-1,L"=========================================================\n");
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_QUERY_STRUCT_COND), gShellDebug1HiiHandle);

    if (QueryType == STRUCTURE_TYPE_RANDOM) {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_QUERYTYPE_RANDOM), gShellDebug1HiiHandle);
    } else {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_QUERYTYPE), gShellDebug1HiiHandle, QueryType);
    }

    if (RandomView) {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_QUERYHANDLE_RANDOM), gShellDebug1HiiHandle);
    } else {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_QUERYHANDLE), gShellDebug1HiiHandle, QueryHandle);
    }

    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_SHOWTYPE), gShellDebug1HiiHandle);
    ShellPrintEx(-1,-1,GetShowTypeString (gShowType));
    ShellPrintEx(-1,-1,L"\n\n");

/*
    //
    // Get internal commands, such as change options.
    //
    Status = WaitEnter ();
    if (EFI_ERROR (Status)) {
      if (Status == EFI_ABORTED) {
        return EFI_SUCCESS;
      }

      return Status;
    }
*/

    //
    // Searching and display structure info
    //
    Handle    = QueryHandle;
    TableHead = SMBiosTable->TableAddress;
    Offset    = 0;
    for (Index = 0; Index < SMBiosTable->NumberOfSmbiosStructures; Index++) {
      //
      // if reach the end of table, break..
      //
      if (Handle == STRUCTURE_HANDLE_INVALID) {
        break;
      }
      //
      // handle then point to the next!
      //
      if (LibGetSmbiosStructure (&Handle, Buffer, &Length) != DMI_SUCCESS) {
        break;
      }
      Offset      = (UINT16) (Offset + Length);
      pStruct.Raw = Buffer;

      //
      // if QueryType==Random, print this structure.
      // if QueryType!=Random, but Hdr->Type==QueryType, also print it.
      // only if QueryType != Random and Hdr->Type != QueryType, skiped it.
      //
      if (QueryType != STRUCTURE_TYPE_RANDOM && pStruct.Hdr->Type != QueryType) {
        continue;
      }

      ShellPrintEx(-1,-1,L"\n=========================================================\n");
      ShellPrintHiiEx(-1,-1,NULL,
        STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_TYPE_HANDLE_DUMP_STRUCT),
        gShellDebug1HiiHandle,
        pStruct.Hdr->Type,
        pStruct.Hdr->Handle
       );
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_INDEX_LENGTH), gShellDebug1HiiHandle, Index, Length);
      //
      // Addr of structure in structure in table
      //
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_ADDR), gShellDebug1HiiHandle, TableHead + Offset);
      DumpHex (0, 0, Length, Buffer);

/*
      //
      // Get internal commands, such as change options.
      //
      Status = WaitEnter ();
      if (EFI_ERROR (Status)) {
        if (Status == EFI_ABORTED) {
          return EFI_SUCCESS;
        }

        return Status;
      }
*/

      if (gShowType != SHOW_NONE) {
        //
        // check structure legality
        //
        SmbiosCheckStructure (&pStruct);

        //
        // Print structure information
        //
        SmbiosPrintStructure (&pStruct, gShowType);
        ShellPrintEx(-1,-1,L"\n");

/*
        //
        // Get internal commands, such as change options.
        //
        Status = WaitEnter ();
        if (EFI_ERROR (Status)) {
          if (Status == EFI_ABORTED) {
            return EFI_SUCCESS;
          }

          return Status;
        }
*/
      }
      if (!RandomView) {
        break;
      }
    }

    ShellPrintEx(-1,-1,L"\n=========================================================\n");
    return EFI_SUCCESS;
  }

  return EFI_BAD_BUFFER_SIZE;
}

/**
  Function to initialize the global mStatisticsTable object.

  @retval EFI_SUCCESS           print is successful.
**/
EFI_STATUS
EFIAPI
InitSmbiosTableStatistics (
  VOID
  )
{
  UINT16                    Handle;
  UINT8                     Buffer[1024];
  UINT16                    Length;
  UINT16                    Offset;
  UINT16                    Index;

  SMBIOS_STRUCTURE_POINTER  pStruct;
  SMBIOS_STRUCTURE_TABLE    *SMBiosTable;
  STRUCTURE_STATISTICS      *pStatistics;

  SMBiosTable = NULL;
  LibSmbiosGetEPS (&SMBiosTable);
  if (SMBiosTable == NULL) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_CANNOT_ACCESS_TABLE), gShellDebug1HiiHandle);
    return EFI_NOT_FOUND;
  }

  if (CompareMem (SMBiosTable->AnchorString, "_SM_", 4) != 0) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_SMBIOS_TABLE), gShellDebug1HiiHandle);
    return EFI_INVALID_PARAMETER;
  }
  //
  // Allocate memory to mStatisticsTable
  //
  if (mStatisticsTable != NULL) {
    FreePool (mStatisticsTable);
    mStatisticsTable = NULL;
  }

  mStatisticsTable = (STRUCTURE_STATISTICS *) AllocatePool (SMBiosTable->NumberOfSmbiosStructures * sizeof (STRUCTURE_STATISTICS));

  if (mStatisticsTable == NULL) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_OUT_OF_MEM), gShellDebug1HiiHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  Offset      = 0;
  pStatistics = mStatisticsTable;

  //
  // search from the first one
  //
  Handle = STRUCTURE_HANDLE_INVALID;
  LibGetSmbiosStructure (&Handle, NULL, NULL);
  for (Index = 1; Index <= SMBiosTable->NumberOfSmbiosStructures; Index++) {
    //
    // If reach the end of table, break..
    //
    if (Handle == STRUCTURE_HANDLE_INVALID) {
      break;
    }
    //
    // After LibGetSmbiosStructure(), handle then point to the next!
    //
    if (LibGetSmbiosStructure (&Handle, Buffer, &Length) != DMI_SUCCESS) {
      break;
    }

    pStruct.Raw = Buffer;
    Offset      = (UINT16) (Offset + Length);

    //
    // general statistics
    //
    pStatistics->Index  = Index;
    pStatistics->Type   = pStruct.Hdr->Type;
    pStatistics->Handle = pStruct.Hdr->Handle;
    pStatistics->Length = Length;
    pStatistics->Addr   = Offset;

    pStatistics         = &mStatisticsTable[Index];
  }

  return EFI_SUCCESS;
}

/**
  Function to display the global mStatisticsTable object.

  @param[in] Option             ECHO, NORMAL, or DETAIL control the amount of detail displayed.

  @retval EFI_SUCCESS           print is successful.
**/
EFI_STATUS
EFIAPI
DisplayStatisticsTable (
  IN   UINT8   Option
  )
{
  UINTN                   Index;
  UINTN                   Num;
  STRUCTURE_STATISTICS    *pStatistics;
  SMBIOS_STRUCTURE_TABLE  *SMBiosTable;

  SMBiosTable = NULL;
  if (Option < SHOW_OUTLINE) {
    return EFI_SUCCESS;
  }
  //
  // display EPS information firstly
  //
  LibSmbiosGetEPS (&SMBiosTable);
  if (SMBiosTable == NULL) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_CANNOT_ACCESS_TABLE), gShellDebug1HiiHandle);
    return EFI_UNSUPPORTED;
  }

  ShellPrintEx(-1,-1,L"\n============================================================\n");
  SmbiosPrintEPSInfo (SMBiosTable, Option);

  if (Option < SHOW_NORMAL) {
    return EFI_SUCCESS;
  }

  if (mStatisticsTable == NULL) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_CANNOT_ACCESS_STATS), gShellDebug1HiiHandle);
    return EFI_NOT_FOUND;
  }

  ShellPrintEx(-1,-1,L"============================================================\n");
  pStatistics = &mStatisticsTable[0];
  Num         = SMBiosTable->NumberOfSmbiosStructures;
  //
  // display statistics table content
  //
  for (Index = 1; Index <= Num; Index++) {
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_INDEX), gShellDebug1HiiHandle, pStatistics->Index);
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_TYPE), gShellDebug1HiiHandle, pStatistics->Type);
    ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_HANDLE), gShellDebug1HiiHandle, pStatistics->Handle);
    if (Option >= SHOW_DETAIL) {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_OFFSET), gShellDebug1HiiHandle, pStatistics->Addr);
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_LENGTH), gShellDebug1HiiHandle, pStatistics->Length);
    }

    ShellPrintEx(-1,-1,L"\n");
    pStatistics = &mStatisticsTable[Index];
/*
    //
    // Display 20 lines and wait for a page break
    //
    if (Index % 20 == 0) {
      ShellPrintHiiEx(-1,-1,NULL,STRING_TOKEN (STR_SMBIOSVIEW_SMBIOSVIEW_ENTER_CONTINUE), gShellDebug1HiiHandle);
      Status = WaitEnter ();
      if (EFI_ERROR (Status)) {
        if (Status == EFI_ABORTED) {
          return EFI_SUCCESS;
        }

        return Status;
      }
    }
*/
  }

  return EFI_SUCCESS;
}

/**
  function to return a string of the detail level.

  @param[in] ShowType         The detail level whose name is desired in clear text.

  @return   A pointer to a string representing the ShowType (or 'undefined type' if not known).
**/
CHAR16 *
EFIAPI
GetShowTypeString (
  UINT8 ShowType
  )
{
  //
  // show type
  //
  switch (ShowType) {

  case SHOW_NONE:
    return L"SHOW_NONE";

  case SHOW_OUTLINE:
    return L"SHOW_OUTLINE";

  case SHOW_NORMAL:
    return L"SHOW_NORMAL";

  case SHOW_DETAIL:
    return L"SHOW_DETAIL";

  case SHOW_ALL:
    return L"SHOW_ALL";

  default:
    return L"Undefined type";
  }
}

/*
EFI_STATUS
InitializeSmbiosViewApplicationGetLineHelp (
  OUT CHAR16              **Str
  )
{
  return LibCmdGetStringByToken (STRING_ARRAY_NAME, &EfiSmbiosViewGuid, STRING_TOKEN (STR_SMBIOSVIEW_LINE_HELP), Str);
}
*/