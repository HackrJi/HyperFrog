#include "vt.h"
#include "vt_help.h"
EXTERN_C	pFrogCpu		Frog_Cpu;

void			vmexit_readmsr_handle(pFrog_GuestContext	Context)
{
	ULONG64		MsrValue = 0;

	MsrValue = (ULONG64)__readmsr((ULONG)Context->Rcx);

	Context->Rax = LODWORD(MsrValue);
	Context->Rdx = HIDWORD(MsrValue);

}
void			vmexit_cpuid_handle(pFrog_GuestContext	    Context)
{
	int		CpuidInfo[4] = { 0 };
	__cpuidex(CpuidInfo, (int)Context->Rax,(int)Context->Rcx);
	Context->Rax = (ULONG64)CpuidInfo[0];
	Context->Rbx = (ULONG64)CpuidInfo[1];
	Context->Rcx = (ULONG64)CpuidInfo[2];
	Context->Rdx = (ULONG64)CpuidInfo[3];

	return;
}
void         vmexit_craccess_handle(pFrog_GuestContext	Context)
{
    CrxVmExitQualification          CrxQualification = { 0 };
    PULONG64                            RegContext = (PULONG64)Context;
    ULONG64                             Register = 0;

    CrxQualification.all = Frog_Vmx_Read(EXIT_QUALIFICATION);
    Register = RegContext[CrxQualification.Bits.gp_register];

    if (CrxQualification.Bits.access_type == kMoveToCr)
    {
        switch (CrxQualification.Bits.crn)
        {
        case 0:
            {
                Frog_Vmx_Write(GUEST_CR0, Register);
                break;
            }
        case 3:
        {
            Frog_Vmx_Write(GUEST_CR3, Register);
            break;
        }
        case 4:
            {
                Frog_Vmx_Write(GUEST_CR4, Register);
                break;
            } 
        }

    }


}

EXTERN_C VOID		vmexit_handle(pFrog_GuestContext	Context)
{
	VmxExitInfo		ExitInfo = { 0 };
	ULONG64		Rip = 0;
	ULONG64		Rsp = 0;
	ULONG64		ExitinstructionsLength = 0;
    FlagReg           GuestRflag = { 0 };
   
	ExitInfo.all = 	(ULONG32)Frog_Vmx_Read(VM_EXIT_REASON);

    FrogBreak();
	switch (ExitInfo.fields.reason)
	{
		case	ExitCpuid:
			vmexit_cpuid_handle(Context);
			break;
		case ExitInvd:
			__wbinvd();
			break;
		case ExitGetSec://��ʱ������
			
			break;
		case ExitXsetbv:
			_xsetbv((ULONG32)Context->Rcx, MAKEQWORD(Context->Rax, Context->Rdx));
			break;
		case ExitMsrRead:
			vmexit_readmsr_handle(Context);
			break;
        case ExitCrAccess:
            vmexit_craccess_handle(Context);
            break;
		case ExitInvept:
		case ExitInvvpid:
		case ExitVmcall:
		case  ExitVmclear:
		case  ExitVmlaunch:
		case  ExitVmptrld:
		case  ExitVmptrst:
		case  ExitVmread:
		case  ExitVmresume:
		case  ExitVmwrite:
		case  ExitVmoff:
		case  ExitVmon:
        {
            GuestRflag.all = Frog_Vmx_Read(GUEST_RFLAGS);
            GuestRflag.fields.cf = 1;//�ܾ�Ƕ��
            Frog_Vmx_Write(GUEST_RFLAGS, GuestRflag.all);
        }
			break;
		default:
			break;

	}

    //������������
	Rip =	Frog_Vmx_Read(GUEST_RIP);
	Rsp =   Frog_Vmx_Read(GUEST_RSP);
	ExitinstructionsLength = Frog_Vmx_Read(VM_EXIT_INSTRUCTION_LEN);
	Rip += ExitinstructionsLength;

    Frog_PrintfEx("reason=%d rip=%p", ExitInfo.fields.reason,Rip);
	Frog_Vmx_Write(GUEST_RIP, Rip);
	Frog_Vmx_Write(GUEST_RSP, Rsp);
    return;
}