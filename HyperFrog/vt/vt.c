#include "vt.h"
#include "vt_help.h"

pFrogCpu		Frog_Cpu = NULL;

FrogRetCode
Frog_SetupVmcs(pFrogVmx		pForgVmxEntry) 
{
	ULONG														UseTrueMsrs = 0;
	FrogRetCode												Status = FrogSuccess;
	Ia32VmxBasicMsr										VmxBasicMsr = { 0 };
	VmxPinBasedControls									VmPinBasedControls = { 0 };
	VmxProcessorBasedControls						VmProcessorBasedControls = { 0 };
	VmxSecondaryProcessorBasedControls		VmSecondaryProcessorBasedControls = { 0 };
	VmxVmentryControls									VmVmentryControls = { 0 };
	VmxmexitControls										VmExitControls = { 0 };
	KPROCESSOR_STATE									HostState = pForgVmxEntry->HostState;

	VmxBasicMsr.all = __readmsr(kIa32VmxBasic);
	UseTrueMsrs = (BOOLEAN)VmxBasicMsr.fields.vmx_capability_hint;

	//Pin-Based
	VmPinBasedControls.all = Frog_VmxAdjustControlValue(UseTrueMsrs ? kIa32VmxTruePinbasedCtls : kIa32VmxPinbasedCtls, VmPinBasedControls.all);

	//������������
	VmProcessorBasedControls.fields.use_msr_bitmaps = TRUE;
	VmProcessorBasedControls.fields.activate_secondary_control = TRUE;
	VmProcessorBasedControls.all = Frog_VmxAdjustControlValue(UseTrueMsrs ? kIa32VmxTrueProcBasedCtls : kIa32VmxProcBasedCtls, VmProcessorBasedControls.all);

	//����������չ������
    VmSecondaryProcessorBasedControls.fields.enable_rdtscp = TRUE;
    VmSecondaryProcessorBasedControls.fields.enable_invpcid = TRUE;
    VmSecondaryProcessorBasedControls.fields.enable_xsaves_xstors = TRUE;
	VmSecondaryProcessorBasedControls.all = Frog_VmxAdjustControlValue(kIa32VmxProcBasedCtls2, VmSecondaryProcessorBasedControls.all);

	//Vm-Entry������
	VmVmentryControls.fields.ia32e_mode_guest = TRUE;
	VmVmentryControls.all = Frog_VmxAdjustControlValue(UseTrueMsrs ? kIa32VmxTrueEntryCtls : kIa32VmxEntryCtls, VmVmentryControls.all);

	//Vm-Exit������
	VmExitControls.fields.acknowledge_interrupt_on_exit = TRUE;
	VmExitControls.fields.host_address_space_size = TRUE;
	VmExitControls.all = Frog_VmxAdjustControlValue(UseTrueMsrs ? kIa32VmxTrueExitCtls : kIa32VmxExitCtls, VmExitControls.all);

	Status |=Frog_Vmx_Write(PIN_BASED_VM_EXEC_CONTROL, VmPinBasedControls.all);
	Status |=Frog_Vmx_Write(CPU_BASED_VM_EXEC_CONTROL, VmProcessorBasedControls.all);
	Status |=Frog_Vmx_Write(SECONDARY_VM_EXEC_CONTROL, VmSecondaryProcessorBasedControls.all);
	Status |= Frog_Vmx_Write(VM_ENTRY_CONTROLS, VmVmentryControls.all);
	Status |= Frog_Vmx_Write(VM_EXIT_CONTROLS, VmExitControls.all);

  //PULONG  bitMapReadLow =  pForgVmxEntry->VmxBitMapArea.BitMapA;
  //PULONG  bitMapReadHight = pForgVmxEntry->VmxBitMapArea.BitMapB;
  //RTL_BITMAP bitMapReadLowHeader = { 0 };
  //RTL_BITMAP bitMapReadHighHeader = { 0 };
  //RtlInitializeBitMap(&bitMapReadLowHeader, (PULONG)bitMapReadLow, 1024 * 8);
  //RtlInitializeBitMap(&bitMapReadHighHeader, (PULONG)bitMapReadHight, 1024 * 8);
  //
  //RtlSetBit(&bitMapReadLowHeader, MSR_IA32_FEATURE_CONTROL);    // MSR_IA32_FEATURE_CONTROL
  //RtlSetBit(&bitMapReadLowHeader, MSR_IA32_DEBUGCTL);          // MSR_DEBUGCTL
  //RtlSetBit(&bitMapReadHighHeader, MSR_LSTAR - 0xC0000000);     // MSR_LSTAR
  //for (ULONG i = MSR_IA32_VMX_BASIC; i <= MSR_IA32_VMX_VMFUNC; i++)
  //    RtlSetBit(&bitMapReadLowHeader, i);

    Status |= Frog_Vmx_Write(MSR_BITMAP, MmGetPhysicalAddress(pForgVmxEntry->VmxBitMapArea.BitMap).QuadPart);

	//Segment
	Status|=Frog_FullVmxSelector(HostState);

	//gdt
	Status|= Frog_Vmx_Write(GUEST_GDTR_BASE, (ULONG64)HostState.SpecialRegisters.Gdtr.Base);
	Status|=Frog_Vmx_Write(GUEST_GDTR_LIMIT, HostState.SpecialRegisters.Gdtr.Limit);
	Status|=Frog_Vmx_Write(HOST_GDTR_BASE, (ULONG64)HostState.SpecialRegisters.Gdtr.Base);

	//idt
	Status|=Frog_Vmx_Write(GUEST_IDTR_BASE, (ULONG64)HostState.SpecialRegisters.Idtr.Base);
	Status|=Frog_Vmx_Write(GUEST_IDTR_LIMIT, HostState.SpecialRegisters.Idtr.Limit);
	Status|=Frog_Vmx_Write(HOST_IDTR_BASE, (ULONG64)HostState.SpecialRegisters.Idtr.Base);

	// ע�⣺	��ЩCRX_GUEST_HOST_MASK ����ĳһ��λ�����ϵ�ʱ��Guest�������λ�᷵��Shadow��ֵ��Guest��д���λ�����VM-EXIT
	//CR0
	Status|=Frog_Vmx_Write(GUEST_CR0, HostState.SpecialRegisters.Cr0);
	Status|=Frog_Vmx_Write(HOST_CR0, HostState.SpecialRegisters.Cr0);
	//Status|=Frog_Vmx_Write(CR0_READ_SHADOW, HostState.SpecialRegisters.Cr0);

	//CR3
	Status|= Frog_Vmx_Write(GUEST_CR3, HostState.SpecialRegisters.Cr3);
	//��Ϊʹ����KeGenericCallDpc�������ж��ͬ���������������������ǵ�����ͨ��DPCͶ�ŵ���Ľ������棬����CR3�ᱻ�ı䣬����CR3��֮ǰ��Ҫ����
	Status|= Frog_Vmx_Write(HOST_CR3, Frog_Cpu->KernelCr3);

	//CR4
	Status|=Frog_Vmx_Write(GUEST_CR4, HostState.SpecialRegisters.Cr4);
	Status|=Frog_Vmx_Write(HOST_CR4, HostState.SpecialRegisters.Cr4);
	//Status|=Frog_Vmx_Write(CR4_READ_SHADOW, HostState.SpecialRegisters.Cr4);

	//DR7
    Status |= Frog_Vmx_Write(GUEST_IA32_DEBUGCTL, HostState.SpecialRegisters.DebugControl);
	Status|=Frog_Vmx_Write(GUEST_DR7, HostState.SpecialRegisters.KernelDr7);

	//GUEST RSP RIP RFLAGS
	Status|=Frog_Vmx_Write(GUEST_RSP, HostState.ContextFrame.Rsp);
	Status|=Frog_Vmx_Write(GUEST_RIP, HostState.ContextFrame.Rip);
	Status|=Frog_Vmx_Write(GUEST_RFLAGS, HostState.ContextFrame.EFlags);

	//HOST RIP RSP
	Status|=Frog_Vmx_Write(HOST_RSP, (ULONG64)pForgVmxEntry->VmxHostStackArea + HostStackSize);
	Status|=Frog_Vmx_Write(HOST_RIP, (ULONG64)VmxEntryPointer);

    Status |= Frog_Vmx_Write(VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFF);

	return Status;
}


VOID	Frog_DpcRunHyper(
	_In_ struct _KDPC *Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
) {
	//��ʼ��VMX����
	FrogRetCode	Status = FrogSuccess;
	ULONG			CpuNumber = KeGetCurrentProcessorNumber();
	pFrogVmx		pForgVmxEntry = &Frog_Cpu->pForgVmxEntrys[CpuNumber];
	size_t				VmxErrorCode = 0;

	//ÿ��CPU�˶��и�CR4��CR0�о�����ȫ�������˺�
	Frog_SetCr0andCr4BitToEnableHyper(pForgVmxEntry);

	//����HOST��������
	KeSaveStateForHibernate(&pForgVmxEntry->HostState);
	RtlCaptureContext(&pForgVmxEntry->HostState.ContextFrame);

    //����ط��洢�˻�����Ҳ��˵��RIPҲ�ᱻ�����ȥ������GUEST_RIP���Ҳ���������������ط��ᱻ�������Σ�������Ҫ�ж�һ��VMX�Ƿ�����
    if (pForgVmxEntry->HyperIsEnable == FALSE)
    {
        //����VMCS��VMXON���ȵ�����
        Status = Frog_AllocateHyperRegion(pForgVmxEntry, CpuNumber);
        if (!Frog_SUCCESS(Status))
        {
            FrogBreak();
            FrogPrint("AllocateHyperRegion	Error");
            goto	_HyperInitExit;
        }

        //����VMCS��VMXON�汾��
        Frog_SetHyperRegionVersion(pForgVmxEntry, CpuNumber);

        //VMXON
        if (__vmx_on(&pForgVmxEntry->VmxOnAreaPhysicalAddr))
        {
            FrogBreak();
            FrogPrint("Vmxon	Error");
            goto	_HyperInitExit;
        }

        //vmclear
        if (__vmx_vmclear(&pForgVmxEntry->VmxVmcsAreaPhysicalAddr))
        {
            FrogBreak();
            FrogPrint("ForgVmClear	Error");
            goto _HyperInitExit;
        }

        //vmptrld
        if (__vmx_vmptrld(&pForgVmxEntry->VmxVmcsAreaPhysicalAddr))
        {
            FrogBreak();
            FrogPrint("ForgVmptrld	Error");
            goto _HyperInitExit;
        }

        //VMCS
        Status = Frog_SetupVmcs(pForgVmxEntry);
        if (!Frog_SUCCESS(Status)) {
            FrogBreak();
            FrogPrint("Frog_SetupVmcs	Error");
            goto	_HyperInitExit;
        }

        pForgVmxEntry->HyperIsEnable = TRUE;
        if (__vmx_vmlaunch())
        {
            VmxErrorCode = Frog_Vmx_Read(VM_INSTRUCTION_ERROR);
            FrogPrint("VmLaunch	Error = %d", VmxErrorCode);
            FrogBreak();
        }
    }
_HyperInitExit:

	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);

}



FrogRetCode 	Frog_EnableHyper() 
{
	NTSTATUS	Status = STATUS_SUCCESS;
	//��ѯ�Ƿ�֧�����⻯
	if (!Frog_IsSupportHyper()) {
		FrogBreak();
		FrogPrint("NoSupportHyper");
		return NoSupportHyper;
	} 

	//���� ForgVmxRegion
	if (!Forg_AllocateForgVmxRegion()) {
		FrogBreak();
		FrogPrint("ForgAllocatePoolError");
		return ForgAllocatePoolError;
	}

	//����MSR��λ��֧�����⻯
	Frog_SetMsrBitToEnableHyper();

    Frog_Cpu->KernelCr3 = __readcr3();//DPC��Ͷ�ķ�ʽ�ᵼ�½��뵽��ͬ�Ľ��̻����У�������Ҫ�����ں˵�CR3
  
	KeGenericCallDpc(Frog_DpcRunHyper, NULL);

	return	FrogSuccess;

}



//--------------------------------------Unload

VOID	Frog_DpcDisableHyper(
    _In_ struct _KDPC *Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    ULONG	        CurrentProcessor = 0;
    pFrogVmx		pForgVmxEntry = NULL;
    CpuId               Data = { 0 };

    CurrentProcessor = KeGetCurrentProcessorNumber();
    pForgVmxEntry = &Frog_Cpu->pForgVmxEntrys[CurrentProcessor];

    FrogBreak();
    if (Frog_VmCall(FrogExitTag, 0, 0, 0))
    {
        //ͦ�ֵ�
       // __writecr4(pForgVmxEntry->OrigCr4);
        //__writecr0(pForgVmxEntry->OrigCr0);
        Frog_FreeHyperRegion(pForgVmxEntry);
    }
    if (pForgVmxEntry)		FrogExFreePool(pForgVmxEntry);

    KeSignalCallDpcSynchronize(SystemArgument2);
    KeSignalCallDpcDone(SystemArgument1);
    
}


FrogRetCode	Frog_DisableHyper() 
{
    ULONG       NumberOfProcessors = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    for (ULONG  ProcessIndex = 0 ; ProcessIndex < NumberOfProcessors ; ProcessIndex++)
    {
        PROCESSOR_NUMBER          ProcessNumber = {0};
        GROUP_AFFINITY                  affinity;
        GROUP_AFFINITY                  Origaffinity;
    
        KeGetProcessorNumberFromIndex(ProcessIndex, &ProcessNumber);
        RtlSecureZeroMemory(&affinity, sizeof(GROUP_AFFINITY));
        affinity.Group = ProcessNumber.Group;
        affinity.Mask = (KAFFINITY)1ull << ProcessNumber.Number;
        KeSetSystemGroupAffinityThread(&affinity, &Origaffinity);
    
        pFrogVmx		pForgVmxEntry = NULL;
        pForgVmxEntry = &Frog_Cpu->pForgVmxEntrys[ProcessIndex];
    
        if (Frog_VmCall(FrogExitTag, 0, 0, 0))
        {
            //ͦ�ֵ�
            //__writecr4(pForgVmxEntry->OrigCr4);
            //__writecr0(pForgVmxEntry->OrigCr0);
            Frog_FreeHyperRegion(pForgVmxEntry);
        }
        if (pForgVmxEntry)		FrogExFreePool(pForgVmxEntry);
    
        KeRevertToUserGroupAffinityThread(&Origaffinity);
    }

	//__writemsr(kIa32FeatureControl, Frog_Cpu->OrigFeatureControlMsr.all); //ֱ��дҲ������
	if (Frog_Cpu)		FrogExFreePool(Frog_Cpu);

	return	FrogSuccess;
}