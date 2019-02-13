/*

A�ܵ�ring3��ring0ͨ���ļ�������NtReadFile������ͨ��

*/

#include "Control.h"


/*

��ȡEPROCESS���̵��ļ���С
����������PEPROCESS

*/
ULONG GetCallerFileSize(__in PEPROCESS Eprocess)
{
	WCHAR CallerFilePath[260] = {0};
	ULONG ulSizeRet = 0;
	UNICODE_STRING UnicodeCallerFile;
	// ��ʼ���ļ�·��
	OBJECT_ATTRIBUTES obj_attrib;
	NTSTATUS status;
	IO_STATUS_BLOCK Io_Status_Block;
	ULONG ulHighPart;
	ULONG ulLowPart;
	HANDLE hFile;

	if (DebugOn)
		KdPrint(("GetCallerFile:%08x\r\n",Eprocess));

	memset(CallerFilePath,0,sizeof(CallerFilePath));
	if (GetProcessFullImagePath(Eprocess,&CallerFilePath))
	{
		if (DebugOn)
			KdPrint(("GetCallerFile:%ws\r\n",CallerFilePath));

		RtlInitUnicodeString(&UnicodeCallerFile,CallerFilePath);
		InitializeObjectAttributes(
			&obj_attrib,
			&UnicodeCallerFile,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL, 
			NULL
			);
		status = IoCreateFile(
			&hFile,
			GENERIC_READ,  //��ֻ���ķ�ʽ�򿪣���Ȼ����ʾ����32
			&obj_attrib,
			&Io_Status_Block,
			0,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_OPEN_IF,
			0,
			NULL,
			0,
			0,
			NULL,
			IO_NO_PARAMETER_CHECKING
			);
		if (NT_SUCCESS(status))
		{
			ulLowPart = CsGetFileSize(hFile,&ulHighPart);
			if (ulLowPart != -1)
			{
				if (DebugOn)
					KdPrint(("FileSize:%d\r\n",ulLowPart));

				ulSizeRet = ulLowPart;
			}
			ZwClose(hFile);
		}
	}
	return ulLowPart;
}
/*

ͨ�ź���

*/
NTSTATUS __stdcall NewNtReadFile(
	__in      HANDLE FileHandle,
	__in_opt  HANDLE Event,
	__in_opt  PIO_APC_ROUTINE ApcRoutine,
	__in_opt  PVOID ApcContext,
	__out     PIO_STATUS_BLOCK IoStatusBlock,
	__out     PVOID Buffer,
	__in      ULONG Length,
	__in_opt  PLARGE_INTEGER ByteOffset,
	__in_opt  PULONG Key
	)
{
	NTSTATUS status;
	ULONG ulSize;
	ULONG ulKeServiceDescriptorTable;
	int i=0,x=0;
	BOOL bInit = FALSE;
	WIN_VER_DETAIL WinVer;
	HANDLE HFileHandle;
	WCHAR lpwzKey[256];
	WCHAR lpwzModule[256];
	char *lpszProName = NULL;
	BOOL bIsNormalServices = FALSE;
	ULONG g_Offset_Eprocess_ProcessId;
	PVOID KernelBuffer;
	ULONG ulCsrssTemp;
	ULONG ulRealDispatch;
	CHAR lpszModule[256];
	ZWREADFILE OldZwReadFile;
	BOOL bIsMyCommand = FALSE;
	KIRQL oldIrql;
	ULONG ulReLoadSelectModuleBase = 0;

	OldZwReadFile = OriginalServiceDescriptorTable->ServiceTable[ZwReadFileIndex];

	//IRQL�п��ܹ���Ŷ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL){
		goto _FunctionRet;
	}
	/*

	���û���ҵ�Ҫ���صĽ���explorer����ö�ٽ��̲�����
	���سɹ�����reload win32K
	Ȼ���ʼ������������̴���
	*/
	if (!IsExitProcess(AttachGuiEProcess))
	{
		lpszProName = (char *)PsGetProcessImageFileName(RPsGetCurrentProcess());
		if (_strnicmp(lpszProName,"csrss.exe",strlen("csrss.exe")) == 0)
		{
			//��ȡcsrss��eprocess�����ﲻ�ܻ�ȡ������gui����Ȼ��KeInsertQueueApc����ͻῨס����ɽ����޷��˳��ȵ�����
			AttachGuiEProcess = RPsGetCurrentProcess();

			//�����Լ�
			if (!bProtect)
			{
				//���أ�Ȼ��reload win32K
				if (ReloadWin32K() == STATUS_SUCCESS)
				{
					KdPrint(("Init Win32K module success\r\n"));
					bInitWin32K = TRUE; //success
				}
				ProtectCode();
				bProtect = TRUE;
			}
		}
	}
	/*

	������ring3�򿪶Ի����ʱ��InitSuccessҪΪFALSE������A�ܽ���Ҫ���ڣ��Ϳ�����ͣ�±���
	��ͣ��ʱ��ֱ������SAFE_SYSTEM����

	*/
	if (FileHandle == RESUME_PROTECT &&
		bIsInitSuccess == FALSE &&
		IsExitProcess(ProtectEProcess))
	{
		goto _ResumeProtect;
	}
	/*

	�ж��Ƿ���A�ܽ��̵�����

	*/
	if (bIsInitSuccess == TRUE &&
		IsExitProcess(ProtectEProcess)){
			if (RPsGetCurrentProcess() == ProtectEProcess){
				bIsMyCommand = TRUE;
			}
	}
	/*

	����A������������ô��ֻ����Ƿ���SAFE_SYSTEM��������ǣ�ֱ�ӷ���
	�����SAFE_SYSTEM��˵����A�ܻ�������������׼����ʼ��
	�����SAFE_SYSTEM������ProtectEProcessȴ���ڣ�˵����˫����ֱ�ӷ���
	*/
	if (!bIsMyCommand){
		//ֻҪ����SAFE_SYSTEM�����һ�ɷ��أ�
		if (FileHandle != SAFE_SYSTEM){
			goto _FunctionRet;
		}
		//�����SAFE_SYSTEM������ҽ��̻��ڵ�ʱ��Ҳ����
		if (FileHandle == SAFE_SYSTEM)
		{
			if (IsExitProcess(ProtectEProcess)){
				goto _FunctionRet;
			}
		}
	}
_ResumeProtect:
	if (Buffer != NULL &&
		Length > 0)
	{
		__try{
			ProbeForRead( Buffer, Length, sizeof( UCHAR ) );
			ProbeForWrite( Buffer, Length, sizeof( UCHAR ) );
		}__except(EXCEPTION_EXECUTE_HANDLER){
			return STATUS_UNSUCCESSFUL;
		}
	}
	if (FileHandle == START_IO_TIMER)
	{
		if (DebugOn)
			KdPrint(("start io time:%08x\n",Length));

		if (MmIsAddressValidEx((PDEVICE_OBJECT)Length)){
			IoTimerControl((PDEVICE_OBJECT)Length,TRUE);
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == STOP_IO_TIMER)
	{
		if (DebugOn)
			KdPrint(("stop io time:%08x\n",Length));

		if (MmIsAddressValidEx((PDEVICE_OBJECT)Length)){
			IoTimerControl((PDEVICE_OBJECT)Length,FALSE);
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_IO_TIMER)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		IoTimer = (PMyIoTimer)RExAllocatePool(NonPagedPool,sizeof(MyIoTimer)*256);
		if (!IoTimer)
		{
			return;
		}
		memset(IoTimer,0,sizeof(MyIoTimer)*256);
		QueryIoTimer(IoTimer);
		if (Length > sizeof(MyIoTimer)*256)
		{
			for (i=0;i<IoTimer->ulCount;i++)
			{
				if (DebugOn) 
					KdPrint(("DeviceObject:%08x\nTimerRoutine:%08x\r\nModule:%s\r\nstatus:%d\r\n\r\n",
					IoTimer->MyTimer[i].DeviceObject,
					IoTimer->MyTimer[i].IoTimerRoutineAddress,
					IoTimer->MyTimer[i].lpszModule,
					IoTimer->MyTimer[i].ulStatus));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,IoTimer,sizeof(MyIoTimer)*256);
			Length = sizeof(MyIoTimer)*256;
		}
		RExFreePool(IoTimer);
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ָ���ѡ��ģ���inline hook��Ҫ�ĳ�ʼ��ģ��Ļ�ַ

	*/
	if (FileHandle == INIT_SET_SELECT_INLINE_HOOK_1)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			ulInitRealModuleBase = Length;
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ָ���ѡģ���inline hook��Ҫ�ĳ�ʼ����������ʵ��ַ

	*/
	if (FileHandle == INIT_SET_SELECT_INLINE_HOOK)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			ulInitRealFuncBase = Length;
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�����ǻָ�inlinehook��anti inlinehook������
	�������������
	������������֮�󣬽��и�ģ�������
	����֮��Ϳ�ʼ�ֱ��ж�����

	*/
	if (FileHandle == SET_SELECT_INLINE_HOOK ||
		FileHandle == ANTI_SELECT_INLINE_HOOK)
	{
		if (MmIsAddressValidEx(Buffer) &&
			Length*2 < sizeof(lpwzModule) &&
			ulInitRealFuncBase &&
			ulInitRealModuleBase)
		{
			memset(lpwzModule,0,sizeof(lpwzModule));
			memcpy(lpwzModule,Buffer,Length*2);

			if (DebugOn)
				KdPrint(("func:%08x module:%08x path:%ws\n",ulInitRealFuncBase,ulInitRealModuleBase,lpwzModule));

			//���ص�ǰģ��
			//c:\\windows\\system32\\drivers\\tcpip.sys
			//������IsFileInSystem��������飬\\??\\c:\\windows\\system32\\drivers\\tcpip.sys�ŷ��ϼ�麯����·��
			if (PeLoad(
				lpwzModule,
				&ulReLoadSelectModuleBase,
				PDriverObject,
				ulInitRealModuleBase
				))
			{
				if (DebugOn)
					KdPrint(("reload success:%08x\n",ulReLoadSelectModuleBase));

				if (FileHandle == SET_SELECT_INLINE_HOOK){
					RestoreInlineHook(ulInitRealFuncBase,ulInitRealModuleBase,ulReLoadSelectModuleBase);
				}
				if (FileHandle == ANTI_SELECT_INLINE_HOOK){
					AntiInlineHook(ulInitRealFuncBase,ulInitRealModuleBase,ulReLoadSelectModuleBase);
				}
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ɨ����ѡ������inlinhook������һ���ṹ���ڱ���ɨ�赽�ù���

	*/
	if (FileHandle == LIST_SELECT_MODULE_INLINE_HOOK)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		SelectModuleInlineHookInfo = (PINLINEHOOKINFO)RExAllocatePool(NonPagedPool,SystemKernelModuleSize+260);
		if (SelectModuleInlineHookInfo)
		{
			memset(SelectModuleInlineHookInfo,0,SystemKernelModuleSize+260);
			KernelHookCheck(SelectModuleInlineHookInfo,SelectModule);
			if (Length > SystemKernelModuleSize+260)
			{
				for (i=0;i<SelectModuleInlineHookInfo->ulCount;i++)
				{
					if (DebugOn)
						KdPrint(("[%d]SelectModuleHook\r\n"
						"���ҹ���ַ:%08x\r\n"
						"ԭʼ��ַ:%08x\r\n"
						"�ҹ�����:%s\r\n"
						"hook��ת��ַ:%08x\r\n"
						"����ģ��:%s\r\n"
						"ģ���ַ:%08x\r\n"
						"ģ���С:%x\r\n",
						i,
						SelectModuleInlineHookInfo->InlineHook[i].ulMemoryFunctionBase,
						SelectModuleInlineHookInfo->InlineHook[i].ulRealFunctionBase,
						SelectModuleInlineHookInfo->InlineHook[i].lpszFunction,
						SelectModuleInlineHookInfo->InlineHook[i].ulMemoryHookBase,
						SelectModuleInlineHookInfo->InlineHook[i].lpszHookModuleImage,
						SelectModuleInlineHookInfo->InlineHook[i].ulHookModuleBase,
						SelectModuleInlineHookInfo->InlineHook[i].ulHookModuleSize
						));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,SelectModuleInlineHookInfo,SystemKernelModuleSize+260);
				Length = SystemKernelModuleSize+260;
			}
			RExFreePool(SelectModuleInlineHookInfo);
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ָ���ѡ������hook֮ǰ������Ҫ��ʼ����ǰģ���PDB����PDB��������ȡ������ַ��������
	�����ǰ��������win32K������Ҫ�ҿ���GUI�߳�

	*/
	if (FileHandle == INIT_SELECT_MODULE_INLINE_HOOK)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RKeAttachProcess,L"KeAttachProcess",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RKeDetachProcess,L"KeDetachProcess",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy &&
			RKeAttachProcess &&
			RKeDetachProcess)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		if (DebugOn)
			KdPrint(("INIT_SELECT_MODULE_INLINE_HOOK\n"));

		if (MmIsAddressValidEx(Buffer) &&
			SystemKernelModuleSize+1024 > Length &&
			Length > SystemKernelModuleSize)
		{
			if (SelectModuleFuncInfo)
				RExFreePool(SelectModuleFuncInfo);

			SelectModuleFuncInfo = RExAllocatePool(PagedPool,SystemKernelModuleSize+1024);    //�����㹻��Ļ���
			if (!SelectModuleFuncInfo){
				return STATUS_UNSUCCESSFUL;
			}
			memset(SelectModuleFuncInfo,0,SystemKernelModuleSize+1024);
			//��ring3�õ��ں���Ϣ
			Rmemcpy(SelectModuleFuncInfo,Buffer,Length);

			if (DebugOn)
				KdPrint(("copy memory\n"));

			if (SelectModuleFuncInfo->ulCount > 10){

				if (DebugOn)
					KdPrint(("reload path:0x%08X:%ws\n",SelectModuleFuncInfo->ulModuleBase,SelectModuleFuncInfo->ModulePath));
				//�����win32K������
				if (SelectModuleFuncInfo->ulModuleBase == ulWin32kBase){
					RKeAttachProcess(AttachGuiEProcess);
				}
				//���ص�ǰģ��
				if (PeLoad(
					SelectModuleFuncInfo->ModulePath,
					&ulReLoadSelectModuleBase,
					PDriverObject,
					SelectModuleFuncInfo->ulModuleBase
					))
				{
					for (i=0;i<SelectModuleFuncInfo->ulCount;i++)
					{
						//�������غ�ĵ�ַ�����棬�����ǰ��ַ��Ч����ֵ0
						if (wcslen(SelectModuleFuncInfo->NtosFuncInfo[i].FuncName) &&
							MmIsAddressValidEx(ulReLoadSelectModuleBase) &&
							MmIsAddressValidEx(SelectModuleFuncInfo->NtosFuncInfo[i].ulAddress)){
							SelectModuleFuncInfo->NtosFuncInfo[i].ulReloadAddress = SelectModuleFuncInfo->NtosFuncInfo[i].ulAddress - SelectModuleFuncInfo->ulModuleBase + (ULONG)ulReLoadSelectModuleBase;
						}else{
							SelectModuleFuncInfo->NtosFuncInfo[i].ulReloadAddress = 0;
							SelectModuleFuncInfo->NtosFuncInfo[i].ulAddress = 0;
							//KdPrint(("%ws : 0x%X\n",SelectModuleFuncInfo->NtosFuncInfo[i].FuncName,SelectModuleFuncInfo->NtosFuncInfo[i].ulAddress));
						}
					}
				}
				if (SelectModuleFuncInfo->ulModuleBase == ulWin32kBase){
					RKeDetachProcess();
				}
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ֶ�����

	*/
	if (FileHandle == KERNEL_BSOD)
	{
		oldIrql = KeRaiseIrqlToDpcLevel();
		PsGetProcessImageFileName(PsGetCurrentProcess());

		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ں����ݲ鿴��Ҫ�鿴�Ĵ�С

	*/
	if (FileHandle == INIT_KERNEL_DATA_SIZE)
	{
		if (Length > 0x10)
		{
			ulLookupSize = Length;
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ں����ݲ鿴��Ҫ�鿴����ʼ��ַ

	*/
	if (FileHandle == INIT_KERNEL_DATA_BASE)
	{
		if (Length > 0x123456 &&
			MmIsAddressValidEx(Length))
		{
			LookupBase = Length;
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ʼ��ȡ�ں����ݣ������͵�ring3

	*/
	if (FileHandle == LIST_KERNEL_DATA)
	{
		if (!MmIsAddressRangeValid(LookupBase,ulLookupSize)){
			return STATUS_UNSUCCESSFUL;
		}
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		LookupKernelData = (PLOOKUP_KERNEL_DATA)RExAllocatePool(PagedPool,SystemKernelModuleSize);    //�����㹻��Ļ���
		if (LookupKernelData == NULL) 
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(LookupKernelData,0,SystemKernelModuleSize);
		LookupKernelDataInfo(LookupBase,ulLookupSize,LookupKernelData);
		if (Length > SystemKernelModuleSize)
		{
			for (i=0;i<LookupKernelData->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("0x%08x %08x %08x %08x %08x\n",
					LookupKernelData->KernelData[i].ulAddress,
					LookupKernelData->KernelData[i].ulStack1,
					LookupKernelData->KernelData[i].ulStack2,
					LookupKernelData->KernelData[i].ulStack3,
					LookupKernelData->KernelData[i].ulStack4));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,LookupKernelData,SystemKernelModuleSize);
			Length = SystemKernelModuleSize;
		}
		RExFreePool(LookupKernelData);
		LookupBase = 0;
		ulLookupSize = 0;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_THREAD_STACK)
	{
		if (Length > 0x123456 &&
			MmIsAddressValidEx(Length) &&
			!PsIsThreadTerminating(Length))
		{
			ulThread = Length;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_THREAD_STACK)
	{
		if (!ulThread)
			return STATUS_UNSUCCESSFUL;

		if (Length > 0x123456)
		{
			ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
			ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
			ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
			if (RExAllocatePool &&
				RExFreePool &&
				Rmemcpy)
			{
				bInit = TRUE;
			}
			if (!bInit)
				return STATUS_UNSUCCESSFUL;

			ThreadStack = RExAllocatePool(PagedPool,SystemKernelModuleSize);    //�����㹻��Ļ���
			if (ThreadStack == NULL) 
			{
				return STATUS_UNSUCCESSFUL;
			}
			memset(ThreadStack,0,SystemKernelModuleSize);
			ReadThreadStack((PETHREAD)ulThread,ThreadStack);
			if (Length > SystemKernelModuleSize)
			{
				for (i=0;i<ThreadStack->ulCount;i++)
				{
					if (DebugOn)
						KdPrint(("0x%08x %08x %08x %08x %08x\n",
						ThreadStack->StackInfo[i].ulAddress,
						ThreadStack->StackInfo[i].ulStack1,
						ThreadStack->StackInfo[i].ulStack2,
						ThreadStack->StackInfo[i].ulStack3,
						ThreadStack->StackInfo[i].ulStack4));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,ThreadStack,SystemKernelModuleSize);
				Length = SystemKernelModuleSize;
			}
			RExFreePool(ThreadStack);
			ulThread = 0;
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�����ǰ�Ļ����������ģ����ʼ��ntkrnlpaɨ��δ��������
	�����ǰ�Ļ�������������Ĭ��ɨ�赼������

	*/
	if (FileHandle == INIT_PDB_KERNEL_INFO)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		//�Ѿ�ȡ���ˣ�ֱ�ӷ����ˡ�
		if (bKrnlPDBSuccess){
			if (DebugOn)
				KdPrint(("doen success\n"));
			return STATUS_UNSUCCESSFUL;
		}
		if (DebugOn)
			KdPrint(("enter\n"));

		if (MmIsAddressValidEx(Buffer) &&
			SystemKernelModuleSize+1024 > Length &&
			Length > SystemKernelModuleSize)
		{
			PDBNtosFuncAddressInfo = RExAllocatePool(PagedPool,SystemKernelModuleSize+1024);    //�����㹻��Ļ���
			if (PDBNtosFuncAddressInfo == NULL) 
			{
				if (DebugOn)
					KdPrint(("pdb failed\n"));
				return STATUS_UNSUCCESSFUL;
			}
			memset(PDBNtosFuncAddressInfo,0,SystemKernelModuleSize+1024);

			//��ring3�õ��ں���Ϣ
			Rmemcpy(PDBNtosFuncAddressInfo,Buffer,Length);

			if (DebugOn)
				KdPrint(("copy memory\n"));

			for (i=0;i<PDBNtosFuncAddressInfo->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("%ws : 0x%X\n",PDBNtosFuncAddressInfo->NtosFuncInfo[i].FuncName,PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulAddress));

				//�������غ�ĵ�ַ������
				if (wcslen(PDBNtosFuncAddressInfo->NtosFuncInfo[i].FuncName) &&
					MmIsAddressValidEx(PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulAddress)){
					PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulReloadAddress = PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulAddress - SystemKernelModuleBase + (ULONG)ImageModuleBase;
				}else{
					PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulAddress = 0;
					PDBNtosFuncAddressInfo->NtosFuncInfo[i].ulReloadAddress = 0;
				}
			}
			if (DebugOn)
				KdPrint(("copy memory ok\n"));

			if (PDBNtosFuncAddressInfo->ulCount > 188 &&
				MmIsAddressValidEx(PDBNtosFuncAddressInfo->NtosFuncInfo[188].ulAddress)){
				bKrnlPDBSuccess = TRUE;
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ͣ����

	*/
	if (FileHandle == SUSPEND_PROCESS)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (SuspendProcess((PEPROCESS)Length) == STATUS_SUCCESS)
			{
				if (DebugOn)
					KdPrint(("Suspend process:%08x",Length));
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ָ���������

	*/
	if (FileHandle == RESUME_PROCESS)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (ResumeProcess((PEPROCESS)Length) == STATUS_SUCCESS)
			{
				if (DebugOn)
					KdPrint(("ResumeThread process:%08x",Length));
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ͣ�߳�

	*/
	if (FileHandle == SUSPEND_THREAD)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (SuspendThread((PETHREAD)Length) == STATUS_SUCCESS)
			{
				if (DebugOn)
					KdPrint(("Suspend Thread:%08x",Length));
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ָ��߳�����

	*/
	if (FileHandle == RESUME_THREAD)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (ResumeThread((PETHREAD)Length) == STATUS_SUCCESS)
			{
				if (DebugOn)
					KdPrint(("ResumeThread Thread:%08x",Length));
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DLL_FUCK)
	{
		bDisDllFuck = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_DLL_FUCK)
	{
		bDisDllFuck = FALSE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_WINDOWS_HOOK)
	{
		bDisSetWindowsHook = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_SET_WINDOWS_HOOK)
	{
		bDisSetWindowsHook = FALSE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KERNEL_THREAD)
	{
		bDisKernelThread = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_KERNEL_THREAD)
	{
		bDisKernelThread = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	if (FileHandle == RESET_SRV)
	{
		bDisResetSrv = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_RESET_SRV)
	{
		bDisResetSrv = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	if (FileHandle == PROTECT_PROCESS)
	{
		bProtectProcess = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == UNPROTECT_PROCESS)
	{
		bProtectProcess = FALSE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_KILL_SYSTEM_NOTIFY)
	{
		IntNotify = Length;
		return STATUS_UNSUCCESSFUL;
	}
	/*

	���������̵߳�ö�٣�ͨ��Ӳ����ķ�ʽ��λKTHREAD��kernelstack��ջ��

	*/
	if (FileHandle == LIST_WORKQUEUE)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		WorkQueueThread = RExAllocatePool(PagedPool,sizeof(WORKQUEUE)*788);    //�����㹻��Ļ���
		if (WorkQueueThread == NULL) 
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(WorkQueueThread,0,sizeof(WORKQUEUE)*788);
		QueryWorkQueue(WorkQueueThread);
		if (DebugOn)
			KdPrint(("%d Length:%08x :%08x\r\n",WorkQueueThread->ulCount,Length,sizeof(WORKQUEUE)*788));

		if (Length >  sizeof(WORKQUEUE)*788)
		{
			for (i=0;i<WorkQueueThread->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]���������߳�\r\n"
					"EHTREAD��%08X\r\n"
					"���ͣ�%d\r\n"
					"������ڣ�%08X\r\n"
					"�����������ģ�飺%s\r\n",
					i,
					WorkQueueThread->WorkQueueInfo[i].ulEthread,
					WorkQueueThread->WorkQueueInfo[i].ulBasePriority,
					WorkQueueThread->WorkQueueInfo[i].ulWorkerRoutine,
					WorkQueueThread->WorkQueueInfo[i].lpszModule));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,WorkQueueThread,sizeof(WORKQUEUE)*788);
			Length =  sizeof(WORKQUEUE)*788;
		}
		RExFreePool(WorkQueueThread);
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��������

	*/
	if (FileHandle == LIST_START_UP)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		StartupInfo = RExAllocatePool(PagedPool,sizeof(STARTUP_INFO)*788);    //�����㹻��Ļ���
		if (StartupInfo == NULL) 
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(StartupInfo,0,sizeof(STARTUP_INFO)*788);
		QueryStartup(StartupInfo);

		if (DebugOn)
			KdPrint(("Length:%08x :%08x\r\n",Length,sizeof(STARTUP_INFO)*788));

		if (Length >  sizeof(STARTUP_INFO)*788)
		{
			for (i=0;i<StartupInfo->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]������\r\n"
					"���ƣ�%ws\r\n"
					"ע���·����%ws\r\n"
					"��ֵ��%ws\r\n\r\n",
					i,
					StartupInfo->Startup[i].lpwzName,
					StartupInfo->Startup[i].lpwzKeyPath,
					StartupInfo->Startup[i].lpwzKeyValue));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,StartupInfo,sizeof(STARTUP_INFO)*788);
			Length =  sizeof(STARTUP_INFO)*788;
		}
		RExFreePool(StartupInfo);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KILL_SYSTEM_NOTIFY)
	{
		if (DebugOn)
			KdPrint(("Length:0x%08X IntNotify:%d\r\n",Length,IntNotify));

		if (MmIsAddressValidEx(Length)){
			RemoveNotifyRoutine(Length,IntNotify);
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��ϵͳ�ص�

	*/
	if (FileHandle == LIST_SYSTEM_NOTIFY)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		SystemNotify = RExAllocatePool(PagedPool,sizeof(SYSTEM_NOTIFY)*1024);    //�����㹻��Ļ���
		if (SystemNotify == NULL) 
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(SystemNotify,0,sizeof(SYSTEM_NOTIFY)*1024);
		QuerySystemNotify(PDriverObject,SystemNotify);

		if (DebugOn)
			KdPrint(("Length:%08x :%08x\r\n",Length,sizeof(SYSTEM_NOTIFY)*1024));

		if (Length >  sizeof(SYSTEM_NOTIFY)*1024)
		{
			for (i=0;i<SystemNotify->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]�ص�����:%ws\r\n"
					"�ص����:%08X\r\n"
					"����ģ��:%s\r\n"
					"����:%ws\r\n\r\n",
					i,
					SystemNotify->NotifyInfo[i].lpwzType,
					SystemNotify->NotifyInfo[i].ulNotifyBase,
					SystemNotify->NotifyInfo[i].lpszModule,
					SystemNotify->NotifyInfo[i].lpwzObject));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,SystemNotify,sizeof(SYSTEM_NOTIFY)*1024);
			Length =  sizeof(SYSTEM_NOTIFY)*1024;
		}
		RExFreePool(SystemNotify);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KILL_DPC_TIMER)
	{
		if (Length > 0x123456 &&
			MmIsAddressValidEx((PKTIMER)Length))
		{
			if (DebugOn)
				KdPrint(("Timer:0x%08X",Length));

			KillDcpTimer((PKTIMER)Length);
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��DPC��ʱ��

	*/
	if (FileHandle == LIST_DPC_TIMER)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		DpcTimer = RExAllocatePool(PagedPool,sizeof(MyDpcTimer)*MAX_DPCTIMER_COUNT);    //�����㹻��Ļ���
		if (DpcTimer == NULL) 
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(DpcTimer,0,sizeof(MyDpcTimer)*MAX_DPCTIMER_COUNT);
		WinVer = GetWindowsVersion();
		switch (WinVer)
		{
		case WINDOWS_VERSION_2K3_SP1_SP2:
		case WINDOWS_VERSION_XP:
		case WINDOWS_VERSION_7_7000:
			GetDpcTimerInformation_XP_2K3_WIN7000(DpcTimer);
			break;
		case WINDOWS_VERSION_7_7600_UP:
			GetDpcTimerInformation_WIN7600_UP(DpcTimer);
			break;
		}
		
		if (Length >  sizeof(MyDpcTimer)*MAX_DPCTIMER_COUNT)
		{
			for (i=0;i<DpcTimer->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]��ʱ������:%08x\r\n"
					"��������:%d\r\n"
					"�������:%08x\r\n"
					"�����������ģ��:%s\r\n"
					"DPC�ṹ��ַ:%08x\r\n",
					i,
					DpcTimer->MyTimer[i].TimerAddress,
					DpcTimer->MyTimer[i].Period,
					DpcTimer->MyTimer[i].DpcRoutineAddress,
					DpcTimer->MyTimer[i].lpszModule,
					DpcTimer->MyTimer[i].DpcAddress));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,DpcTimer,sizeof(MyDpcTimer)*MAX_DPCTIMER_COUNT);
			Length =  sizeof(MyDpcTimer)*MAX_DPCTIMER_COUNT;
		}
		RExFreePool(DpcTimer);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_KERNEL_FILTER_DRIVER)
	{
		if (Length > 0 &&
			MmIsAddressRangeValid(Buffer,Length))
		{
			memset(lpwzFilter,0,sizeof(lpwzFilter));
			wcsncat(lpwzFilter,Buffer,Length);
			if (DebugOn)
				KdPrint(("lpwzFilter:%ws",lpwzFilter));
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DELETE_KERNEL_FILTER_DRIVER)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			ulDeviceObject = Length;
			if (DebugOn)
				KdPrint(("ulDeviceObject:%08X",ulDeviceObject));

			ClearFilters(lpwzFilter,ulDeviceObject);
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	����ں��߳����ݣ����֮ǰҪ��ͣ�£�������Դ��յ�ͬʱ������ģ����ʾͻ�BSOD
	��ʵҲ��������ѡ���ķ�ʽ���㣬��������ֱ�Ӹ�ֵFALSE��һ�дӼ�

	*/
	if (FileHandle == CLEAR_KERNEL_THREAD)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		bIsInitSuccess = FALSE;   //��ͣ����

		if (KernelThread)
			RExFreePool(KernelThread);

		ThreadCount = 0;
		KernelThread = (PKERNEL_THREAD_INFO)RExAllocatePool(NonPagedPool,sizeof(KERNEL_THREAD_INFO)*256);
		if (!KernelThread)
		{
			if (DebugOn)
				KdPrint(("KernelThread failed"));
			return STATUS_UNSUCCESSFUL;
		}
		memset(KernelThread,0,sizeof(KERNEL_THREAD_INFO)*256);

		bIsInitSuccess = TRUE;   //�ָ�

		return STATUS_UNSUCCESSFUL;
	}
	/*

	�ں��߳���ͨ��Hook PsCreateSystemThread�������̵߳Ĵ��������浽�ṹ����ring3��Ҫ��ʱ��ֱ�Ӵ��ͽṹ����

	*/
	if (FileHandle == LIST_KERNEL_THREAD)
	{
		if (Length >  sizeof(KERNEL_THREAD_INFO)*256)
		{
			if (DebugOn)
				KdPrint(("Length:%08x-%08x",Length,sizeof(KERNEL_THREAD_INFO)*256));
			for (i=0;i<ThreadCount;i++)
			{
				if (MmIsAddressValidEx(KernelThread->KernelThreadInfo[i].ThreadStart))
				{
					if (DebugOn)
						KdPrint(("ThreadStart:%08x",KernelThread->KernelThreadInfo[i].ThreadStart));

					memset(lpszModule,0,sizeof(lpszModule));
					if (!IsAddressInSystem(
						KernelThread->KernelThreadInfo[i].ThreadStart,
						&ulThreadModuleBase,
						&ulThreadModuleSize,
						lpszModule))
					{
						KernelThread->KernelThreadInfo[i].ulHideType = 1;  //�����߳�
					}
					if (DebugOn)
						KdPrint(("Hided:%08x:%s",KernelThread->KernelThreadInfo[i].ThreadStart,lpszModule));
				}else
				{
					KernelThread->KernelThreadInfo[i].ulStatus = 1;   //�߳��˳�
				}
			}
			KernelThread->ulCount = ThreadCount;
			if (DebugOn)
				KdPrint(("ThreadCount:%d",KernelThread->ulCount));

			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,KernelThread,sizeof(KERNEL_THREAD_INFO)*256);
			Length =  sizeof(KERNEL_THREAD_INFO)*256;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_PROCESS_THREAD)
	{
		if (MmIsAddressValidEx(Length))
		{
			TempEProcess = (PEPROCESS)Length;
			KdPrint(("TempEProcess:%08x success",TempEProcess));
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KILL_SYSTEM_THREAD)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (KillThread((PETHREAD)Length))
			{
				if (DebugOn)
					KdPrint(("Kill ETHREAD:%08x success",Length));
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��ϵͳ�߳� or �����߳�

	*/
	if (FileHandle == LIST_SYSTEM_THREAD ||
		FileHandle == LIST_PROCESS_THREAD)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		SystemThread = (PSYSTEM_THREAD_INFO)RExAllocatePool(NonPagedPool,sizeof(SYSTEM_THREAD_INFO)*256);
		if (!SystemThread)
		{
			if (DebugOn)
				KdPrint(("SystemThread failed"));
			return STATUS_UNSUCCESSFUL;
		}
		memset(SystemThread,0,sizeof(SYSTEM_THREAD_INFO)*256);
		if (FileHandle == LIST_PROCESS_THREAD)
		{
			QuerySystemThread(SystemThread,TempEProcess);
		}else
			QuerySystemThread(SystemThread,SystemEProcess);

		if (Length >  sizeof(SYSTEM_THREAD_INFO)*256)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,SystemThread,sizeof(SYSTEM_THREAD_INFO)*256);
			Length =  sizeof(SYSTEM_THREAD_INFO)*256;
		}
		RExFreePool(SystemThread);
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö�ٹ�������

	*/
	if (FileHandle == LIST_KERNEL_FILTER_DRIVER)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		KernelFilterDriver = (PKERNEL_FILTERDRIVER)RExAllocatePool(NonPagedPool,sizeof(KERNEL_FILTERDRIVER)*256);
		if (!KernelFilterDriver)
		{
			if (DebugOn)
				KdPrint(("KernelFilterDriver failed"));
			return STATUS_UNSUCCESSFUL;
		}
		memset(KernelFilterDriver,0,sizeof(KERNEL_FILTERDRIVER)*256);
		if (KernelFilterDriverEnum(KernelFilterDriver) == STATUS_SUCCESS)
		{
			if (DebugOn)
				KdPrint(("KernelFilterDriverEnum STATUS_SUCCESS"));
			if (Length >  sizeof(KERNEL_FILTERDRIVER)*256)
			{
				for (i=0;i<KernelFilterDriver->ulCount;i++)
				{
					if (DebugOn)
						KdPrint(("[%d]��������\r\n"
						"����:%08X\r\n" 
						"����������:%ws\r\n"
						"����·��:%ws\r\n"
						"�豸��ַ:%08X\r\n"
						"��������������:%ws\r\n\r\n",
						i,
						KernelFilterDriver->KernelFilterDriverInfo[i].ulObjType,
						KernelFilterDriver->KernelFilterDriverInfo[i].FileName,
						KernelFilterDriver->KernelFilterDriverInfo[i].FilePath,
						KernelFilterDriver->KernelFilterDriverInfo[i].ulAttachDevice,
						KernelFilterDriver->KernelFilterDriverInfo[i].HostFileName));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,KernelFilterDriver, sizeof(KERNEL_FILTERDRIVER)*256);
				Length =  sizeof(KERNEL_FILTERDRIVER)*256;
			}
		}
		RExFreePool(KernelFilterDriver);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == PROTECT_360SAFE)
	{
		bIsProtect360 = TRUE;
		//Fix360Hook(bIsProtect360);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == UNPROTECT_360SAFE)
	{
		bIsProtect360 = FALSE;
		//Fix360Hook(bIsProtect360);
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��object hook

	*/
	if (FileHandle == LIST_OBJECT_HOOK)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		ObjectHookInfo = (POBJECTHOOKINFO)RExAllocatePool(NonPagedPool, sizeof(OBJECTHOOKINFO)*256);
		if (!ObjectHookInfo)
		{
			//KdPrint(("ObjectHookInfo failed"));
			return STATUS_UNSUCCESSFUL;
		}
		memset(ObjectHookInfo,0, sizeof(OBJECTHOOKINFO)*256);
		IoFileObjectTypeHookInfo(ObjectHookInfo);
		IoDeviceObjectTypeHookInfo(ObjectHookInfo);
		IoDriverObjectTypeHookInfo(ObjectHookInfo);
		CmpKeyObjectTypeHookInfo(ObjectHookInfo);

		if (DebugOn)
			KdPrint(("Length:%08x-ObjectHookInfo:%08x",Length, sizeof(OBJECTHOOKINFO)*256));

		if (Length >  sizeof(OBJECTHOOKINFO)*256)
		{
			for (i=0;i<ObjectHookInfo->ulCount;i++)
			{
				if (DebugOn)
				   KdPrint(("[%d]ObjectHook\r\n"
					"��ǰ������ַ:%08X\r\n"
					"ԭʼ������ַ:%08X\r\n"
					"������:%s\r\n"
					"����ģ��:%s\r\n"
					"ObjectType��ַ:%08X\r\n"
					"hook����:%d\r\n"
					"objectType����:%s\r\n",
					i,
					ObjectHookInfo->ObjectHook[i].ulMemoryHookBase,
					ObjectHookInfo->ObjectHook[i].ulMemoryFunctionBase,
					ObjectHookInfo->ObjectHook[i].lpszFunction,
					ObjectHookInfo->ObjectHook[i].lpszHookModuleImage,
					ObjectHookInfo->ObjectHook[i].ulObjectTypeBase,
					ObjectHookInfo->ObjectHook[i].ulHookType,
					ObjectHookInfo->ObjectHook[i].lpszObjectTypeName
					));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,ObjectHookInfo, sizeof(OBJECTHOOKINFO)*256);
			Length =  sizeof(OBJECTHOOKINFO)*256;
		}
		RExFreePool(ObjectHookInfo);
	}
	if (FileHandle == SET_SHADOWSSDT_INLINE_HOOK)
	{
		//��������
		if (Length > 0 ||
			Length == 0)
		{
			if (IsExitProcess(AttachGuiEProcess))
			{
				ReLoadNtosCALL(&RKeAttachProcess,L"KeAttachProcess",SystemKernelModuleBase,ImageModuleBase);
				ReLoadNtosCALL(&RKeDetachProcess,L"KeDetachProcess",SystemKernelModuleBase,ImageModuleBase);
				if (RKeAttachProcess &&
					RKeDetachProcess)
				{
					RKeAttachProcess(AttachGuiEProcess);
					RestoreShadowInlineHook(Length);
					RKeDetachProcess();
				}
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_ONE_SHADOWSSDT)
	{
		//��������
		if (Length > 0 ||
			Length == 0)
		{
			if (IsExitProcess(AttachGuiEProcess))
			{
				ReLoadNtosCALL(&RKeAttachProcess,L"KeAttachProcess",SystemKernelModuleBase,ImageModuleBase);
				ReLoadNtosCALL(&RKeDetachProcess,L"KeDetachProcess",SystemKernelModuleBase,ImageModuleBase);
				if (RKeAttachProcess &&
					RKeDetachProcess)
				{
					RKeAttachProcess(AttachGuiEProcess);
					RestoreAllShadowSSDTFunction(Length);
					RKeDetachProcess();
				}
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_ALL_SHADOWSSDT)
	{
		if (IsExitProcess(AttachGuiEProcess))
		{
			ReLoadNtosCALL(&RKeAttachProcess,L"KeAttachProcess",SystemKernelModuleBase,ImageModuleBase);
			ReLoadNtosCALL(&RKeDetachProcess,L"KeDetachProcess",SystemKernelModuleBase,ImageModuleBase);
			if (RKeAttachProcess &&
				RKeDetachProcess)
			{
				RKeAttachProcess(AttachGuiEProcess);
				RestoreAllShadowSSDTFunction(8888);
				RKeDetachProcess();
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��ShadowSSDT

	*/
	if (FileHandle == LIST_SHADOWSSDT ||
		FileHandle == LIST_SHADOWSSDT_ALL)
	{
		if (FileHandle == LIST_SHADOWSSDT_ALL)
		{
			//KdPrint(("Print SSDT All"));
			bShadowSSDTAll = TRUE;
		}
		if (FileHandle == LIST_SHADOWSSDT)
		{
			//KdPrint(("Print SSDT"));
			bShadowSSDTAll = FALSE;
		}
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		ShadowSSDTInfo = (PSHADOWSSDTINFO)RExAllocatePool(NonPagedPool,sizeof(SHADOWSSDTINFO)*900);
		if (!ShadowSSDTInfo)
		{
			if (DebugOn)
				KdPrint(("ShadowSSDTInfo failed:%08x\r\n",sizeof(SHADOWSSDTINFO)*900));
			return STATUS_UNSUCCESSFUL;
		}
		memset(ShadowSSDTInfo,0,sizeof(SHADOWSSDTINFO)*900);
		if (IsExitProcess(AttachGuiEProcess))
		{
			ReLoadNtosCALL(&RKeAttachProcess,L"KeAttachProcess",SystemKernelModuleBase,ImageModuleBase);
			ReLoadNtosCALL(&RKeDetachProcess,L"KeDetachProcess",SystemKernelModuleBase,ImageModuleBase);
			if (RKeAttachProcess &&
				RKeDetachProcess)
			{
				RKeAttachProcess(AttachGuiEProcess);
				ShadowSSDTHookCheck(ShadowSSDTInfo);
				RKeDetachProcess();

				if (DebugOn)
					KdPrint(("Length:%08x-ShadowSSDTInfo:%08x\r\n",Length,sizeof(SHADOWSSDTINFO)*900));

				if (Length > sizeof(SHADOWSSDTINFO)*900)
				{
					for (i=0;i<ShadowSSDTInfo->ulCount;i++)
					{
						if (DebugOn)
							KdPrint(("[%d]����ShadowSSDT hook\r\n"
							"�����:%d\r\n"
							"��ǰ��ַ:%08x\r\n"
							"��������:%s\r\n"
							"��ǰhookģ��:%s\r\n"
							"��ǰģ���ַ:%08x\r\n"
							"��ǰģ���С:%d KB\r\n"
							"Hook����:%d\r\n\r\n",
							i,
							ShadowSSDTInfo->SSDT[i].ulNumber,
							ShadowSSDTInfo->SSDT[i].ulMemoryFunctionBase,
							ShadowSSDTInfo->SSDT[i].lpszFunction,
							ShadowSSDTInfo->SSDT[i].lpszHookModuleImage,
							ShadowSSDTInfo->SSDT[i].ulHookModuleBase,
							ShadowSSDTInfo->SSDT[i].ulHookModuleSize/1024,
							ShadowSSDTInfo->SSDT[i].IntHookType));
					}
					status = OldZwReadFile(
						FileHandle,
						Event,
						ApcRoutine,
						ApcContext,
						IoStatusBlock,
						Buffer,
						Length,
						ByteOffset,
						Key
						);
					Rmemcpy(Buffer,ShadowSSDTInfo,sizeof(SHADOWSSDTINFO)*900);
					Length = sizeof(SHADOWSSDTINFO)*900;
				}
			}
		}
		bShadowSSDTAll = FALSE;
		RExFreePool(ShadowSSDTInfo);
		return STATUS_UNSUCCESSFUL;
	}
	//ǿ������
	if (FileHandle == SHUT_DOWN_SYSTEM)
	{
		KeBugCheck(POWER_FAILURE_SIMULATE);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LOAD_DRIVER)
	{
		if (bDisLoadDriver == FALSE)
		{
			bDisLoadDriver = TRUE;
			EnableDriverLoading();   //�����������
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_LOAD_DRIVER)
	{
		if (bDisLoadDriver == TRUE)
		{
			bDisLoadDriver = FALSE;
			DisEnableDriverLoading();    //��ֹ��������
		}
		return STATUS_UNSUCCESSFUL;
	}
	//---------------------------------------------
	if (FileHandle == WRITE_FILE)
	{
		bDisWriteFile = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_WRITE_FILE)
	{
		bDisWriteFile = FALSE;
		return STATUS_UNSUCCESSFUL;
	}
	//----------------------------------------------------
	if (FileHandle == CREATE_PROCESS)
	{
		bDisCreateProcess = TRUE;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == DIS_CREATE_PROCESS)
	{
		bDisCreateProcess = FALSE;
		return STATUS_UNSUCCESSFUL;
	}
	/*

	DUMP�ں�ģ�鵽�ļ�

	*/
	if (FileHandle == DUMP_KERNEL_MODULE_MEMORY)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		if (Buffer != NULL &&
			Length > 0)
		{
			//KdPrint(("savefile:%ws",Buffer));

			KernelBuffer = RExAllocatePool(NonPagedPool,ulDumpKernelSize+0x100); //����һ�����ڴ�
			if (KernelBuffer)
			{
				memset(KernelBuffer,0,ulDumpKernelSize);
				if (MmIsAddressValidEx(ulDumpKernelBase))
				{
					if (DumpMemory((PVOID)ulDumpKernelBase,KernelBuffer,ulDumpKernelSize) == STATUS_SUCCESS)
					{
						DebugWriteToFile(Buffer,KernelBuffer,ulDumpKernelSize);

						if (DebugOn)
							KdPrint(("DumpKernel success"));
					}
				}
				RExFreePool(KernelBuffer);
			}
		}
	}
	//size
	if (FileHandle == INIT_DUMP_KERNEL_MODULE_MEMORY_1)
	{
		if (Length > 0x10 &&
			Length < 0xfffffff)
		{
			ulDumpKernelSize = Length;

			if (DebugOn)
				KdPrint(("ulDumpKernelBase:%08x\nulDumpKernelSize:%x",ulDumpKernelBase,ulDumpKernelSize));
		}
		return STATUS_UNSUCCESSFUL;
	}
	//Base
	if (FileHandle == INIT_DUMP_KERNEL_MODULE_MEMORY)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			ulDumpKernelBase = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��շ�����־�����֮ǰ��ͣ��������Դ�������̷߳���

	*/
	if (FileHandle == CLEAR_LIST_LOG)
	{
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		bIsInitSuccess = FALSE;   // ��ͣ����

		if (LogDefenseInfo)
			RExFreePool(LogDefenseInfo);

		ulLogCount = 0;
		LogDefenseInfo = (PLOGDEFENSE)RExAllocatePool(NonPagedPool,sizeof(LOGDEFENSE)*1024);
		if (!LogDefenseInfo)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(LogDefenseInfo,0,sizeof(LOGDEFENSE)*1024);

		bIsInitSuccess = TRUE;   //�ָ�

		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ring3��Ҫ��ʱ��ֱ�Ӵ��ͽṹ

	*/
	if (FileHandle == LIST_LOG)
	{
		WinVer = GetWindowsVersion();
		switch(WinVer)
		{
		case WINDOWS_VERSION_XP:
			g_Offset_Eprocess_ProcessId = 0x84;
			break;
		case WINDOWS_VERSION_7_7000:
		case WINDOWS_VERSION_7_7600_UP:
			g_Offset_Eprocess_ProcessId = 0xb4;
			break;
		case WINDOWS_VERSION_2K3_SP1_SP2:
			g_Offset_Eprocess_ProcessId = 0x94;
			break;
		}
		
		if (DebugOn)
			KdPrint(("Length:%x %x",Length,sizeof(LOGDEFENSE)*1024));

		if (Length > sizeof(LOGDEFENSE)*1024)
		{
			__try
			{
				for (i=0;i<ulLogCount;i++)
				{
					if (LogDefenseInfo->LogDefense[i].ulPID)
					{
						//����LogDefenseInfo->LogDefense[i].EProcess�������Ļ�ַ�����е�������
						if (LogDefenseInfo->LogDefense[i].Type == 6)
						{
							if (!MmIsAddressValidEx(LogDefenseInfo->LogDefense[i].EProcess)){
								LogDefenseInfo->LogDefense[i].EProcess = 0;
							}
							LogDefenseInfo->ulCount = ulLogCount;
							continue;
						}
						if (!IsExitProcess(LogDefenseInfo->LogDefense[i].EProcess)){
							memset(LogDefenseInfo->LogDefense[i].lpszProName,0,sizeof(LogDefenseInfo->LogDefense[i].lpszProName));
							strcat(LogDefenseInfo->LogDefense[i].lpszProName,"Unknown");
							LogDefenseInfo->LogDefense[i].ulPID = 0;

						}else{
							memset(LogDefenseInfo->LogDefense[i].lpszProName,0,sizeof(LogDefenseInfo->LogDefense[i].lpszProName));
							lpszProName = (CHAR *)PsGetProcessImageFileName((PEPROCESS)LogDefenseInfo->LogDefense[i].EProcess);
							if (lpszProName){
								strcat(LogDefenseInfo->LogDefense[i].lpszProName,lpszProName);
							}
							lpszProName = NULL;
							LogDefenseInfo->LogDefense[i].ulInheritedFromProcessId = GetInheritedProcessPid((PEPROCESS)LogDefenseInfo->LogDefense[i].EProcess);
						}
					    LogDefenseInfo->ulCount = ulLogCount;

					}
				}
				if (DebugOn)
					KdPrint(("ulLogCount:%d",LogDefenseInfo->ulCount));

			}__except(EXCEPTION_EXECUTE_HANDLER){
				if (DebugOn)
					KdPrint(("[EXCEPTION_EXECUTE_HANDLER]ulLogCount:%d,%d:%ws",LogDefenseInfo->ulCount,ulLogCount,LogDefenseInfo->LogDefense[i].lpwzCreateProcess));

				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,LogDefenseInfo,sizeof(LOGDEFENSE)*1024);
				Length = sizeof(LOGDEFENSE)*1024;
				return STATUS_UNSUCCESSFUL;
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,LogDefenseInfo,sizeof(LOGDEFENSE)*1024);
			Length = sizeof(LOGDEFENSE)*1024;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == CHANG_SERVICES_TYPE_1 ||
		FileHandle == CHANG_SERVICES_TYPE_2 ||
		FileHandle == CHANG_SERVICES_TYPE_3)
	{
		if (MmIsAddressRangeValid(Buffer,Length) &&
			Length > 0)
		{
			memset(lpwzKey,0,sizeof(lpwzKey));
			wcscat(lpwzKey,L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\");
			wcscat(lpwzKey,Buffer);

			switch ((ULONG)FileHandle)
			{
			case CHANG_SERVICES_TYPE_1:  //�ֶ�
				Safe_CreateValueKey(lpwzKey,REG_DWORD,L"Start",0x3);
				break;
			case CHANG_SERVICES_TYPE_2:  //�Զ�
				Safe_CreateValueKey(lpwzKey,REG_DWORD,L"Start",0x2);
				break;
			case CHANG_SERVICES_TYPE_3:  //����
				Safe_CreateValueKey(lpwzKey,REG_DWORD,L"Start",0x4);
				break;
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ȷ���ɨ�裬��ʵ����ע��һ��������������Ȼ����ע���ոճ�ʼ���õ�ʱ��ö��ע���
	��Ϊ�����������磬��ľ��û�����������ԾͿ���ö�ٵ�ľ���ע���

	*/
	if (FileHandle == LIST_DEPTH_SERVICES)
	{
		if (DepthServicesRegistry == NULL)
		{
			if (DebugOn)
				KdPrint(("DepthServicesRegistry is NULL"));
			return STATUS_UNSUCCESSFUL;
		}
		if (!MmIsAddressValidEx(DepthServicesRegistry))
		{
			if (DebugOn)
				KdPrint(("MmIsAddressValidEx!!!"));
			return STATUS_UNSUCCESSFUL;
		}
		
		if (DebugOn)
			KdPrint(("Length:%08x--DepthServicesRegistry:%08x",Length,sizeof(SERVICESREGISTRY)*1024));

		if (Length > sizeof(SERVICESREGISTRY)*1024)
		{
			if (DepthServicesRegistry->ulCount)
			{
				for (i=0;i<DepthServicesRegistry->ulCount;i++)
				{
					if (DebugOn)
						KdPrint(("[%d]��ȷ���鿴\r\n"
						"������:%ws\r\n"
						"ӳ��·��:%ws\r\n"
						"��̬���ӿ�:%ws\r\n\r\n",
						i,
						DepthServicesRegistry->SrvReg[i].lpwzSrvName,
						DepthServicesRegistry->SrvReg[i].lpwzImageName,
						DepthServicesRegistry->SrvReg[i].lpwzDLLPath
						));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,DepthServicesRegistry,sizeof(SERVICESREGISTRY)*1024);
				Length = sizeof(SERVICESREGISTRY)*1024;
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ͨ��ʽ�ķ���ö��

	*/
	if (FileHandle == LIST_SERVICES)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		ServicesRegistry = (PSERVICESREGISTRY)RExAllocatePool(NonPagedPool,sizeof(SERVICESREGISTRY)*1024);
		if (!ServicesRegistry)
		{
			if (DebugOn)
				KdPrint(("RExAllocatePool !!"));
			return STATUS_UNSUCCESSFUL;
		}
		if (DebugOn)
			KdPrint(("search !!"));
		memset(ServicesRegistry,0,sizeof(SERVICESREGISTRY)*1024);
		if (QueryServicesRegistry(ServicesRegistry) == STATUS_SUCCESS)
		{
			if (DebugOn)
				KdPrint(("Length:%08x-ServicesRegistry:%08x",Length,sizeof(SERVICESREGISTRY)*1024));
			if (Length > sizeof(SERVICESREGISTRY)*1024)
			{
				if (DebugOn)
					KdPrint(("Length !!"));

				for (i=0;i<ServicesRegistry->ulCount;i++)
				{
					if (DepthServicesRegistry)
					{
						for (x=0;x<DepthServicesRegistry->ulCount;x++)
						{
							bIsNormalServices = FALSE;

							if (_wcsnicmp(ServicesRegistry->SrvReg[i].lpwzSrvName,DepthServicesRegistry->SrvReg[x].lpwzSrvName,wcslen(DepthServicesRegistry->SrvReg[x].lpwzSrvName)) == 0)
							{
								bIsNormalServices = TRUE;
								break;
							}
						}
						//��������
						if (!bIsNormalServices)
						{
							wcscat(ServicesRegistry->SrvReg[i].lpwzSrvName,L"(���´���)");
						}
					}
					if (DebugOn)
						KdPrint(("[%d]����鿴\r\n"
						"������:%ws\r\n"
						"ӳ��·��:%ws\r\n"
						"��̬���ӿ�:%ws\r\n\r\n",
						i,
						ServicesRegistry->SrvReg[i].lpwzSrvName,
						ServicesRegistry->SrvReg[i].lpwzImageName,
						ServicesRegistry->SrvReg[i].lpwzDLLPath
						));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,ServicesRegistry,sizeof(SERVICESREGISTRY)*1024);
				Length = sizeof(SERVICESREGISTRY)*1024;
			}
		}
		RExFreePool(ServicesRegistry);
		return STATUS_UNSUCCESSFUL;
	}
// 	//��ͣ����
	if (FileHandle == SUSPEND_PROTECT)
	{
		bIsInitSuccess = FALSE;   //�ָ����
		return STATUS_UNSUCCESSFUL;
	}
	//�ָ�����
	if (FileHandle == RESUME_PROTECT)
	{
		bIsInitSuccess = TRUE;   //�ָ����
		return STATUS_UNSUCCESSFUL;
	}
	//��㸳ֵ���ý���˳��exit~��
	if (FileHandle == EXIT_PROCESS)
	{
		bIsInitSuccess = FALSE;   //�ָ����
		ProtectEProcess = 0x12345678;

		return STATUS_UNSUCCESSFUL;
	}
	/*

	��ȡ����ģ��

	*/
	if (FileHandle == LIST_SYS_MODULE)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		SysModuleInfo = (PSYSINFO)RExAllocatePool(NonPagedPool,sizeof(SYSINFO)*260);
		if (!SysModuleInfo)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(SysModuleInfo,0,sizeof(SYSINFO)*260);
		EnumKernelModule(PDriverObject,SysModuleInfo);
		if (Length > sizeof(SYSINFO)*260)
		{
			for (i=0;i<SysModuleInfo->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]SysModule\r\n"
						"����:%08x\r\n"
						"��ַ:%08x\r\n"
						"��С:%x\r\n"
						"������:%ws\r\n"
						"����·��:%ws\r\n"
						"����:%ws\r\n"
						"��������:%d\r\n",
						i,
						SysModuleInfo->SysInfo[i].DriverObject,
						SysModuleInfo->SysInfo[i].ulSysBase,
						SysModuleInfo->SysInfo[i].SizeOfImage,
						SysModuleInfo->SysInfo[i].lpwzBaseSysName,
						SysModuleInfo->SysInfo[i].lpwzFullSysName,
						SysModuleInfo->SysInfo[i].lpwzServiceName,
						SysModuleInfo->SysInfo[i].IntHideType
						));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,SysModuleInfo,sizeof(SYSINFO)*260);
			Length = sizeof(SYSINFO)*260;
		}
		RExFreePool(SysModuleInfo);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == NO_KERNEL_SAFE_MODULE)
	{
		bKernelSafeModule = FALSE;  //�ر��ں˰�ȫģʽ
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KERNEL_SAFE_MODULE)
	{
		bKernelSafeModule = TRUE;  //�����ں˰�ȫģʽ����ϵͳ�������κε�hook
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_EAT_HOOK)
	{
		ulInitRealModuleBase = Length;

		KdPrint(("num:%d module:%x func:%x\r\n",ulNumber,ulInitRealModuleBase,ulInitRealFuncBase));

		if (MmIsAddressValidEx(ulInitRealModuleBase) &&
			MmIsAddressValidEx(ulInitRealFuncBase))
		{
			KdPrint(("111num:%d module:%x func:%x\r\n",ulNumber,ulInitRealModuleBase,ulInitRealFuncBase));
			ReSetEatHook(ulNumber,ulInitRealModuleBase,ulInitRealFuncBase);
			ulNumber = 0;
			ulInitRealModuleBase = 0;
			ulInitRealFuncBase = 0;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_EAT_REAL_ADDRESS)
	{
		ulInitRealFuncBase = Length;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_EAT_NUMBER)
	{
		ulNumber = Length;
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == ANTI_INLINEHOOK)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0)
		{
			AntiInlineHook(Length,SystemKernelModuleBase,ImageModuleBase);
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��ntkrnlpa��inline hook�������������ط���ɨ��δ������������֮ɨ�赼��������

	*/
	if (FileHandle == LIST_INLINEHOOK)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		InlineHookInfo = (PINLINEHOOKINFO)RExAllocatePool(NonPagedPool,sizeof(INLINEHOOKINFO)*256);
		if (!InlineHookInfo)
		{
			if (DebugOn)
				KdPrint(("InlineHookInfo failed\r\n"));
			return STATUS_UNSUCCESSFUL;
		}
		memset(InlineHookInfo,0,sizeof(INLINEHOOKINFO)*256);
		KernelHookCheck(InlineHookInfo,NtosModule);

		//KdPrint(("%08x---%08x\r\n",Length,sizeof(INLINEHOOKINFO)*256));
		if (Length > sizeof(INLINEHOOKINFO)*256)
		{
			for (i=0;i<InlineHookInfo->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]KernelHook\r\n"
					"���ҹ���ַ:%08x\r\n"
					"ԭʼ��ַ:%08x\r\n"
					"�ҹ�����:%s\r\n"
					"hook��ת��ַ:%08x\r\n"
					"����ģ��:%s\r\n"
					"ģ���ַ:%08x\r\n"
					"ģ���С:%x\r\n",
					i,
					InlineHookInfo->InlineHook[i].ulMemoryFunctionBase,
					InlineHookInfo->InlineHook[i].ulRealFunctionBase,
					InlineHookInfo->InlineHook[i].lpszFunction,
					InlineHookInfo->InlineHook[i].ulMemoryHookBase,
					InlineHookInfo->InlineHook[i].lpszHookModuleImage,
					InlineHookInfo->InlineHook[i].ulHookModuleBase,
					InlineHookInfo->InlineHook[i].ulHookModuleSize
					));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,InlineHookInfo,sizeof(INLINEHOOKINFO)*256);
			Length = sizeof(INLINEHOOKINFO)*256;
		}
		RExFreePool(InlineHookInfo);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_PROCESS_LIST_PROCESS_MODULE)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			ulInitEProcess = Length;  //��ʼ��
			if (DebugOn)
				KdPrint(("InitEprocess:%08x\n",ulInitEProcess));
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö�ٽ���DLLģ��

	*/
	if (FileHandle == LIST_PROCESS_MODULE)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		if (!MmIsAddressValidEx(ulInitEProcess))
		{
			return STATUS_UNSUCCESSFUL;
		}
		PDll = (PDLLINFO)RExAllocatePool(NonPagedPool,sizeof(DLLINFO)*512);
		if (!PDll)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(PDll,0,sizeof(DLLINFO)*512);
		
		EunmProcessModule(ulInitEProcess,PDll);

		ulInitEProcess = NULL;  //�ָ�ΪNULL
		if (Length > sizeof(DLLINFO)*512)
		{
			for (i=0;i<PDll->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]Dllģ��\r\n"
					"Path:%ws\r\n"
					"Base:%08X\r\n\r\n",
					i,
					PDll->DllInfo[i].lpwzDllModule,
					PDll->DllInfo[i].ulBase
					));
			}
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,PDll,sizeof(DLLINFO)*512);
			Length = sizeof(DLLINFO)*512;
		}
		RExFreePool(PDll);
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö�ٽ���ģ��

	*/
	if (FileHandle == LIST_PROCESS)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		NormalProcessInfo = (PPROCESSINFO)RExAllocatePool(NonPagedPool,sizeof(PROCESSINFO)*256);
		if (!NormalProcessInfo)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(NormalProcessInfo,0,sizeof(PROCESSINFO)*256);

		bPaused = TRUE;  //��ͣ�¶�ȡ�ڴ棬������ͬ������

		GetNormalProcessList(NormalProcessInfo,HideProcessInfo);
		if (DebugOn)
			KdPrint(("Length:%08x-NormalProcessInfo:%08x",Length,sizeof(PROCESSINFO)*256));
		if (Length > sizeof(PROCESSINFO)*256)
		{
			for (i=0;i<NormalProcessInfo->ulCount;i++)
			{
				if (DebugOn)
					KdPrint(("[%d]���̲鿴\r\n"
						"����״̬:%d\r\n"
						"pid:%d\r\n"
						"������:%d\r\n"
						"�ں˷���״̬:%d\r\n"
						"PEPROCESS:%08x\r\n"
						"����ȫ·��:%ws\r\n\r\n",
						i,
						NormalProcessInfo->ProcessInfo[i].IntHideType,
						NormalProcessInfo->ProcessInfo[i].ulPid,
						NormalProcessInfo->ProcessInfo[i].ulInheritedFromProcessId,
						NormalProcessInfo->ProcessInfo[i].ulKernelOpen,
						NormalProcessInfo->ProcessInfo[i].EProcess,
						NormalProcessInfo->ProcessInfo[i].lpwzFullProcessPath
						));
			}
			status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,NormalProcessInfo,sizeof(PROCESSINFO)*256);
				Length = sizeof(PROCESSINFO)*256;
		}
		RExFreePool(NormalProcessInfo);

		//�鿴���󣬾�Ҫ����һ��
		//��ΪbPaused���ƣ����ԾͲ�����ͬ������
		//��ʵҲ��������ѡ��

		memset(HideProcessInfo,0,(sizeof(PROCESSINFO)+sizeof(SAFESYSTEM_PROCESS_INFORMATION))*120);

		bPaused = FALSE;   //�ָ����ؽ��̵Ķ�ȡ

		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == KILL_PROCESS_BY_PID)
	{
		//��������
		if (Length > 0)
		{
			bKernelSafeModule = TRUE;

			KillPro(Length);

			bKernelSafeModule = FALSE;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_SET_ATAPI_HOOK)
	{
		if (Length >= NULL && Length <= 0x1c)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_ATAPI_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= 0x1c)
			{
				SetAtapiHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_ATAPI_HOOK)
	{
		//��ʼ���
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		AtapiDispatchBakUp = (PATAPIDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(ATAPIDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!AtapiDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(AtapiDispatchBakUp,0,sizeof(ATAPIDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);

		ReLoadAtapi(PDriverObject,AtapiDispatchBakUp,1);  //kbdclass hook

		if (Length > sizeof(ATAPIDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,AtapiDispatchBakUp,sizeof(ATAPIDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(ATAPIDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		RExFreePool(AtapiDispatchBakUp);

		return STATUS_UNSUCCESSFUL;
	}
	/////////////////
	if (FileHandle == INIT_SET_MOUCLASS_HOOK)
	{
		if (Length >= NULL && Length <= IRP_MJ_MAXIMUM_FUNCTION)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_MOUCLASS_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= IRP_MJ_MAXIMUM_FUNCTION)
			{
				SetMouclassHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_MOUCLASS_HOOK)
	{
		//��ʼ���
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		MouclassDispatchBakUp = (PMOUCLASSDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(MOUCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!MouclassDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(MouclassDispatchBakUp,0,sizeof(MOUCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);

		ReLoadMouclass(PDriverObject,MouclassDispatchBakUp,1);  //kbdclass hook

		if (Length > sizeof(MOUCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,MouclassDispatchBakUp,sizeof(MOUCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(MOUCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		RExFreePool(MouclassDispatchBakUp);

		return STATUS_UNSUCCESSFUL;
	}
	///////
	if (FileHandle == INIT_SET_KBDCLASS_HOOK)
	{
		if (Length >= NULL && Length <= IRP_MJ_MAXIMUM_FUNCTION)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_KBDCLASS_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= IRP_MJ_MAXIMUM_FUNCTION)
			{
				SetKbdclassHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_KBDCLASS_HOOK)
	{
		//��ʼ���
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		KbdclassDispatchBakUp = (PKBDCLASSDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(KBDCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!KbdclassDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(KbdclassDispatchBakUp,0,sizeof(KBDCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);

		ReLoadKbdclass(PDriverObject,KbdclassDispatchBakUp,1);  //kbdclass hook

		if (Length > sizeof(KBDCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,KbdclassDispatchBakUp,sizeof(KBDCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(KBDCLASSDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		RExFreePool(KbdclassDispatchBakUp);

		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_SET_FSD_HOOK)
	{
		if (Length >= NULL && Length <= IRP_MJ_MAXIMUM_FUNCTION)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_FSD_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= IRP_MJ_MAXIMUM_FUNCTION)
			{
				SetFsdHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_FSD_HOOK)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		NtfsDispatchBakUp = (PNTFSDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(NTFSDISPATCHBAKU)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!NtfsDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(NtfsDispatchBakUp,0,sizeof(NTFSDISPATCHBAKU)*IRP_MJ_MAXIMUM_FUNCTION);

		ReLoadNtfs(PDriverObject,NtfsDispatchBakUp,1);  //fsd hook

		if (Length > sizeof(NTFSDISPATCHBAKU)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,NtfsDispatchBakUp,sizeof(NTFSDISPATCHBAKU)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(NTFSDISPATCHBAKU)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		RExFreePool(NtfsDispatchBakUp);

		return STATUS_UNSUCCESSFUL;
	}
	//ONLY_DELETE_FILE��ɾ��360�ļ�����˲���Ҫreload
	if (FileHandle == DELETE_FILE ||
		FileHandle == ONLY_DELETE_FILE)
	{
		if (MmIsAddressValidEx(Buffer) &&
			Length > 0)
		{
			bKernelSafeModule = TRUE;

			if (FileHandle == DELETE_FILE)
			{
				ReLoadNtfs(PDriverObject,0,0);  //reload ntfs �ָ���ʵ��ַ
				ReLoadAtapi(PDriverObject,0,0); //reload atapi 
			}
			if (IsFileInSystem(Buffer))
			{
				HFileHandle = SkillIoOpenFile(
					Buffer,   //ɾ��dll�ļ�
					FILE_READ_ATTRIBUTES,
					FILE_SHARE_DELETE);
				if (HFileHandle!=NULL)
				{
					SKillDeleteFile(HFileHandle);
					ZwClose(HFileHandle);
				}
			}
			if (FileHandle == DELETE_FILE)
			{
				ReLoadNtfsFree();  //�ָ�
				ReLoadAtapiFree();
			}
			bKernelSafeModule = FALSE;
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_SET_TCPIP_HOOK)
	{
		if (Length >= NULL && Length <= IRP_MJ_MAXIMUM_FUNCTION)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_TCPIP_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= IRP_MJ_MAXIMUM_FUNCTION)
			{
				SetTcpHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_TCPIP_HOOK)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		TcpDispatchBakUp = (PTCPDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(TCPDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!TcpDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(TcpDispatchBakUp,0,sizeof(TCPDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);

		ReLoadTcpip(PDriverObject,TcpDispatchBakUp,1);
		if (Length > sizeof(TCPDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,TcpDispatchBakUp,sizeof(TCPDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(TCPDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		ReLoadTcpipFree(); //�ͷ�

		RExFreePool(TcpDispatchBakUp);

		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == INIT_SET_NSIPROXY_HOOK)
	{
		if (Length >= NULL && Length <= IRP_MJ_MAXIMUM_FUNCTION)
		{
			ulNumber = Length;  //��ʼ��
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_NSIPROXY_HOOK)
	{
		if (Length > 0x123456)
		{
			ulRealDispatch = Length;

			if (DebugOn)
				KdPrint(("Init ulRealDispatch:[%d]%X\n",ulNumber,ulRealDispatch));

			if (ulNumber >= NULL && ulNumber <= IRP_MJ_MAXIMUM_FUNCTION)
			{
				SetNsiproxyHook(ulNumber,ulRealDispatch);
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_NSIPROXY_HOOK)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		NsiproxyDispatchBakUp = (PNSIPROXYDISPATCHBAKUP)RExAllocatePool(NonPagedPool,sizeof(NSIPROXYDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		if (!NsiproxyDispatchBakUp)
		{
			return STATUS_UNSUCCESSFUL;
		}
		memset(NsiproxyDispatchBakUp,0,sizeof(NSIPROXYDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
		ReLoadNsiproxy(PDriverObject,NsiproxyDispatchBakUp,1);

		if (Length > sizeof(NSIPROXYDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION)
		{
			status = OldZwReadFile(
				FileHandle,
				Event,
				ApcRoutine,
				ApcContext,
				IoStatusBlock,
				Buffer,
				Length,
				ByteOffset,
				Key
				);
			Rmemcpy(Buffer,NsiproxyDispatchBakUp,sizeof(NSIPROXYDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION);
			Length = sizeof(NSIPROXYDISPATCHBAKUP)*IRP_MJ_MAXIMUM_FUNCTION;
		}
		RExFreePool(NsiproxyDispatchBakUp);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == LIST_TCPUDP)
	{
		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

		ReLoadTcpip(PDriverObject,0,0);  //��reload
		ReLoadNsiproxy(PDriverObject,0,0);  //��reload

		TCPUDPInfo = (PTCPUDPINFO)RExAllocatePool(NonPagedPool,sizeof(TCPUDPINFO)*256);
		if (TCPUDPInfo)
		{
			memset(TCPUDPInfo,0,sizeof(TCPUDPINFO)*256);

			WinVer = GetWindowsVersion();
			if (WinVer == WINDOWS_VERSION_XP ||
				WinVer == WINDOWS_VERSION_2K3_SP1_SP2)
			{
				PrintTcpIp(TCPUDPInfo);
			}
			else if (WinVer == WINDOWS_VERSION_7_7000 || 
				     WinVer == WINDOWS_VERSION_7_7600_UP)
			{
				PrintTcpIpInWin7(TCPUDPInfo);
			}
			if (Length > sizeof(TCPUDPINFO)*256)
			{
				for (i = 0; i<TCPUDPInfo->ulCount ;i++)
				{
					if (DebugOn)
						KdPrint(("[%d]��������\r\n"
						"Э��:%d\r\n"
						"����״̬:%d\r\n"
						"���ص�ַ:%08x\r\n"
						"���ض˿�:%d\r\n"
						"����pid:%d\r\n"
						"����·��:%ws\r\n"
						"Զ�̵�ַ:%08x\r\n"
						"Զ�̶˿�:%d\r\n\r\n",
						i,
						TCPUDPInfo->TCPUDP[i].ulType,
						TCPUDPInfo->TCPUDP[i].ulConnectType,
						TCPUDPInfo->TCPUDP[i].ulLocalAddress,
						TCPUDPInfo->TCPUDP[i].ulLocalPort,
						TCPUDPInfo->TCPUDP[i].ulPid,
						TCPUDPInfo->TCPUDP[i].lpwzFullPath,
						TCPUDPInfo->TCPUDP[i].ulRemoteAddress,
						TCPUDPInfo->TCPUDP[i].ulRemotePort));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,TCPUDPInfo,sizeof(TCPUDPINFO)*256);
				Length = sizeof(TCPUDPINFO)*256;
			}
			RExFreePool(TCPUDPInfo);
		}
		ReLoadTcpipFree(); //�ͷ�
		ReLoadNsiproxyFree(); //�ͷ�

		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_INLINE_HOOK)
	{
		if (MmIsAddressValidEx(Length) &&
			Length > 0x123456)
		{
			if (DebugOn)
				KdPrint(("Set Inline hook:%08x\n",Length));

			RestoreInlineHook(Length,SystemKernelModuleBase,ImageModuleBase);
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_ONE_SSDT)
	{
		//��������
		if (Length > 0 ||
			Length == 0)
		{
			RestoreAllSSDTFunction(Length);
		}
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == SET_ALL_SSDT)
	{
		RestoreAllSSDTFunction(8888);
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == UNPROTECT_DRIVER_FILE)
	{
		bProtectDriverFile = FALSE;   //ȡ������
		KdPrint(("UnProtect Driver File\r\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (FileHandle == PROTECT_DRIVER_FILE)
	{
		bProtectDriverFile = TRUE;     //����
		KdPrint(("Protect Driver File\r\n"));
		return STATUS_UNSUCCESSFUL;
	}
	/*

	�������ܹؼ�����������A�ܵ����ֻҪд��һ��Ϳ��Ա���ð��

	*/
	if (FileHandle == SAFE_SYSTEM)
	{
		if (DebugOn)
			KdPrint(("ProtectCode:%08x\r\n",SAFE_SYSTEM));

		if (Length == 8 &&
			MmIsAddressValidEx(Buffer))
		{
			if (DebugOn)
				KdPrint(("Buffer:%s %d\r\n",Buffer,Length));

			if (_strnicmp(Buffer,"Safe",4) ==0)
			{
				//��֤Caller�Ĵ�С
// 				if (GetCallerFileSize(RPsGetCurrentProcess()) != ulCallerFileSize){
// 					//�ļ������С���ԣ������Բ���A��
// 					status = OldZwReadFile(
// 						FileHandle,
// 						Event,
// 						ApcRoutine,
// 						ApcContext,
// 						IoStatusBlock,
// 						Buffer,
// 						Length,
// 						ByteOffset,
// 						Key
// 						);
// 					memcpy(Buffer,"call",strlen("call"));
// 					Length = 8;
// 					return STATUS_UNSUCCESSFUL;
// 				}
				ProtectEProcess = PsGetCurrentProcess();   //�Լ��Ľ��̰�
				ProtectProcessId = PsGetCurrentProcessId();  //Shadow SSDT ��Ҫ�õ�

				if (DebugOn)
					KdPrint(("ProtectCode:%08x\r\n",ProtectEProcess));

				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				memcpy(Buffer,"hehe",strlen("hehe"));
				Length = 8;
				bIsInitSuccess = TRUE;
			}
		}
		return STATUS_UNSUCCESSFUL;
	}
	/*

	ö��SSDT

	*/
	if (FileHandle == LIST_SSDT ||
		FileHandle == LIST_SSDT_ALL)
	{
		if (FileHandle == LIST_SSDT_ALL)
		{
			//KdPrint(("Print SSDT All"));
			bSSDTAll = TRUE;
		}
		if (FileHandle == LIST_SSDT)
		{
			//KdPrint(("Print SSDT"));
		}

		ReLoadNtosCALL(&Rmemcpy,L"memcpy",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
		ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
		if (RExAllocatePool &&
			RExFreePool &&
			Rmemcpy)
		{
			bInit = TRUE;
		}
		if (!bInit)
			return STATUS_UNSUCCESSFUL;

// 		ulKeServiceDescriptorTable = GetSystemRoutineAddress(1,L"KeServiceDescriptorTable");
// 		ulSize = ((PSERVICE_DESCRIPTOR_TABLE)ulKeServiceDescriptorTable)->TableSize;
		SSDTInfo = (PSSDTINFO)RExAllocatePool(NonPagedPool,sizeof(SSDTINFO)*800);
		if (SSDTInfo)
		{
			memset(SSDTInfo,0,sizeof(SSDTINFO)*800);
			PrintSSDT(SSDTInfo);
			if (Length > sizeof(SSDTINFO)*800)
			{
				for (i = 0; i< SSDTInfo->ulCount ;i++)
				{
					if (DebugOn)
						KdPrint(("[%d]����SSDT hook\r\n"
						"�����:%d\r\n"
						"��ǰ��ַ:%08x\r\n"
						"ԭʼ��ַ:%08x\r\n"
						"��������:%s\r\n"
						"��ǰhookģ��:%s\r\n"
						"��ǰģ���ַ:%08x\r\n"
						"��ǰģ���С:%d KB\r\n"
						"Hook����:%d\r\n\r\n",
						i,
						SSDTInfo->SSDT[i].ulNumber,
						SSDTInfo->SSDT[i].ulMemoryFunctionBase,
						SSDTInfo->SSDT[i].ulRealFunctionBase,
						SSDTInfo->SSDT[i].lpszFunction,
						SSDTInfo->SSDT[i].lpszHookModuleImage,
						SSDTInfo->SSDT[i].ulHookModuleBase,
						SSDTInfo->SSDT[i].ulHookModuleSize/1024,
						SSDTInfo->SSDT[i].IntHookType));
				}
				status = OldZwReadFile(
					FileHandle,
					Event,
					ApcRoutine,
					ApcContext,
					IoStatusBlock,
					Buffer,
					Length,
					ByteOffset,
					Key
					);
				Rmemcpy(Buffer,SSDTInfo,sizeof(SSDTINFO)*800);
				Length = sizeof(SSDTINFO)*800;
			}
			RExFreePool(SSDTInfo);
		}
		bSSDTAll = FALSE;
		return status;
	}
_FunctionRet:
	return OldZwReadFile(
		FileHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		Buffer,
		Length,
		ByteOffset,
		Key
		);
}
__declspec(naked) NTSTATUS NtReadFileHookZone(,...)
{
	_asm
	{
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		_emit 0x90;
		jmp [NtReadFileRet];
	}
}
/*

�ں��µ�Sleep
������Ҫsleep�ĺ���

*/
VOID WaitMicroSecond(__in LONG MicroSeconds)
{
	KEVENT KEnentTemp;
	LARGE_INTEGER waitTime;

	KeInitializeEvent(
		&KEnentTemp, 
		SynchronizationEvent, 
		FALSE
		);
	waitTime = RtlConvertLongToLargeInteger(-10 * MicroSeconds);

	KeWaitForSingleObject(
		&KEnentTemp,
		Executive,
		KernelMode,
		FALSE, 
		&waitTime
		);
}
/*

����Hook NtReadFile�ͻָ�
��Ϊ֮ǰҪ��ȡcsrss.exe��EPROCESS��ssdt hook ZwReadFile�ǻ�ȡ������
����Ҫinline hook NtReadFile

*/

VOID ResetMyControl()
{
	BOOL bRet = FALSE;
	KIRQL oldIrql = NULL; 

	while (1)
	{
		if (!bRet)
		{
			bRet = HookFunctionHeader((DWORD)NewNtReadFile,L"ZwReadFile",TRUE,0,(PVOID)NtReadFileHookZone,&NtReadFilePatchCodeLen,&NtReadFileRet);
			if (DebugOn)
				KdPrint(("inline hook ZwReadFile success"));
		}
		//ȡ��pid֮��
		if (IsExitProcess(AttachGuiEProcess))
		{
			//�߳����IRQL̫���ˣ�Ҫ������
			if (KeGetCurrentIrql() <= DISPATCH_LEVEL &&
				KeGetCurrentIrql() > PASSIVE_LEVEL)
			{
				if (!oldIrql)
					oldIrql = KeRaiseIrqlToDpcLevel(); //ע�������� 
			}
			UnHookFunctionHeader(L"ZwReadFile",TRUE,0,(PVOID)NtReadFileHookZone,NtReadFilePatchCodeLen);  //�ָ�һ��

			if (oldIrql)
				KeLowerIrql(oldIrql);

			KdPrint(("Init Protect Thread success\r\n"));
			
			/*

			���ɨ�裬�������������ܵ���PsTerminateSystemThread�����߳�
			Ӧ�����߳��Լ�����

			*/
			break;
		}
		WaitMicroSecond(88);
	}
}

NTSTATUS __stdcall NewZwTerminateProcess(
	IN HANDLE  ProcessHandle,
	IN NTSTATUS  ExitStatus
	)
{
	PEPROCESS EProcess;
	NTSTATUS status;
	ZWTERMINATEPROCESS OldZwTerminateProcess;
	KPROCESSOR_MODE PreviousMode;

	//KdPrint(("bIsProtect360:%d",bIsProtect360));
	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		goto _FunctionRet;
	}
	//����˳���
	if (!bIsInitSuccess)
		goto _FunctionRet;

	//Ĭ�ϵ�һ��򵥱�����
// 	if (!bProtectProcess)
// 		goto _FunctionRet;

	if (ProcessHandle &&
		ARGUMENT_PRESENT(ProcessHandle))
	{
		status = ObReferenceObjectByHandle(
			ProcessHandle,
			PROCESS_ALL_ACCESS,
			0,
			KernelMode,
			(PVOID*)&EProcess,
			NULL
			);
		if (NT_SUCCESS(status))
		{
			ObDereferenceObject(EProcess);

			//��������
			if (EProcess == ProtectEProcess &&
				PsGetCurrentProcess() != ProtectEProcess)
			{
				return STATUS_ACCESS_DENIED;
			}
		}
	}
_FunctionRet:
	OldZwTerminateProcess = OriginalServiceDescriptorTable->ServiceTable[ZwTerminateProcessIndex];
	return OldZwTerminateProcess(
		ProcessHandle,
		ExitStatus
		);
}
/*

��ʼ��ͨ�ſ���

*/
BOOL InitControl()
{
	UNICODE_STRING UnicdeFunction;
	HANDLE ThreadHandle;
	PEPROCESS EProcess;

 	if (SystemCallEntryTableHook(
		"ZwReadFile",
		&ZwReadFileIndex,
		NewNtReadFile) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Init Control Thread success 1\r\n"));
	}
	if (bKernelBooting)
	{
		//������Ҫ��ʼ��
		bIsInitSuccess = TRUE;
		KdPrint(("kernel booting\r\n"));
	}
	if (SystemCallEntryTableHook(
		"ZwTerminateProcess",
		&ZwTerminateProcessIndex,
		NewZwTerminateProcess) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Create Control Thread success 2\r\n"));
	}
	InitZwSetValueKey();  //ע���
 	InitNetworkDefence();

 	InitWriteFile();

	//ȥ��object hook�����ļ�����ʱ����Ҫ��
	//InstallFileObejctHook();
	InitKernelThreadData();   //kernel thread hook

	if (PsCreateSystemThread(
		&ThreadHandle,
		0,
		NULL,
		NULL,
		NULL,
		ResetMyControl,
		NULL) == STATUS_SUCCESS)
	{
		ZwClose(ThreadHandle);
		if (DebugOn)
			KdPrint(("Create Control Thread success 2\r\n"));
	}
}