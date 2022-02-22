#include "public.h"

void	Frog_PrintfEx(char *format, ...) {
	NTSTATUS	Status = STATUS_SUCCESS;
	char buf[1024] = { 0 };
	va_list args = NULL;

	va_start(args, format);
	Status = RtlStringCchVPrintfA(buf, RTL_NUMBER_OF(buf), format,args);
	va_end(args);

	if (!NT_SUCCESS(Status))
	{
		FrogBreak();
		return;
	}

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[-]Frog : %s \r\n", buf);
}

ULONG RvaToOffset(PIMAGE_NT_HEADERS pnth, ULONG Rva, ULONG FileSize)
{
    PIMAGE_SECTION_HEADER psh = IMAGE_FIRST_SECTION(pnth);
    USHORT NumberOfSections = pnth->FileHeader.NumberOfSections;
    for (int i = 0; i < NumberOfSections; i++)
    {
        if (psh->VirtualAddress <= Rva)
        {
            if ((psh->VirtualAddress + psh->Misc.VirtualSize) > Rva)
            {
                Rva -= psh->VirtualAddress;
                Rva += psh->PointerToRawData;
                return Rva < FileSize ? Rva : 0;
            }
        }
        psh++;
    }
    return 0;
}

void sleep(LONG milliseconds)
{
    LARGE_INTEGER interval;
    interval.QuadPart = -10000ll * milliseconds;

    KeDelayExecutionThread(KernelMode, FALSE, &interval);
}


// �޸�Cr0�Ĵ���, ȥ��д�������ڴ汣�����ƣ�
KIRQL RemovWP()
{
    KIRQL irQl;
    //DbgPrint("RemovWP\n");
    // (PASSIVE_LEVEL)���� IRQL �ȼ�ΪDISPATCH_LEVEL�������ؾɵ� IRQL
    // ��Ҫһ���ߵ�IRQL�����޸�
    irQl = KeRaiseIrqlToDpcLevel();
    ULONG_PTR cr0 = __readcr0(); // ������������ȡCr0�Ĵ�����ֵ, �൱��: mov eax,  cr0;

    // ����16λ��WPλ����0������д����
    cr0 &= ~0x10000; // ~ ��λȡ��
    _disable(); // ����жϱ��, �൱�� cli ָ��޸� IF��־λ
    __writecr0(cr0); // ��cr0������������д��Cr0�Ĵ����У��൱��: mov cr0, eax
    //DbgPrint("�˳�RemovWP\n");
    return irQl;
}

// ��ԭCr0�Ĵ���
KIRQL UnRemovWP(KIRQL irQl)
{

    //DbgPrint("UndoWP\n");
    ULONG_PTR cr0 = __readcr0();
    cr0 |= 0x10000; // WP��ԭΪ1
    _disable(); // ����жϱ��, �൱�� cli ָ���� IF��־λ
    __writecr0(cr0); // ��cr0������������д��Cr0�Ĵ����У��൱��: mov cr0, eax

    // �ָ�IRQL�ȼ�
    KeLowerIrql(irQl);
    //DbgPrint("�˳�UndoWP\n");
    return irQl;
}


// ��ȡ�����ַ��Ӧ��Pte
// �� ring3 ���ڴ�ӳ�䵽 ring0 ������һ���ں� LinerAddress
VOID* GetKernelModeLinerAddress(ULONG_PTR cr3, ULONG_PTR user_mode_address,size_t size)
{
    PHYSICAL_ADDRESS cr3_phy = { 0 };
    cr3_phy.QuadPart = cr3;
    ULONG_PTR current_cr3 = 0;
    PVOID cr3_liner_address = NULL;

    PHYSICAL_ADDRESS user_phy = { 0 };
    PVOID kernel_mode_liner_address = NULL;

    // �ж�cr3�Ƿ���ȷ	
    cr3_liner_address = MmGetVirtualForPhysical(cr3_phy);
    if (!MmIsAddressValid(cr3_liner_address)) {
        return NULL;
    }
    // �ж��Ƿ�Ϊ rin3 �ĵ�ַ �Լ� ��ַ�Ƿ�ɶ�ȡ
    else if (user_mode_address >= 0xFFFFF80000000000) {
        // ���Ϊ�ں˵�ַ, ����Ҫӳ��
        return (void*)user_mode_address;
    }
    // �����ַ���ɶ�
    else if (!MmIsAddressValid((void*)user_mode_address)) {
        return NULL;
    }

    current_cr3 = __readcr3();
    // �ر�д�������л�Cr3
    KIRQL Irql = RemovWP();
    __writecr3(cr3_phy.QuadPart);

    // ӳ�� user mode �ڴ�	
    user_phy = MmGetPhysicalAddress((void*)user_mode_address);
    //PVOID kernel_mode_liner_address = MmGetVirtualForPhysical(user_phy); //(ֱ�ӷֽ�PTE����ʽ��ȡ��Ӧ�������ַ)
    kernel_mode_liner_address = MmMapIoSpace(user_phy, size, MmNonCached); // ӳ��rin3�ڴ浽rin0

    // �ָ�
    __writecr3(current_cr3);
    UnRemovWP(Irql);

    if (kernel_mode_liner_address) {
        return kernel_mode_liner_address;
    }
    else
        return NULL;
}

VOID FreeKernelModeLinerAddress(VOID* p, size_t size)
{
    if ((ULONG_PTR)p < 0xFFFFF80000000000) {
        if (p && size) {
            MmUnmapIoSpace(p, size);
        }
    }
}