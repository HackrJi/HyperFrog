#include "public.h"

pFrogCpu g_FrogCpu = NULL;

//EnableHyper
FrogRetCode
Frog_SetupVmcs(pFrogVmx pForgVmxEntry) 
{
	ULONG														UsetrueMsrs = 0;
	FrogRetCode												Status = FrogSuccess;
	Ia32VmxBasicMsr										    VmxBasicMsr = { 0 };
	VmxPinBasedControls									VmPinBasedControls = { 0 };
	VmxProcessorBasedControls						VmProcessorBasedControls = { 0 };
	VmxSecondaryProcessorBasedControls		VmSecondaryProcessorBasedControls = { 0 };
	VmxVmentryControls									VmVmentryControls = { 0 };
	VmxmexitControls										VmExitControls = { 0 };
	KPROCESSOR_STATE									HostState = pForgVmxEntry->HostState;
    ULONG64                                                    VirtualProcessorId = pForgVmxEntry->ProcessorNumber;
    VirtualProcessorId++;

	Status |= Frog_Vmx_Write(VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFF);
	VmxBasicMsr.all = __readmsr(kIa32VmxBasic);
	UsetrueMsrs = (bool)VmxBasicMsr.fields.vmx_capability_hint;

	//Pin-Based
	VmPinBasedControls.all = Frog_VmxAdjustControlValue(UsetrueMsrs ? kIa32VmxtruePinbasedCtls : kIa32VmxPinbasedCtls, VmPinBasedControls.all);

	//������������
	VmProcessorBasedControls.fields.use_msr_bitmaps = true;
	VmProcessorBasedControls.fields.activate_secondary_control = true;
	VmProcessorBasedControls.all = Frog_VmxAdjustControlValue(UsetrueMsrs ? kIa32VmxtrueProcBasedCtls : kIa32VmxProcBasedCtls, VmProcessorBasedControls.all);

	//����������չ������
    VmSecondaryProcessorBasedControls.fields.enable_rdtscp = true;
    VmSecondaryProcessorBasedControls.fields.enable_invpcid = true;
    VmSecondaryProcessorBasedControls.fields.enable_xsaves_xstors = true;
    if (g_FrogCpu->EnableEpt)
    {
        VmSecondaryProcessorBasedControls.fields.enable_ept = true;  // ���� EPT
         VmSecondaryProcessorBasedControls.fields.enable_vpid = true; // ���� VPID
    }
	VmSecondaryProcessorBasedControls.all = Frog_VmxAdjustControlValue(kIa32VmxProcBasedCtls2, VmSecondaryProcessorBasedControls.all);

	//Vm-Entry������
	VmVmentryControls.fields.ia32e_mode_guest = true;
	VmVmentryControls.all = Frog_VmxAdjustControlValue(UsetrueMsrs ? kIa32VmxtrueEntryCtls : kIa32VmxEntryCtls, VmVmentryControls.all);

	//Vm-Exit������
	VmExitControls.fields.acknowledge_interrupt_on_exit = true;
	VmExitControls.fields.host_address_space_size = true;
	VmExitControls.all = Frog_VmxAdjustControlValue(UsetrueMsrs ? kIa32VmxtrueExitCtls : kIa32VmxExitCtls, VmExitControls.all);

	Status |=Frog_Vmx_Write(PIN_BASED_VM_EXEC_CONTROL, VmPinBasedControls.all);
	Status |=Frog_Vmx_Write(CPU_BASED_VM_EXEC_CONTROL, VmProcessorBasedControls.all);
	Status |=Frog_Vmx_Write(SECONDARY_VM_EXEC_CONTROL, VmSecondaryProcessorBasedControls.all);
	Status |= Frog_Vmx_Write(VM_ENTRY_CONTROLS, VmVmentryControls.all);
	Status |= Frog_Vmx_Write(VM_EXIT_CONTROLS, VmExitControls.all);

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
	Status|= Frog_Vmx_Write(HOST_CR3, g_FrogCpu->KernelCr3);

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


    if (g_FrogCpu->EnableEpt)
    {
        Status |= Frog_Vmx_Write(EPT_POINTER, pForgVmxEntry->VmxEptInfo.VmxEptp.Flags);
        Status |= Frog_Vmx_Write(VIRTUAL_PROCESSOR_ID, VirtualProcessorId);
    }


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
	pFrogVmx		pForgVmxEntry = &g_FrogCpu->pForgVmxEntrys[CpuNumber];
	size_t				VmxErrorCode = 0;

	//ÿ��CPU�˶��и�CR4��CR0�о�����ȫ�������˺�
	Frog_SetCrxToEnableHyper();

	//����HOST��������
	KeSaveStateForHibernate(&pForgVmxEntry->HostState);
	RtlCaptureContext(&pForgVmxEntry->HostState.ContextFrame);

    //����ط��洢�˻�����Ҳ��˵��RIPҲ�ᱻ�����ȥ������GUEST_RIP���Ҳ���������������ط��ᱻ�������Σ�������Ҫ�ж�һ��VMX�Ƿ�����
    if (pForgVmxEntry->HyperIsEnable == false)
    {
        //����VMCS��VMXON���ȵ�����
        Status = Frog_AllocateHyperRegion(pForgVmxEntry, CpuNumber);
        if (!Frog_SUCCESS(Status))
        {
            FrogBreak();
            FrogPrint("AllocateHyperRegion	Error");
            goto	_HyperInitExit;
        }

        if (g_FrogCpu->EnableEpt)
        {
            Frog_BuildEpt(pForgVmxEntry);//��ʼ��EPT�ڴ�
            Frog_SetEptp(pForgVmxEntry);
        }

        //����VMCS��VMXON�汾��
        Frog_SetHyperRegionVersion(pForgVmxEntry, CpuNumber);

        //VMXON
        if (__vmx_on(&pForgVmxEntry->VmxOnAreaPhysicalAddr))
        {
            FrogBreak();
            FrogPrint("Vmxon	 Error");
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
            FrogPrint("ForgVmptrld Error");
            goto _HyperInitExit;
        }

        //VMCS
        Status = Frog_SetupVmcs(pForgVmxEntry);
        if (!Frog_SUCCESS(Status)) {
            FrogBreak();
            FrogPrint("Frog_SetupVmcs Error");
            goto	_HyperInitExit;
        }

        pForgVmxEntry->HyperIsEnable = true;
        if (__vmx_vmlaunch())
        {
			pForgVmxEntry->HyperIsEnable = false;
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
    g_FrogCpu->KernelCr3 = __readcr3();//DPC��Ͷ�ķ�ʽ�ᵼ�½��뵽��ͬ�Ľ��̻����У�������Ҫ�����ں˵�CR3

	//��ѯ�Ƿ�֧�����⻯
	if (!Frog_IsSupportHyper()) {
		FrogBreak();
		FrogPrint("NoSupportHyper");
		return NoSupportHyper;
	} 

	//����MSR��λ��֧�����⻯
	Frog_SetMsrBitToEnableHyper();

    //��ȡMTRR��Ϣ
    Frog_GetMtrrInfo();

	KeGenericCallDpc(Frog_DpcRunHyper, NULL);

	return	FrogSuccess;

}


//--------------------------------------DisableHype


void RunEachProcessToDisableHyper(unsigned long ProcessorIndex)
{
    pFrogVmx		pForgVmxEntry = NULL;
    pForgVmxEntry = &g_FrogCpu->pForgVmxEntrys[ProcessorIndex];

    if (Frog_VmCall(FrogExitTag, 0, 0, 0))
    {
        Cr4 cr4 = { 0 };
        cr4.all = __readcr4();
        cr4.fields.vmxe = 0;
        __writecr4(cr4.all);

        Frog_FreeHyperRegion(pForgVmxEntry);
    }
}

FrogRetCode	Frog_DisableHyper() 
{
    Frog_RunEachProcessor(RunEachProcessToDisableHyper);
    Frog_UnHook();
    __writemsr(kIa32FeatureControl, g_FrogCpu->OrigFeatureControlMsr.all);
    if (g_FrogCpu->pForgVmxEntrys)		FrogExFreePool(g_FrogCpu->pForgVmxEntrys);
	if (g_FrogCpu)		FrogExFreePool(g_FrogCpu);
    sleep(15000);
	return	FrogSuccess;
}