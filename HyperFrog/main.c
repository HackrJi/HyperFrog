#include "public.h"

void	UnloadDriver(PDRIVER_OBJECT DriverObject) {
	FrogRetCode	Status;
	Status = Frog_DisableHyper();
	CommUnload();
	if (!Frog_SUCCESS(Status))
	{
		FrogBreak();
		FrogPrint("HyperUnload Error");
	}
}

//��VT����Ŷ����سɹ��������������
void Frog_CallRoutine(PDRIVER_OBJECT pObj)
{
	Frog_Hook();
}

NTSTATUS	DriverEntry(PDRIVER_OBJECT	pDriverObj,PUNICODE_STRING	pReg) {
	pDriverObj->DriverUnload = UnloadDriver;
	FrogRetCode	fStatus = FrogSuccess;
	NTSTATUS		nStatus = STATUS_SUCCESS;
	
	nStatus = InitComm(pDriverObj);
	if (!NT_SUCCESS(nStatus))
	{
        FrogBreak();
        FrogPrint("InitComm Error");
        return STATUS_UNSUCCESSFUL;
	}
    //���� ForgVmxRegion
    if (!Forg_AllocateForgVmxRegion()) {
        FrogBreak();
        FrogPrint("ForgAllocatePool Error");
        return STATUS_UNSUCCESSFUL;
    }

    g_FrogCpu->EnableEpt = true;
    g_FrogCpu->EnableHookMsr = false;

	fStatus = Frog_EnableHyper();

	return	STATUS_SUCCESS;
}