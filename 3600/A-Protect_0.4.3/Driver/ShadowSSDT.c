#include "ShadowSSDT.h"
#include "SDTShadowRestore.h"

NTSTATUS __stdcall NewNtUserFindWindowEx(
	IN HWND hwndParent, 
	IN HWND hwndChild, 
	IN PUNICODE_STRING pstrClassName OPTIONAL, 
	IN PUNICODE_STRING pstrWindowName OPTIONAL, 
	IN DWORD dwType)
{
	ULONG result;
	NTUSERFINDWINDOWEX OldNtUserFindWindowEx;
	NTUSERQUERYWINDOW OldNtUserQueryWindow;

	//KdPrint(("NtUserFindWindowEx Called\n"));

	OldNtUserFindWindowEx = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserFindWindowExIndex];
	result = OldNtUserFindWindowEx(hwndParent, hwndChild, pstrClassName, pstrWindowName, dwType);

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		ULONG ProcessID;

		OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
		ProcessID = OldNtUserQueryWindow(result, 0);

		if (ProcessID==(ULONG)ProtectProcessId)
			return 0;
	}
	//���A���Լ������Լ�����Ź�
	return result;
}

NTSTATUS  __stdcall NewNtUserBuildHwndList(IN HDESK hdesk, IN HWND hwndNext, IN ULONG fEnumChildren, IN DWORD idThread, IN UINT cHwndMax, OUT HWND *phwndFirst, OUT ULONG* pcHwndNeeded)
{
	NTSTATUS result;
	NTUSERQUERYWINDOW OldNtUserQueryWindow;
	NTUSERBUILDHWNDLIST OldNtUserBuildHwndList;

	//KdPrint(("NtUserBuildHwndList Called\n"));

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		ULONG ProcessID;

		if (fEnumChildren==1)
		{
			OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
			ProcessID = OldNtUserQueryWindow((ULONG)hwndNext, 0);

			if (ProcessID==(ULONG)ProtectProcessId)
				return STATUS_UNSUCCESSFUL;
		}
		OldNtUserBuildHwndList = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserBuildHwndListIndex];
		result = OldNtUserBuildHwndList(hdesk,hwndNext,fEnumChildren,idThread,cHwndMax,phwndFirst,pcHwndNeeded);

		if (result==STATUS_SUCCESS)
		{
			ULONG i=0;
			ULONG j;

			while (i<*pcHwndNeeded)
			{
				OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
				ProcessID=OldNtUserQueryWindow((ULONG)phwndFirst[i],0);
				if (ProcessID==(ULONG)ProtectProcessId)
				{
					for (j=i; j<(*pcHwndNeeded)-1; j++)					
						phwndFirst[j]=phwndFirst[j+1]; 

					phwndFirst[*pcHwndNeeded-1]=0; 

					(*pcHwndNeeded)--;
					continue; 
				}
				i++;				
			}

		}
		return result;
	}
	//���A���Լ������Լ�����Ź�
	OldNtUserBuildHwndList = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserBuildHwndListIndex];
	return OldNtUserBuildHwndList(hdesk,hwndNext,fEnumChildren,idThread,cHwndMax,phwndFirst,pcHwndNeeded);
}

ULONG  __stdcall NewNtUserGetForegroundWindow(VOID)
{
	ULONG result;
	NTUSERGETFOREGROUNDWINDOW OldNtUserGetForegroundWindow;
	NTUSERQUERYWINDOW OldNtUserQueryWindow;

	//KdPrint(("NtUserGetForegroundWindow Called\n"));

	OldNtUserGetForegroundWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserGetForegroundWindowIndex];
	result= OldNtUserGetForegroundWindow();	

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		ULONG ProcessID;

		OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
		ProcessID=OldNtUserQueryWindow(result, 0);

		if (ProcessID == (ULONG)ProtectProcessId)
			result=LastForegroundWindow;
		else
			LastForegroundWindow=result;
	}	
	//���A���Լ������Լ�����Ź�
	return result;
}

UINT_PTR  __stdcall NewNtUserQueryWindow(IN ULONG WindowHandle,IN ULONG TypeInformation)
{
	ULONG WindowHandleProcessID;
	NTUSERQUERYWINDOW OldNtUserQueryWindow;

	//KdPrint(("NtUserQueryWindow Called\n"));

	OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		WindowHandleProcessID = OldNtUserQueryWindow(WindowHandle,0);
		if (WindowHandleProcessID==(ULONG)ProtectProcessId)
			return 0;
	}
	//���A���Լ������Լ�����Ź�
	return OldNtUserQueryWindow(WindowHandle,TypeInformation);
}
BOOL __stdcall NewNtUserDestroyWindow(HWND hWnd)  
{   
	NTUSERDESTROYWINDOW OldNtUserDestroyWindow;
	NTUSERQUERYWINDOW OldNtUserQueryWindow;
	ULONG WindowHandleProcessID;

	//KdPrint(("NtUserDestroyWindow Called\n"));

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
		WindowHandleProcessID = OldNtUserQueryWindow(hWnd,0);
		if (WindowHandleProcessID == (ULONG)ProtectProcessId)
			return 0;
	}
	//���A���Լ������Լ�����Ź�
	OldNtUserDestroyWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserDestroyWindowIndex];
	return OldNtUserDestroyWindow(hWnd);
}
NTSTATUS __stdcall NewNtUserPostMessage(
	IN HWND hWnd,
	IN ULONG pMsg,
	IN ULONG wParam,
	IN ULONG lParam
	)  
{   
	NTUSERQUERYWINDOW OldNtUserQueryWindow;
	NTUSERPOSTMESSAGE OldNtUserPostMessage;

	ULONG WindowHandleProcessID;

	//KdPrint(("NtUserPostMessage Called\n"));

	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		OldNtUserQueryWindow = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserQueryWindowIndex];
		WindowHandleProcessID = OldNtUserQueryWindow(hWnd,0);
		if (WindowHandleProcessID == (ULONG)ProtectProcessId)
			return 0;
	}
	//���A���Լ������Լ�����Ź�
	OldNtUserPostMessage = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserPostMessageIndex];
	return OldNtUserPostMessage(hWnd,pMsg,wParam,lParam);
}
BOOL __stdcall NewNtUserPostThreadMessage(
	IN DWORD idThread,
	IN UINT Msg,
	IN ULONG wParam,
	IN ULONG lParam
	)
{
	PEPROCESS EProcess = NULL;
	PETHREAD  Ethread = NULL;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	NTUSERPOSTTHREADMESSAGE OldNtUserPostThreadMessage;

	//�����NtUserPostThreadMessage��������ʲô�ȼ��£�ֻ���޶���PASSIVE_LEVEL��
	if (KeGetCurrentIrql() != PASSIVE_LEVEL){
		goto _FuncRet;
	}
	if (RPsGetCurrentProcess() != ProtectEProcess)
	{
		status=PsLookupThreadByThreadId((HANDLE)idThread,&Ethread);
		if (NT_SUCCESS(status))
		{
			//�������
			ObDereferenceObject(Ethread);

			EProcess=IoThreadToProcess(Ethread);
			if(EProcess == ProtectEProcess){
				//�����߲���A�ܣ�����ȷ����A�ܵĴ��ڣ��ܾ�
				return FALSE;
			}
		}
	}
_FuncRet:
	//���A���Լ������Լ�����Ź�
	OldNtUserPostThreadMessage = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserPostThreadMessageIndex];
	return OldNtUserPostThreadMessage(
		idThread,
		Msg,
		wParam,
		lParam
		);
}
//����ȫ�ֹ���Ŷ
HHOOK __stdcall NewNtUserSetWindowsHookEx(
	HINSTANCE Mod, 
	PUNICODE_STRING UnsafeModuleName, 
	DWORD ThreadId, 
	int HookId, 
	PVOID HookProc, 
	BOOL Ansi)
{
	PEPROCESS EProcess = NULL;
	PETHREAD  Ethread = NULL;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	NTUSERSETWINDOWSHOOKEX OldNtUserSetWindowsHookEx;

	//����ȫ�ֹ�����
	if (!ThreadId){

		//��� bDisSetWindowsHook = FALSE������ֹȫ�ֹ���
		if (!bDisSetWindowsHook){
			return FALSE;
		}
	}
	OldNtUserSetWindowsHookEx = OriginalShadowServiceDescriptorTable->ServiceTable[NtUserSetWindowsHookExIndex];
	return OldNtUserSetWindowsHookEx(
		Mod, 
		UnsafeModuleName, 
		ThreadId, 
		HookId, 
		HookProc,
		Ansi 
		);
}
VOID InitShadowSSDTHook()
{
	UNICODE_STRING UnicdeFunction;

	//EnumWindows ö�����ж��㴰��
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserBuildHwndList",
		&NtUserBuildHwndListIndex,
		NewNtUserBuildHwndList) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Init NtUserBuildHwndList Thread success\r\n"));
	}
	//GetForegroundWindow �õ���ǰ���㴰��
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserGetForegroundWindow",
		&NtUserGetForegroundWindowIndex,
		NewNtUserGetForegroundWindow) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Init NtUserGetForegroundWindow Thread success\r\n"));
	}
	//GetWindowThreadProcessId ��ȡ�����Ӧ�Ľ���PID
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserQueryWindow",
		&NtUserQueryWindowIndex,
		NewNtUserQueryWindow) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Init NtUserQueryWindow Thread success\r\n"));
	}
	//FindWindow ���Ҵ��ڻ�ȡ���
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserFindWindowEx",
		&NtUserFindWindowExIndex,
		NewNtUserFindWindowEx) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Init NtUserFindWindowEx Thread success\r\n"));
	}
	//DestroyWindow ���ٴ���
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserDestroyWindow",
		&NtUserDestroyWindowIndex,
		NewNtUserDestroyWindow) == TRUE)
	{
		if (DebugOn) 
			KdPrint(("Init NtUserDestroyWindow Thread success\r\n"));
	}
	//PostMessage ������Ϣ
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserPostMessage",
		&NtUserPostMessageIndex,
		NewNtUserPostMessage) == TRUE)
	{
		if (DebugOn) 
			KdPrint(("Init NtUserPostMessage Thread success\r\n"));
	}
	//SendMessage ������Ϣ
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserPostThreadMessage",
		&NtUserPostThreadMessageIndex,
		NewNtUserPostThreadMessage) == TRUE)
	{
		if (DebugOn) 
			KdPrint(("Init NtUserPostThreadMessage Thread success\r\n"));
	}
	//����ȫ�ֹ���
	if (SystemCallEntryShadowSSDTTableHook(
		"NtUserSetWindowsHookEx",
		&NtUserSetWindowsHookExIndex,
		NewNtUserSetWindowsHookEx) == TRUE)
	{
		if (DebugOn) 
			KdPrint(("Init NtUserSetWindowsHookEx Thread success\r\n"));
	}
}
VOID ShadowSSDTHookCheck(PSHADOWSSDTINFO ShadowSSDTInfo)
{
	ULONG ulReloadShadowSSDTAddress;
	ULONG ulOldShadowSSDTAddress;
	ULONG ulCodeSize,ulReloadCodeSize;
	ULONG ulHookFunctionAddress;
	PUCHAR ulTemp;
	PUCHAR ulReloadTemp;

	INSTRUCTION	Inst;
	INSTRUCTION	Instb;
	PUCHAR p;
	ULONG ulHookModuleBase;
	ULONG ulHookModuleSize;
	CHAR lpszHookModuleImage[256];
	CHAR *lpszFunction = NULL;
	int i = 0,count=0;
	WIN_VER_DETAIL WinVer;
	ULONG ulMemoryFunctionBase;
	BOOL bIsHooked = FALSE;
	ULONG ulSize;
	int JmpCount = 0;

	__try
	{
		for (i=0 ;i<OriginalShadowServiceDescriptorTable->TableSize ;i++)
		{
			ulReloadShadowSSDTAddress = OriginalShadowServiceDescriptorTable->ServiceTable[i];
			ulOldShadowSSDTAddress = ulReloadShadowSSDTAddress - (ULONG)Win32kImageModuleBase + ulWin32kBase;

			if (MmIsAddressValidEx(ulReloadShadowSSDTAddress) &&
				MmIsAddressValidEx(ulOldShadowSSDTAddress))
			{
				ulReloadCodeSize = GetFunctionCodeSize(ulReloadShadowSSDTAddress);

				if (IsFuncInInitSection(ulOldShadowSSDTAddress,ulReloadCodeSize)){
					continue;
				}
				ulCodeSize = GetFunctionCodeSize(ulOldShadowSSDTAddress);
				if (ulCodeSize != ulReloadCodeSize){
					continue;
				}
				if (memcmp(ulReloadShadowSSDTAddress,ulOldShadowSSDTAddress,ulCodeSize) != NULL)
				{
					//KdPrint(("find inline hook[%d]%08x-%08x",i,ulOldShadowSSDTAddress,ulReloadShadowSSDTAddress));

					//��ʼɨ��hook
					for (p=ulOldShadowSSDTAddress ;p< ulOldShadowSSDTAddress+ulCodeSize; p++)
					{
						//�۰�ɨ�裬���ǰ��һ��һ������ʼɨ����һ��
						if (memcmp(ulReloadShadowSSDTAddress,ulOldShadowSSDTAddress,ulCodeSize/2) == NULL)
						{
							ulCodeSize = ulCodeSize + ulCodeSize/2;
							continue;
						}
						//�Ƿ������
						if (*p == 0xcc ||
							*p == 0xc2)
						{
							break;
						}
						ulTemp = NULL;
						get_instruction(&Inst,p,MODE_32);
						switch (Inst.type)
						{
						case INSTRUCTION_TYPE_JMP:
							if(Inst.opcode==0xFF&&Inst.modrm==0x25)
							{
								//DIRECT_JMP
								ulTemp = Inst.op1.displacement;
							}
							else if (Inst.opcode==0xEB)
							{
								ulTemp = (ULONG)(p+Inst.op1.immediate);
							}
							else if(Inst.opcode==0xE9)
							{
								//RELATIVE_JMP;
								ulTemp = (ULONG)(p+Inst.op1.immediate);
							}
							break;
						case INSTRUCTION_TYPE_CALL:
							if(Inst.opcode==0xFF&&Inst.modrm==0x15)
							{
								//DIRECT_CALL
								ulTemp = Inst.op1.displacement;
							}
							else if (Inst.opcode==0x9A)
							{
								ulTemp = (ULONG)(p+Inst.op1.immediate);
							}
							else if(Inst.opcode==0xE8)
							{
								//RELATIVE_CALL;
								ulTemp = (ULONG)(p+Inst.op1.immediate);
							}
							break;
						case INSTRUCTION_TYPE_PUSH:
							if(!MmIsAddressValidEx((PVOID)(p)))
							{
								break;
							}
							get_instruction(&Instb,(BYTE*)(p),MODE_32);
							if(Instb.type == INSTRUCTION_TYPE_RET)
							{
								//StartAddress+len-inst.length-instb.length;
								ulTemp = Instb.op1.displacement;
							}
							break;
						}
						if (MmIsAddressValidEx(ulTemp) &&
							MmIsAddressValidEx(p) && 
							ulTemp != p)   //hook�ĵ�ַҲҪ��Ч�ſ���Ŷ
						{
							//�õ�����
							ulSize = p - ulOldShadowSSDTAddress;
							ulReloadTemp = ulReloadShadowSSDTAddress + ulSize;
							if (MmIsAddressValidEx(ulReloadTemp))
							{
								if (*(ULONG *)p == *(ULONG *)ulReloadTemp){
									continue;
								}
							}else{
								continue;
							}
							if (DebugOn)
								KdPrint(("ulTemp:%08x %08x %08x\n",ulTemp,ulReloadTemp,p));

// 							ulTemp = ulTemp+0x5;
// 							//�򵥴���һ�¶�����
// 							if (*ulTemp == 0xe9 ||
// 								*ulTemp == 0xe8)
// 							{
// 								if (DebugOn)
// 									KdPrint(("ulTemp == 0xe9"));
// 
// 								ulTemp = *(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5);
// 							}
							//�򵥴���һ�¶༶��(��Ĭ��10����ת)
							for (JmpCount=0;JmpCount<10;JmpCount++)
							{
								if (MmIsAddressValidEx(ulTemp))
								{
									ulTemp = ulTemp+0x5;

									if (*ulTemp == 0xe9 ||
										*ulTemp == 0xe8)
									{
										if (DebugOn)
											KdPrint(("ulTemp == 0xe9"));

										ulTemp = *(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5);

									}else
									{
										break;
									}
								}
							}
							//�������hook��
							memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
							if (!IsAddressInSystem(
								ulTemp,
								&ulHookModuleBase,
								&ulHookModuleSize,
								lpszHookModuleImage))
							{
								memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
								strcat(lpszHookModuleImage,"Unknown");
								ulHookModuleBase = 0;
								ulHookModuleSize = 0;
							}
							ShadowSSDTInfo->ulCount = count;
							ShadowSSDTInfo->SSDT[count].ulNumber = i;
							ShadowSSDTInfo->SSDT[count].ulMemoryFunctionBase = ulTemp;
							ShadowSSDTInfo->SSDT[count].ulRealFunctionBase = ulOldShadowSSDTAddress;

							memset(ShadowSSDTInfo->SSDT[count].lpszFunction,0,sizeof(ShadowSSDTInfo->SSDT[count].lpszFunction));
							WinVer = GetWindowsVersion();
							switch (WinVer)
							{
							case WINDOWS_VERSION_XP:
								strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,XPProcName[i],strlen(XPProcName[i]));
								break;
							case WINDOWS_VERSION_2K3_SP1_SP2:
								strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,Win2003ProcName[i],strlen(Win2003ProcName[i]));
								break;
							case WINDOWS_VERSION_7_7000:
							case WINDOWS_VERSION_7_7600_UP:
								strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,Win7ProcName[i],strlen(Win7ProcName[i]));
								break;
							}

							memset(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,0,sizeof(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage));
							strncpy(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));
							ShadowSSDTInfo->SSDT[count].ulHookModuleBase = ulHookModuleBase;
							ShadowSSDTInfo->SSDT[count].ulHookModuleSize = ulHookModuleSize;
							ShadowSSDTInfo->SSDT[count].IntHookType = SSDTINLINEHOOK;

							if (DebugOn)
								KdPrint(("[%d]Found ssdt inline hook!!:%s-%s",
								ShadowSSDTInfo->ulCount,
								ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,
								ShadowSSDTInfo->SSDT[count].lpszFunction));

							count++;

							ulTemp = NULL;
						}
					}
				}
			}
			//shadowssdt hook
			bShadowHooked = TRUE;
			ulMemoryFunctionBase = ShadowSSDTTable[1].ServiceTable[i];
			//���˵������hook
			if (ulMemoryFunctionBase == ulOldShadowSSDTAddress)
			{
				bShadowHooked = FALSE;
			}
			//ö�ٱ�hook�� �������е�shadow����
			if (bShadowHooked ||
				bShadowSSDTAll == TRUE)
			{
				memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
				if (!IsAddressInSystem(
					ulMemoryFunctionBase,
					&ulHookModuleBase,
					&ulHookModuleSize,
					lpszHookModuleImage))
				{
					strcat(lpszHookModuleImage,"Unknown");
					ulHookModuleBase = 0;
					ulHookModuleSize = 0;
				}
				ShadowSSDTInfo->ulCount = count;
				ShadowSSDTInfo->SSDT[count].ulNumber = i;
				ShadowSSDTInfo->SSDT[count].ulMemoryFunctionBase = ulMemoryFunctionBase;
				ShadowSSDTInfo->SSDT[count].ulRealFunctionBase = ulOldShadowSSDTAddress;

				memset(ShadowSSDTInfo->SSDT[count].lpszFunction,0,sizeof(ShadowSSDTInfo->SSDT[count].lpszFunction));
				WinVer = GetWindowsVersion();
				switch (WinVer)
				{
				case WINDOWS_VERSION_XP:
					strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,XPProcName[i],strlen(XPProcName[i]));
					break;
				case WINDOWS_VERSION_2K3_SP1_SP2:
					strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,Win2003ProcName[i],strlen(Win2003ProcName[i]));
					break;
				case WINDOWS_VERSION_7_7000:
				case WINDOWS_VERSION_7_7600_UP:
					strncpy(ShadowSSDTInfo->SSDT[count].lpszFunction,Win7ProcName[i],strlen(Win7ProcName[i]));
					break;
				}

				memset(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,0,sizeof(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage));
				strncpy(ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));

				ShadowSSDTInfo->SSDT[count].ulHookModuleBase = ulHookModuleBase;
				ShadowSSDTInfo->SSDT[count].ulHookModuleSize = ulHookModuleSize;

				if (bShadowHooked == FALSE)
				{
					ShadowSSDTInfo->SSDT[count].IntHookType = NOHOOK;
				}else
					ShadowSSDTInfo->SSDT[count].IntHookType = SSDTHOOK;  //ssdt hook

				if (DebugOn)
					KdPrint(("[%d]Found ssdt hook!!:%s-%s",
					ShadowSSDTInfo->ulCount,
					ShadowSSDTInfo->SSDT[count].lpszHookModuleImage,
					ShadowSSDTInfo->SSDT[count].lpszFunction));

				count++;
			}
		}

	}__except(EXCEPTION_EXECUTE_HANDLER){

	}
}
//8888Ϊȫ��������Ϊ����
BOOL RestoreAllShadowSSDTFunction(ULONG IntType)
{
	ULONG ulMemoryFunctionBase;
	ULONG ulRealMemoryFunctionBase;
	ULONG ulReloadFunctionBase;

	BOOL bHooked = FALSE;
	int i = 0;
	BOOL bReSetOne = FALSE;

	for (i=0 ;i<ShadowSSDTTable[1].TableSize ;i++)
	{
		bHooked = TRUE;
		ulMemoryFunctionBase = ShadowSSDTTable[1].ServiceTable[i];

		//�õ�ԭ��ϵͳ�ڴ���shadowssdt�ĵ�ַ������reload��shadowssdtŶ����
		ulReloadFunctionBase = OriginalShadowServiceDescriptorTable->ServiceTable[i];
		ulRealMemoryFunctionBase = ulReloadFunctionBase - (ULONG)Win32kImageModuleBase + ulWin32kBase;

		if (ulMemoryFunctionBase > ulWin32kBase &&
			ulMemoryFunctionBase < ulWin32kBase + ulWin32kSize)
		{
			bHooked = FALSE;
		}
		if (bHooked == TRUE)
		{
			if (DebugOn)
				KdPrint(("[%d]%08x  %08x",i,ulReloadFunctionBase,ulRealMemoryFunctionBase));

			//��ʼ�ָ�
			//�ָ�ȫ��
			if (IntType == 8888)
			{
				__asm
				{
					cli
						push eax
						mov eax,cr0
						and eax,not 0x10000
						mov cr0,eax
						pop eax
				}
				ShadowSSDTTable[1].ServiceTable[i] = ulRealMemoryFunctionBase;

				if (DebugOn)
					KdPrint(("[%d]%08x  %08x",i,ShadowSSDTTable[1].ServiceTable[i],ulRealMemoryFunctionBase));
				__asm
				{
					push eax
						mov eax,cr0
						or eax,0x10000
						mov cr0,eax
						pop eax
						sti
				}
			}   
			else  //�ָ�����
			{
				if (IntType == i)
				{
					__asm
					{
						cli
							push eax
							mov eax,cr0
							and eax,not 0x10000
							mov cr0,eax
							pop eax
					}
					ShadowSSDTTable[1].ServiceTable[i] = ulRealMemoryFunctionBase;
					__asm
					{
						push eax
							mov eax,cr0
							or eax,0x10000
							mov cr0,eax
							pop eax
							sti
					}
				}
			}
		}
		//�ָ�inline hook
		if (IntType == 8888)
		{
			RestoreShadowInlineHook(i);
		}else
		{
			if (IntType == i)
			{
				RestoreShadowInlineHook(i);
				break;
			}
		}
	}
	return TRUE;
}
BOOL RestoreShadowInlineHook(ULONG ulNumber)
{
	ULONG ulFunction = NULL;
	ULONG ulReloadFunction;
	BOOL bInit = FALSE;
	int i=0;

	for (i=0 ;i<ShadowSSDTTable[1].TableSize ;i++)
	{
		if (ulNumber == i)
		{
			ulFunction = OriginalShadowServiceDescriptorTable->ServiceTable[i] - (ULONG)Win32kImageModuleBase + ulWin32kBase;
			ulReloadFunction = OriginalShadowServiceDescriptorTable->ServiceTable[i];

			if (MmIsAddressValidEx(ulFunction) &&
				MmIsAddressValidEx(ulReloadFunction))
			{
				if (GetFunctionCodeSize(ulFunction) != GetFunctionCodeSize(ulReloadFunction))
				{
					return FALSE;
				}
				__asm
				{
					cli
						push eax
						mov eax,cr0
						and eax,not 0x10000
						mov cr0,eax
						pop eax
				}
				memcpy(ulFunction,ulReloadFunction,GetFunctionCodeSize(ulFunction));
				__asm
				{
					push eax
						mov eax,cr0
						or eax,0x10000
						mov cr0,eax
						pop eax
						sti
				}
				bInit = TRUE;
				break;
			}
		}
	}
	return TRUE;
}