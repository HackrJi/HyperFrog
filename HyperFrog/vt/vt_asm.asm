extern RtlCaptureContext:proc
extern vmexit_handle:proc

.code

VmxEntryPointer	Proc
	int 3
	push rcx
	lea rcx,[rsp + 8h] ;���� + 8 ����Ϊ����push��rcx
    call    RtlCaptureContext
	jmp		vmexit_handle


VmxEntryPointer endp

END