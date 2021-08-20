#include "vt.h"

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

void		FrogExFreePool(PULONG_PTR	FreeAddr) {
	ExFreePoolWithTag(FreeAddr, FrogTag);
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

	if (		CPUID_VMXIsSupport()		&&
			MSR_VMXisSupport()		&&
			CR0_VMXisSuppor()
		)
		return	TRUE;
	

	return FALSE;

}


//����VMX����ṹ
BOOLEAN Forg_AllocateForgVmxRegion() {
	ULONG		CountOfProcessor = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	ULONG		FrogsVmxSize = sizeof(FrogVmx) * CountOfProcessor;
	
	pForgVmxEntrys = FrogExAllocatePool(FrogsVmxSize);

	if (pForgVmxEntrys == NULL)
		return FALSE;
	
	return TRUE;
}

//����VMCS��VMXON��BITMAP����
FrogRetCode Frog_AllocateHyperRegion() {

	ULONG		CpuNumber = 	KeGetCurrentProcessorNumber();
	pFrogVmx		pForgVmxEntry = &pForgVmxEntrys[CpuNumber];
	PULONG_PTR	FreeAddrStart=NULL;
	PULONG_PTR	FreeAddrEnd = NULL;

	pForgVmxEntry->ProcessorNumber = CpuNumber;
	
	pForgVmxEntry->VmxOnArea = FrogExAllocatePool(PAGE_SIZE);

	pForgVmxEntry->VmxVmcsArea = FrogExAllocatePool(PAGE_SIZE);

	pForgVmxEntry->VmxBitMapArea = FrogExAllocatePool(PAGE_SIZE);
	
	pForgVmxEntry->VmxHostStackArea = FrogExAllocatePool(HostStackSize, FrogTag);


	if (
		pForgVmxEntry->VmxOnArea == NULL			 || 
		pForgVmxEntry->VmxVmcsArea == NULL		 ||
		pForgVmxEntry->VmxBitMapArea == NULL		 ||
		pForgVmxEntry->VmxHostStackArea == NULL
		)	goto _AllocateHyperFreePool;


	pForgVmxEntry->VmxOnAreaPhysicalAddr = 	MmGetPhysicalAddress(pForgVmxEntry->VmxOnArea);
	pForgVmxEntry->VmxVmcsAreaPhysicalAddr = MmGetPhysicalAddress(pForgVmxEntry->VmxVmcsArea);
	pForgVmxEntry->VmxBitMapAreaPhysicalAddr = MmGetPhysicalAddress(pForgVmxEntry->VmxBitMapArea);


	return	FrogSuccess;

_AllocateHyperFreePool:

	FreeAddrStart = (PULONG_PTR)&pForgVmxEntry->VmxOnArea;
	FreeAddrEnd = (PULONG_PTR)&pForgVmxEntry->VmxHostStackArea;

	
	do
	{
		if (*FreeAddrStart == NULL)
		{
			FrogExFreePool(*FreeAddrStart);
		}

		FreeAddrStart++;
	} while (FreeAddrStart != FreeAddrEnd);

	pForgVmxEntry->HyperIsEnable = FALSE;

	return	ForgAllocateError;
}

//���� VMXON��VMCS�汾��
void	Frog_SetHyperRegionVersion() {

	Ia32VmxBasicMsr	VmxBasicMsr;
	ULONG		CpuNumber;

	VmxBasicMsr.all = __readmsr(kIa32VmxBasic);
	CpuNumber = KeGetCurrentProcessorNumber();

	pFrogVmx		pForgVmxEntry = &pForgVmxEntrys[CpuNumber];

	pForgVmxEntry->VmxVmcsArea->revision_identifier = VmxBasicMsr.fields.revision_identifier;
	pForgVmxEntry->VmxVmcsArea->revision_identifier = VmxBasicMsr.fields.revision_identifier;

}

VOID	Frog_HyperInit(
	_In_ struct _KDPC *Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
) {

	if (!Forg_AllocateForgVmxRegion) {
		DbgBreakPoint();
	}

	//��ʼ��VMX����
	FrogRetCode	Status;
	Status = Frog_AllocateHyperRegion();
	if (Status != ForgAllocateError)
	{
		DbgBreakPoint();
		goto	_HyperInitExit;
	}

	Frog_SetHyperRegionVersion();

	//дASM��


_HyperInitExit:

	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);

}



FrogRetCode 	Frog_EnableHyper() {

	if (!Frog_IsSupportHyper)	return NoSupportHyper;


	KeGenericCallDpc(Frog_HyperInit,NULL);



}

void Frog_DisableHyper() {
	ExFreePoolWithTag(pForgVmxEntrys, FrogTag);

}