#pragma once

typedef struct _MsrHookTable
{
    char functionName[256];
    PVOID hookFunction;
}MsrHookTable, * pMsrHookTable;


static MsrHookTable g_MsrHookTable[] =
{
   {
       "NtOpenProcess",
       (PVOID)HookNtOpenProcess,
   },
   //{
   //    "NtReadFile",
   //    (PVOID)HookNtReadFile,
   //},
   //{
   //    "NtQueryKey",
   //    (PVOID)HookNtQueryKey,
   //},
 
};
