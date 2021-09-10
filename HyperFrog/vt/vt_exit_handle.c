#include "vt.h"

EXTERN_C VOID vmexit_handle(IN PCONTEXT Context)
{
	KIRQL	Irql = 0;

	Irql = KeGetCurrentIrql();
	if (Irql < DISPATCH_LEVEL) Irql = KeRaiseIrqlToDpcLevel();


	//��ԭRCX
	Context->Rcx = *(PULONG64)((ULONG_PTR)Context - sizeof(Context->Rcx));




	KeLowerIrql(Irql);

}