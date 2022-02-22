#include "public.h"

bool Frog_EferHookEnable()
{
	Ia32VmxBasicMsr VmxBasicMsr = { 0 };
	ULONG UsetrueMsrs = 0;
    VmxBasicMsr.all = __readmsr(kIa32VmxBasic);
    UsetrueMsrs = (bool)VmxBasicMsr.fields.vmx_capability_hint;

	VmxVmentryControls	VmVmentryControls = { 0 };
	VmxmexitControls		VmExitControls = { 0 };
	Efer efer = { 0 };
    efer.all = __readmsr(kIa32Efer);
    efer.Bits.sce = false;
	VmVmentryControls.all = (unsigned int)Frog_Vmx_Read(VM_ENTRY_CONTROLS);
	VmExitControls.all = (unsigned int)Frog_Vmx_Read(VM_EXIT_CONTROLS);

    //   1. Enable VMX
    //   2. ���� VM - entry �е� load_ia32_efer �ֶ�
    //   3. ���� VM - exit �е� save_ia32_efer �� load_ia32_efer �ֶ�
    //   4. ���� MSR - bitmap ������, д��Ͷ�ȡEFER MSRʱ�˳� ����
    //   5. ���� Exception - bitmap ���� #UD �쳣
    //   7. ��� sce λ
    //   8. ���� SysCall �� SysRet ָ��µ� #UD �쳣


	VmVmentryControls.fields.load_ia32_efer = true;
    VmExitControls.fields.load_ia32_efer = true;
    VmExitControls.fields.save_ia32_efer = true;

    ULONG ExceptionBitmap = 0;
    ExceptionBitmap |= 1 << VECTOR_INVALID_OPCODE_EXCEPTION;//����UD�쳣
    Frog_Vmx_Write(EXCEPTION_BITMAP, ExceptionBitmap);

	Frog_Vmx_Write(VM_ENTRY_CONTROLS, VmVmentryControls.all);
	Frog_Vmx_Write(VM_EXIT_CONTROLS, VmExitControls.all);
	Frog_Vmx_Write(GUEST_EFER, efer.all);

    return true;
}

bool Frog_EmulateSyscall(PCONTEXT Context)
{
	// ��ȡ������Ϣ
	PNT_KPROCESS current_process = (PNT_KPROCESS)PsGetCurrentProcess();
	ULONG_PTR MsrValue = 0;
	FrogRetCode Status = FrogSuccess;
	ULONG_PTR guestRip = Frog_Vmx_Read(GUEST_RIP);
	//ULONG_PTR guestRsp = VtBase::VmCsRead(GUEST_RSP);
	ULONG_PTR GuestRflags = Frog_Vmx_Read(GUEST_RFLAGS);
	//ULONG_PTR guest_r3_cr3 = VtBase::VmCsRead(GUEST_CR3);
	ULONG_PTR exitInstructionLength = Frog_Vmx_Read(VM_EXIT_INSTRUCTION_LEN); // �˳���ָ���

	// �ο���Ƥ�� SYSCALL��Fast System Call

	/*
		a.	SysCall loading Rip From the IA32_LSTA MSR
		b.	SysCall ���� IA32_LSTA MSR ��ֵ�� Rip ��
	*/
	//MsrValue = __readmsr(MSR_LSTAR);
	// �����ǵ�����
	MsrValue = (ULONG_PTR)FakeKiSystemCall64;
	Status |= Frog_Vmx_Write(GUEST_RIP, MsrValue);

	/*
		a.	After Saving the Adress of the instruction following SysCall into Rcx
		b.	SysCall �Ὣ��һ��ָ���ַ���浽 Rcx ��
	*/
	ULONG_PTR next_instruction = exitInstructionLength + guestRip;
    Context->Rcx = next_instruction;
	/*
		a. Save RFLAGS into R11 and then mask RFLAGS using MSR_FMASK.
		b. ���� RFLAGS �� R11 �Ĵ�����, ����ʹ�� MSR_FMASK ��� RFLAGS ��Ӧ��ÿһλ
	*/
    
	MsrValue = __readmsr(kIa32Fmask);
	Context ->R11= GuestRflags;
	GuestRflags &= ~(MsrValue | X86_FLAGS_RF);
	Frog_Vmx_Write(GUEST_RFLAGS, GuestRflags);

	/*
		a. SYSCALL loads the CS and SS selectors with values derived from bits 47:32 of the IA32_STAR MSR.
		b. SysCall ���� CS��SS �μĴ�����ֵ������ IA32_STAR MSR �Ĵ����� 32:47 λ
	*/
    MsrValue = __readmsr(kIa32Star);
	ULONG_PTR Cs = (UINT16)((MsrValue >> 32) & ~3);
    Status |= Frog_Vmx_Write(GUEST_CS_SELECTOR, Cs);
    Status |= Frog_Vmx_Write(GUEST_CS_LIMIT, (UINT32)~0);
    Status |= Frog_Vmx_Write(GUEST_CS_AR_BYTES, 0xA09B);
    Status |= Frog_Vmx_Write(GUEST_CS_BASE, 0);

	ULONG_PTR Ss = Cs + 0x8;
    Status |= Frog_Vmx_Write(GUEST_SS_SELECTOR, Ss);
    Status |= Frog_Vmx_Write(GUEST_SS_LIMIT, (UINT32)~0);
    Status |= Frog_Vmx_Write(GUEST_SS_AR_BYTES, 0xC093);
    Status |= Frog_Vmx_Write(GUEST_SS_BASE, 0);

	Status |= Frog_Vmx_Write(GUEST_CR3, current_process->DirectoryTableBase);
	
	if (!Frog_SUCCESS(Status))
		return false;
	
	return true;
}

bool Frog_EmulateSysret(PCONTEXT Context)
{
	// ��ȡ������Ϣ
	//PNT_KPROCESS current_process = (PNT_KPROCESS)PsGetCurrentProcess();
	FrogRetCode Status = FrogSuccess;
	ULONG_PTR MsrValue = 0;
	ULONG_PTR GuestRflags =
		(Context->R11 & ~(X86_FLAGS_RF | X86_FLAGS_VM | X86_FLAGS_RESERVED_BITS)) | X86_FLAGS_FIXED;

	// �ο���Ƥ�� SYSRET��Return From Fast System Call
	/*
		a. It does so by loading RIP from RCX and loading RFLAGS from R11
		b. ���� RCX ֵ���ص� RIP , �� R11 ��ֵ���ص� RFLAGS ��������һ��
	*/

	Status |= Frog_Vmx_Write(GUEST_RIP, Context->Rcx);
	Status |= Frog_Vmx_Write(GUEST_RFLAGS, GuestRflags);

	/*
		a. SYSRET loads the CS and SS selectors with values derived from bits 63:48 of the IA32_STAR MSR.
		b. SysRet ���� CS��SS �μĴ�����ֵ������ IA32_STAR MSR �Ĵ����� 48:63 λ
	*/
	
    MsrValue = __readmsr(kIa32Star);
	ULONG_PTR Cs = (UINT16)(((MsrValue >> 48) + 16) | 3);
	Status |= Frog_Vmx_Write(GUEST_CS_SELECTOR, Cs);
	Status |= Frog_Vmx_Write(GUEST_CS_LIMIT, (UINT32)~0);
	Status |= Frog_Vmx_Write(GUEST_CS_AR_BYTES, 0xA0FB);
	Status |= Frog_Vmx_Write(GUEST_CS_BASE, 0);

	ULONG_PTR Ss = (UINT16)(((MsrValue >> 48) + 8) | 3);
    Status |= Frog_Vmx_Write(GUEST_SS_SELECTOR, Ss);
    Status |= Frog_Vmx_Write(GUEST_SS_LIMIT, (UINT32)~0);
    Status |= Frog_Vmx_Write(GUEST_SS_AR_BYTES, 0xC0F3);
    Status |= Frog_Vmx_Write(GUEST_SS_BASE, 0);

    if (!Frog_SUCCESS(Status))
        return false;

    return true;
}