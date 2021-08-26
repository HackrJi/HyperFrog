#include "vt.h"

pFrogVmx		pForgVmxEntrys = NULL;


BOOLEAN		CPUID_VMXIsSupport() {

	int cpuInfo[4];

	//VMX֧��
	__cpuid(cpuInfo, 0x1);
	CpuFeaturesEcx info;
	info.all = cpuInfo[EnumECX];
	if (!info.fields.vmx)
		return	FALSE;
	
	return	TRUE;
}

BOOLEAN		MSR_VMXisSupport() {

	Ia32FeatureControlMsr VmxFeatureControl;
	VmxFeatureControl.all = 	__readmsr(kIa32FeatureControl);

	if (	VmxFeatureControl.fields.enable_vmxon)
		return	TRUE;
	

	return	FALSE;
}

BOOLEAN		CR0_VMXisSuppor() {

	Cr0 VmxCr0;
	VmxCr0.all = __readcr0();

	if (
		VmxCr0.fields.pg &&
		VmxCr0.fields.ne &&
		VmxCr0.fields.pe
		)
		return TRUE;

	return FALSE;
}

PVOID  FrogExAllocatePool(ULONG Size) {
	PVOID  ResultAddr = 	ExAllocatePoolWithTag(NonPagedPool, Size, FrogTag);
	if (ResultAddr != NULL) 
		RtlZeroMemory(ResultAddr, Size);
	

	return	ResultAddr;
}	

#define FrogExFreePool(Addr) 	ExFreePoolWithTag(Addr, FrogTag);


//����VMX����ṹ
BOOLEAN Forg_AllocateForgVmxRegion() {
	ULONG		CountOfProcessor = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	ULONG		FrogsVmxSize = sizeof(FrogVmx) * CountOfProcessor;

	pForgVmxEntrys = FrogExAllocatePool(FrogsVmxSize);

	if (pForgVmxEntrys == NULL)
		return FALSE;

	return TRUE;
}


void	Frog_FreeHyperRegion(pFrogVmx		pForgVmxEntry) {
	
	if (pForgVmxEntry->VmxOnArea != NULL		&& MmIsAddressValid(pForgVmxEntry->VmxOnArea)		)	FrogExFreePool(pForgVmxEntry->VmxOnArea);
	if (pForgVmxEntry->VmxVmcsArea != NULL		&& MmIsAddressValid(pForgVmxEntry->VmxVmcsArea)		)	FrogExFreePool(pForgVmxEntry->VmxVmcsArea);
	if (pForgVmxEntry->VmxBitMapArea != NULL	&& MmIsAddressValid(pForgVmxEntry->VmxBitMapArea)	)	FrogExFreePool(pForgVmxEntry->VmxBitMapArea);
	if (pForgVmxEntry->VmxHostStackArea != NULL	&& MmIsAddressValid(pForgVmxEntry->VmxHostStackArea))	FrogExFreePool(pForgVmxEntry->VmxHostStackArea);

	return;
}

//����VMCS��VMXON��BITMAP����
FrogRetCode Frog_AllocateHyperRegion(pFrogVmx		pForgVmxEntry, ULONG		CpuNumber) {




	pForgVmxEntry->ProcessorNumber = CpuNumber;

	pForgVmxEntry->VmxOnArea = FrogExAllocatePool(PAGE_SIZE);

	pForgVmxEntry->VmxVmcsArea = FrogExAllocatePool(PAGE_SIZE);

	pForgVmxEntry->VmxBitMapArea = FrogExAllocatePool(PAGE_SIZE);

	pForgVmxEntry->VmxHostStackArea = FrogExAllocatePool(HostStackSize);


	if (
		pForgVmxEntry->VmxOnArea == NULL ||
		pForgVmxEntry->VmxVmcsArea == NULL ||
		pForgVmxEntry->VmxBitMapArea == NULL ||
		pForgVmxEntry->VmxHostStackArea == NULL
		)	goto __AllocateHyperFreePoolExit;


	pForgVmxEntry->VmxOnAreaPhysicalAddr = MmGetPhysicalAddress(pForgVmxEntry->VmxOnArea);
	pForgVmxEntry->VmxVmcsAreaPhysicalAddr = MmGetPhysicalAddress(pForgVmxEntry->VmxVmcsArea);
	pForgVmxEntry->VmxBitMapAreaPhysicalAddr = MmGetPhysicalAddress(pForgVmxEntry->VmxBitMapArea);


	return	FrogSuccess;

__AllocateHyperFreePoolExit:

	Frog_FreeHyperRegion(pForgVmxEntry);

	return	ForgAllocatePoolError;
}

//���� VMXON��VMCS�汾��
void	Frog_SetHyperRegionVersion(pFrogVmx		pForgVmxEntry,ULONG		CpuNumber) {

	Ia32VmxBasicMsr	VmxBasicMsr;
	

	VmxBasicMsr.all = __readmsr(kIa32VmxBasic);
	CpuNumber = KeGetCurrentProcessorNumber();

	

	pForgVmxEntry->VmxVmcsArea->revision_identifier = VmxBasicMsr.fields.revision_identifier;
	pForgVmxEntry->VmxOnArea->revision_identifier = VmxBasicMsr.fields.revision_identifier;

}


// �� ToolsFunction--------------------------------------------------------
//--------------------------------------------------------------------------



//����һЩλ��֧�����⻯
void		Frog_SetBitToEnableHyper() {

	//��λҪ��1������ִ��VMXON
	Ia32FeatureControlMsr VmxFeatureControl;
	VmxFeatureControl.all = __readmsr(kIa32FeatureControl);
	VmxFeatureControl.fields.lock = TRUE;
	__writemsr(kIa32FeatureControl, VmxFeatureControl.all);

	//����������ʹ��VMXON
	Cr4	VmxCr4;
	VmxCr4.all = __readcr4();
	VmxCr4.fields.vmxe = TRUE;
	__writecr4(VmxCr4.all);

}

//����Ƿ�֧�����⻯
BOOLEAN		Frog_IsSupportHyper() {

	if (CPUID_VMXIsSupport() &&
		MSR_VMXisSupport() &&
		CR0_VMXisSuppor()
		)
		return	TRUE;


	return FALSE;

}

FrogRetCode	Frog_SetupVmxOn(pFrogVmx		pForgVmxEntry) {
	return  (FrogRetCode)__asm_vmxon(pForgVmxEntry->VmxOnAreaPhysicalAddr);
}

FrogRetCode	Frog_SetupVmcs(pFrogVmx		pForgVmxEntry) {
	FrogRetCode		Status = FrogSuccess;
	Status = __asm_vmclear(pForgVmxEntry->VmxVmcsAreaPhysicalAddr.QuadPart);
	if (!Frog_SUCCESS(Status))
	{
		return ForgVmClearError;
	}

	Status = __asm_vmptrld(pForgVmxEntry->VmxOnAreaPhysicalAddr.QuadPart);
	if (!Frog_SUCCESS(Status))
	{
		return ForgVmClearError;
	}
	DbgBreakPoint();
	//__asm_vmwrite();

	return Status;
}



VOID	Frog_HyperInit(
	_In_ struct _KDPC *Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
) {

	//��ʼ��VMX����
	FrogRetCode	Status;
	ULONG		CpuNumber = KeGetCurrentProcessorNumber();
	pFrogVmx		pForgVmxEntry = &pForgVmxEntrys[CpuNumber];

	//����VMCS��VMXON���ȵ�����
	Status = Frog_AllocateHyperRegion(pForgVmxEntry, CpuNumber);

	if (!Frog_SUCCESS(Status))
	{
		DbgBreakPoint();
		goto	_HyperInitExit;
	}


	//����VMCS��VMXON�汾��
	Frog_SetHyperRegionVersion(pForgVmxEntry, CpuNumber);

	//VMXON
	Status = Frog_SetupVmxOn(pForgVmxEntry);
	if (!Frog_SUCCESS(Status))
	{
		DbgBreakPoint();
		goto	_HyperInitExit;
	}

	//VMCS
	Frog_SetupVmcs(pForgVmxEntry);




_HyperInitExit:

	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);

}



FrogRetCode 	Frog_EnableHyper() {

	//��ѯ�Ƿ�֧�����⻯
	if (!Frog_IsSupportHyper())	return NoSupportHyper;

	//���� ForgVmxRegion
	if (!Forg_AllocateForgVmxRegion()) return ForgAllocatePoolError;

	Frog_SetBitToEnableHyper();

	KeGenericCallDpc(Frog_HyperInit,NULL);
	return	FrogSuccess;

}


VOID	Frog_HyperUnLoad(
	_In_ struct _KDPC *Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
) {

	ULONG		CpuNumber = KeGetCurrentProcessorNumber();
	pFrogVmx		pForgVmxEntry = &pForgVmxEntrys[CpuNumber];
	Frog_FreeHyperRegion(pForgVmxEntry);


	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}

void Frog_DisableHyper() {

	KeGenericCallDpc(Frog_HyperUnLoad, NULL);

	FrogExFreePool(pForgVmxEntrys);

	//ExFreePoolWithTag(pForgVmxEntrys, FrogTag);

}