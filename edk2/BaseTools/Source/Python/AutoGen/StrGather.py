# Copyright (c) 2007, Intel Corporation
# All rights reserved. This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

#
#This file is used to parse a strings file and create or add to a string database file.
#

##
# Import Modules
#
import re
import Common.EdkLogger as EdkLogger
from Common.BuildToolError import *
from UniClassObject import *

##
# Static definitions
#
EFI_HII_SIBT_END = '0x00'
EFI_HII_SIBT_STRING_SCSU = '0x10'
EFI_HII_SIBT_STRING_SCSU_FONT = '0x11'
EFI_HII_SIBT_STRINGS_SCSU = '0x12'
EFI_HII_SIBT_STRINGS_SCSU_FONT = '0x13'
EFI_HII_SIBT_STRING_UCS2 = '0x14'
EFI_HII_SIBT_STRING_UCS2_FONT = '0x15'
EFI_HII_SIBT_STRINGS_UCS2 = '0x16'
EFI_HII_SIBT_STRINGS_UCS2_FONT = '0x17'
EFI_HII_SIBT_DUPLICATE = '0x20'
EFI_HII_SIBT_SKIP2 = '0x21'
EFI_HII_SIBT_SKIP1 = '0x22'
EFI_HII_SIBT_EXT1 = '0x30'
EFI_HII_SIBT_EXT2 = '0x31'
EFI_HII_SIBT_EXT4 = '0x32'
EFI_HII_SIBT_FONT = '0x40'

EFI_HII_PACKAGE_STRINGS = '0x04'
EFI_HII_PACKAGE_FORM = '0x02'

StringPackageType = EFI_HII_PACKAGE_STRINGS
StringPackageForm = EFI_HII_PACKAGE_FORM
StringBlockType = EFI_HII_SIBT_STRING_UCS2
StringSkipType = EFI_HII_SIBT_SKIP2

HexHeader = '0x'

COMMENT = '// '
DEFINE_STR = '#define'
COMMENT_DEFINE_STR = COMMENT + DEFINE_STR
NOT_REFERENCED = 'not referenced'
COMMENT_NOT_REFERENCED = ' ' + COMMENT + NOT_REFERENCED
CHAR_ARRAY_DEFIN = 'unsigned char'
COMMON_FILE_NAME = 'Strings'
OFFSET = 'offset'
STRING = 'string'
TO = 'to'
STRING_TOKEN = re.compile('STRING_TOKEN *\(([A-Z0-9_]+) *\)', re.MULTILINE | re.UNICODE)

EFI_HII_ARRAY_SIZE_LENGTH = 4
EFI_HII_PACKAGE_HEADER_LENGTH = 4
EFI_HII_HDR_SIZE_LENGTH = 4
EFI_HII_STRING_OFFSET_LENGTH = 4
EFI_STRING_ID = 1
EFI_STRING_ID_LENGTH = 2
EFI_HII_LANGUAGE_WINDOW = 0
EFI_HII_LANGUAGE_WINDOW_LENGTH = 2
EFI_HII_LANGUAGE_WINDOW_NUMBER = 16
EFI_HII_STRING_PACKAGE_HDR_LENGTH = EFI_HII_PACKAGE_HEADER_LENGTH + EFI_HII_HDR_SIZE_LENGTH + EFI_HII_STRING_OFFSET_LENGTH + EFI_HII_LANGUAGE_WINDOW_LENGTH * EFI_HII_LANGUAGE_WINDOW_NUMBER + EFI_STRING_ID_LENGTH

H_C_FILE_HEADER = ['//', \
                   '//  DO NOT EDIT -- auto-generated file', \
                   '//', \
                   '//  This file is generated by the StrGather utility', \
                   '//']
LANGUAGE_NAME_STRING_NAME = '$LANGUAGE_NAME'
PRINTABLE_LANGUAGE_NAME_STRING_NAME = '$PRINTABLE_LANGUAGE_NAME'

## Convert a dec number to a hex string
#
# Convert a dec number to a formatted hex string in length digit
# The digit is set to default 8
# The hex string starts with "0x"
# DecToHexStr(1000) is '0x000003E8'
# DecToHexStr(1000, 6) is '0x0003E8'
#
# @param Dec:    The number in dec format
# @param Digit:  The needed digit of hex string
#
# @retval:       The formatted hex string
#
def DecToHexStr(Dec, Digit = 8):
    return eval("'0x%0" + str(Digit) + "X' % int(Dec)")

## Convert a dec number to a hex list
#
# Convert a dec number to a formatted hex list in size digit
# The digit is set to default 8
# DecToHexList(1000) is ['0xE8', '0x03', '0x00', '0x00']
# DecToHexList(1000, 6) is ['0xE8', '0x03', '0x00']
#
# @param Dec:    The number in dec format
# @param Digit:  The needed digit of hex list
#
# @retval:       A list for formatted hex string
#
def DecToHexList(Dec, Digit = 8):
    Hex = eval("'%0" + str(Digit) + "X' % int(Dec)" )
    List = []
    for Bit in range(Digit - 2, -1, -2):
        List.append(HexHeader + Hex[Bit:Bit + 2])
    return List

## Convert a acsii string to a hex list
#
# Convert a acsii string to a formatted hex list
# AscToHexList('en-US') is ['0x65', '0x6E', '0x2D', '0x55', '0x53']
#
# @param Ascii:  The acsii string
#
# @retval:       A list for formatted hex string
#
def AscToHexList(Ascii):
    List = []
    for Item in Ascii:
        List.append('0x%2X' % ord(Item))

    return List

## Create header of .h file
#
# Create a header of .h file
#
# @param BaseName: The basename of strings
#
# @retval Str:     A string for .h file header
#
def CreateHFileHeader(BaseName):
    Str = ''
    for Item in H_C_FILE_HEADER:
        Str = WriteLine(Str, Item)
    Str = WriteLine(Str, '#ifndef _' + BaseName.upper() + '_STRINGS_DEFINE_H_')
    Str = WriteLine(Str, '#define _' + BaseName.upper() + '_STRINGS_DEFINE_H_')
    return Str

## Create content of .h file
#
# Create content of .h file
#
# @param BaseName:       The basename of strings
# @param UniObjectClass: A UniObjectClass instance
#
# @retval Str:           A string of .h file content
#
def CreateHFileContent(BaseName, UniObjectClass):
    Str = ''
    ValueStartPtr = 60
    Line = COMMENT_DEFINE_STR + ' ' + LANGUAGE_NAME_STRING_NAME + ' ' * (ValueStartPtr - len(DEFINE_STR + LANGUAGE_NAME_STRING_NAME)) + DecToHexStr(0, 4) + COMMENT_NOT_REFERENCED
    Str = WriteLine(Str, Line)
    Line = COMMENT_DEFINE_STR + ' ' + PRINTABLE_LANGUAGE_NAME_STRING_NAME + ' ' * (ValueStartPtr - len(DEFINE_STR + PRINTABLE_LANGUAGE_NAME_STRING_NAME)) + DecToHexStr(1, 4) + COMMENT_NOT_REFERENCED
    Str = WriteLine(Str, Line)
    for Index in range(2, len(UniObjectClass.OrderedStringList[UniObjectClass.LanguageDef[0][0]])):
        StringItem = UniObjectClass.OrderedStringList[UniObjectClass.LanguageDef[0][0]][Index]
        Name = StringItem.StringName
        Token = StringItem.Token
        Referenced = StringItem.Referenced
        if Name != None:
            Line = ''
            if Referenced == True:
                Line = DEFINE_STR + ' ' + Name + ' ' * (ValueStartPtr - len(DEFINE_STR + Name)) + DecToHexStr(Token, 4)
            else:
                Line = COMMENT_DEFINE_STR + ' ' + Name + ' ' * (ValueStartPtr - len(DEFINE_STR + Name)) + DecToHexStr(Token, 4) + COMMENT_NOT_REFERENCED
            Str = WriteLine(Str, Line)

    Str =  WriteLine(Str, '')
    Str = WriteLine(Str, 'extern unsigned char ' + BaseName + 'Strings[];')
    return Str

## Create a complete .h file
#
# Create a complet .h file with file header and file content
#
# @param BaseName:       The basename of strings
# @param UniObjectClass: A UniObjectClass instance
#
# @retval Str:           A string of complete .h file
#
def CreateHFile(BaseName, UniObjectClass):
    HFile = WriteLine('', CreateHFileContent(BaseName, UniObjectClass))

    return HFile

## Create header of .c file
#
# Create a header of .c file
#
# @retval Str:     A string for .c file header
#
def CreateCFileHeader():
    Str = ''
    for Item in H_C_FILE_HEADER:
        Str = WriteLine(Str, Item)

    return Str

## Create a formatted string all items in an array
#
# Use ',' to join each item in an array, and break an new line when reaching the width (default is 16)
#
# @param Array:      The array need to be formatted
# @param Width:      The line length, the default value is set to 16
#
# @retval ArrayItem: A string for all formatted array items
#
def CreateArrayItem(Array, Width = 16):
    MaxLength = Width
    Index = 0
    Line = '  '
    ArrayItem = ''

    for Item in Array:
        if Index < MaxLength:
            Line = Line + Item + ',  '
            Index = Index + 1
        else:
            ArrayItem = WriteLine(ArrayItem, Line)
            Line = '  ' + Item +  ',  '
            Index = 1
    ArrayItem = Write(ArrayItem, Line.rstrip())

    return ArrayItem

## CreateCFileStringValue
#
# Create a line with string value
#
# @param Value:  Value of the string
#
# @retval Str:   A formatted string with string value
#

def CreateCFileStringValue(Value):
    Value = [StringBlockType] + Value
    Str = WriteLine('', CreateArrayItem(Value))

    return Str


## Create content of .c file
#
# Create content of .c file
#
# @param BaseName:       The basename of strings
# @param UniObjectClass: A UniObjectClass instance
#
# @retval Str:           A string of .c file content
#
def CreateCFileContent(BaseName, UniObjectClass, IsCompatibleMode):
    #
    # Init array length
    #
    TotalLength = EFI_HII_ARRAY_SIZE_LENGTH
    Str = ''
    Offset = 0

    #
    # Create lines for each language's strings
    #
    for IndexI in range(len(UniObjectClass.LanguageDef)):
        Language = UniObjectClass.LanguageDef[IndexI][0]
        LangPrintName = UniObjectClass.LanguageDef[IndexI][1]

        StrStringValue = ''
        ArrayLength = 0
        NumberOfUseOhterLangDef = 0
        Index = 0
        for IndexJ in range(1, len(UniObjectClass.OrderedStringList[UniObjectClass.LanguageDef[IndexI][0]])):
            Item = UniObjectClass.FindByToken(IndexJ, Language)
            Name = Item.StringName
            Value = Item.StringValueByteList
            Referenced = Item.Referenced
            Token = Item.Token
            Length = Item.Length
            UseOtherLangDef = Item.UseOtherLangDef

            if UseOtherLangDef != '' and Referenced:
                NumberOfUseOhterLangDef = NumberOfUseOhterLangDef + 1
                Index = Index + 1
            else:
                if NumberOfUseOhterLangDef > 0:
                    StrStringValue = WriteLine(StrStringValue, CreateArrayItem([StringSkipType] + DecToHexList(NumberOfUseOhterLangDef, 4)))
                    NumberOfUseOhterLangDef = 0
                    ArrayLength = ArrayLength + 3
                if Referenced and Item.Token > 0:
                    Index = Index + 1
                    StrStringValue = WriteLine(StrStringValue, "// %s: %s:%s" % (DecToHexStr(Index, 4), Name, DecToHexStr(Token, 4)))
                    StrStringValue = Write(StrStringValue, CreateCFileStringValue(Value))
                    Offset = Offset + Length
                    ArrayLength = ArrayLength + Item.Length + 1 # 1 is for the length of string type

        #
        # EFI_HII_PACKAGE_HEADER
        #
        Offset = EFI_HII_STRING_PACKAGE_HDR_LENGTH + len(Language) + 1
        ArrayLength = Offset + ArrayLength + 1

        #
        # Create PACKAGE HEADER
        #
        Str = WriteLine(Str, '// PACKAGE HEADER\n')
        TotalLength = TotalLength + ArrayLength

        List = DecToHexList(ArrayLength, 6) + \
               [StringPackageType] + \
               DecToHexList(Offset) + \
               DecToHexList(Offset) + \
               DecToHexList(EFI_HII_LANGUAGE_WINDOW, EFI_HII_LANGUAGE_WINDOW_LENGTH * 2) * EFI_HII_LANGUAGE_WINDOW_NUMBER + \
               DecToHexList(EFI_STRING_ID, 4) + \
               AscToHexList(Language) + \
               DecToHexList(0, 2)
        Str = WriteLine(Str, CreateArrayItem(List, 16) + '\n')

        #
        # Create PACKAGE DATA
        #
        Str = WriteLine(Str, '// PACKAGE DATA\n')
        Str = Write(Str, StrStringValue)

        #
        # Add an EFI_HII_SIBT_END at last
        #
        Str = WriteLine(Str, '  ' + EFI_HII_SIBT_END + ",")

    #
    # Create line for string variable name
    # "unsigned char $(BaseName)Strings[] = {"
    #
    AllStr = WriteLine('', CHAR_ARRAY_DEFIN + ' ' + BaseName + COMMON_FILE_NAME + '[] = {\n' )

    #
    # Create FRAMEWORK_EFI_HII_PACK_HEADER in compatible mode
    #
    if IsCompatibleMode:
        AllStr = WriteLine(AllStr, '// FRAMEWORK PACKAGE HEADER Length')
        AllStr = WriteLine(AllStr, CreateArrayItem(DecToHexList(TotalLength + 2)) + '\n')
        AllStr = WriteLine(AllStr, '// FRAMEWORK PACKAGE HEADER Type')
        AllStr = WriteLine(AllStr, CreateArrayItem(DecToHexList(2, 4)) + '\n')

    #
    # Create whole array length in UEFI mode
    #
    if not IsCompatibleMode:
        AllStr = WriteLine(AllStr, '// STRGATHER_OUTPUT_HEADER')
        AllStr = WriteLine(AllStr, CreateArrayItem(DecToHexList(TotalLength)) + '\n')

    #
    # Join package data
    #
    AllStr = Write(AllStr, Str)

    return AllStr

## Create end of .c file
#
# Create end of .c file
#
# @retval Str:           A string of .h file end
#
def CreateCFileEnd():
    Str = Write('', '};')
    return Str

## Create a .c file
#
# Create a complete .c file
#
# @param BaseName:       The basename of strings
# @param UniObjectClass: A UniObjectClass instance
#
# @retval CFile:         A string of complete .c file
#
def CreateCFile(BaseName, UniObjectClass, IsCompatibleMode):
    CFile = ''
    #CFile = WriteLine(CFile, CreateCFileHeader())
    CFile = WriteLine(CFile, CreateCFileContent(BaseName, UniObjectClass, IsCompatibleMode))
    CFile = WriteLine(CFile, CreateCFileEnd())
    return CFile

## GetFileList
#
# Get a list for all files
#
# @param IncludeList:  A list of all path to be searched
# @param SkipList:     A list of all types of file could be skipped
#
# @retval FileList:    A list of all files found
#
def GetFileList(SourceFileList, IncludeList, SkipList):
    if IncludeList == None:
        EdkLogger.error("UnicodeStringGather", AUTOGEN_ERROR, "Include path for unicode file is not defined")

    FileList = []
    if SkipList == None:
        SkipList = []

    for File in SourceFileList:
        for Dir in IncludeList:
            if not os.path.exists(Dir):
                continue
            File = os.path.join(Dir, File.Path)
            #
            # Ignore Dir
            #
            if os.path.isfile(File) != True:
                continue
            #
            # Ignore file listed in skip list
            #
            IsSkip = False
            for Skip in SkipList:
                if os.path.splitext(File)[1].upper() == Skip.upper():
                    EdkLogger.verbose("Skipped %s for string token uses search" % File)
                    IsSkip = True
                    break

            if not IsSkip:
                FileList.append(File)

            break

    return FileList

## SearchString
#
# Search whether all string defined in UniObjectClass are referenced
# All string used should be set to Referenced
#
# @param UniObjectClass:  Input UniObjectClass
# @param FileList:        Search path list
#
# @retval UniObjectClass: UniObjectClass after searched
#
def SearchString(UniObjectClass, FileList):
    if FileList == []:
        return UniObjectClass

    for File in FileList:
        if os.path.isfile(File):
            Lines = open(File, 'r')
            for Line in Lines:
                StringTokenList = STRING_TOKEN.findall(Line)
                for StrName in StringTokenList:
                    EdkLogger.debug(EdkLogger.DEBUG_5, "Found string identifier: " + StrName)
                    UniObjectClass.SetStringReferenced(StrName)

    UniObjectClass.ReToken()

    return UniObjectClass

## GetStringFiles
#
# This function is used for UEFI2.1 spec
#
#
def GetStringFiles(UniFilList, SourceFileList, IncludeList, SkipList, BaseName, IsCompatibleMode = False, ShellMode = False):
    Status = True
    ErrorMessage = ''

    if len(UniFilList) > 0:
        if ShellMode:
            #
            # support ISO 639-2 codes in .UNI files of EDK Shell
            #
            Uni = UniFileClassObject(UniFilList, True)
        else:
            Uni = UniFileClassObject(UniFilList, IsCompatibleMode)
    else:
        EdkLogger.error("UnicodeStringGather", AUTOGEN_ERROR, 'No unicode files given')

    FileList = GetFileList(SourceFileList, IncludeList, SkipList)

    Uni = SearchString(Uni, FileList)

    HFile = CreateHFile(BaseName, Uni)
    CFile = CreateCFile(BaseName, Uni, IsCompatibleMode)

    return HFile, CFile

#
# Write an item
#
def Write(Target, Item):
    return Target + Item

#
# Write an item with a break line
#
def WriteLine(Target, Item):
    return Target + Item + '\n'

# This acts like the main() function for the script, unless it is 'import'ed into another
# script.
if __name__ == '__main__':
    EdkLogger.info('start')

    UniFileList = [
                   r'C:\\Edk\\Strings2.uni',
                   r'C:\\Edk\\Strings.uni'
    ]

    SrcFileList = []
    for Root, Dirs, Files in os.walk('C:\\Edk'):
        for File in Files:
            SrcFileList.append(File)

    IncludeList = [
                   r'C:\\Edk'
    ]

    SkipList = ['.inf', '.uni']
    BaseName = 'DriverSample'
    (h, c) = GetStringFiles(UniFileList, SrcFileList, IncludeList, SkipList, BaseName, True)
    hfile = open('unistring.h', 'w')
    cfile = open('unistring.c', 'w')
    hfile.write(h)
    cfile.write(c)

    EdkLogger.info('end')