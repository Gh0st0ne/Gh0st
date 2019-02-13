#include "SSDT.h"

BOOL PrintSSDT(PSSDTINFO SSDTInfo)
{
	PSERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;
	ULONG ulMemorySSDTBase;
	ULONG ulMemoryFunctionBase;
	
	ULONG ulHookModuleBase;
	ULONG ulHookModuleSize;
	CHAR lpszHookModuleImage[256];
	CHAR lpszFunction[256];
	BOOL bInit = FALSE;
	BOOL bHooked = FALSE;
	//int i = 0;
	int SSDTIndex = 0;
	int count = 0;

	ULONG ulReloadKiFastCallEntry;
	ULONG ulKiFastCallEntry;
	PUCHAR ulTemp,ulReloadTemp;
	PUCHAR p;

	ULONG ulOldSSDTAddress;
	ULONG ulReloadSSDTAddress;
	ULONG ulHookFunctionAddress;
	INSTRUCTION	Inst;
	INSTRUCTION	Instb;
	ULONG ulCodeSize;
	HANDLE hNtSection;
	UNICODE_STRING wsNtDllString;
	ULONG ulNtDllModuleBase;
	ULONG ulSize = 0;
	int JmpCount = 0;

	ReLoadNtosCALL(&RRtlInitUnicodeString,L"RtlInitUnicodeString",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RZwClose,L"ZwClose",SystemKernelModuleBase,ImageModuleBase);
	if (RRtlInitUnicodeString &&
		RZwClose)
	{
		bInit = TRUE;
	}
	if (!bInit)
		return NULL;

	__try
	{

		KeServiceDescriptorTable = GetSystemRoutineAddress(1,L"KeServiceDescriptorTable");
		if (KeServiceDescriptorTable)
		{
			RRtlInitUnicodeString(&wsNtDllString,L"\\SystemRoot\\System32\\ntdll.dll");
			hNtSection = MapFileAsSection(&wsNtDllString,&ulNtDllModuleBase);  //���뵽�ڴ�
			if (!hNtSection)
			{
				if (DebugOn)
					KdPrint(("MapFileAsSection failed"));

				return FALSE;
			}
			RZwClose(hNtSection);

			for (SSDTIndex = 0 ;SSDTIndex<KeServiceDescriptorTable->TableSize ;SSDTIndex++)
			{
				//��ʼ���ssdt inline hook
				//
				memset(lpszFunction,0,sizeof(lpszFunction));
				if (!GetFunctionNameByIndex(ulNtDllModuleBase,&SSDTIndex,lpszFunction))
				{
					goto SearchSSDTHook;
				}
// 				ulOldSSDTAddress = NULL;
// 				ulOldSSDTAddress = GetSystemRoutineAddress(0,lpszFunction);
				//ɨ��ing~
//				ulReloadSSDTAddress = ulOldSSDTAddress - SystemKernelModuleBase + ImageModuleBase;

				ulReloadSSDTAddress = OriginalServiceDescriptorTable->ServiceTable[SSDTIndex];
				ulOldSSDTAddress = ulReloadSSDTAddress - (ULONG)ImageModuleBase + SystemKernelModuleBase;

				if (!MmIsAddressValidEx(ulOldSSDTAddress) ||
					!MmIsAddressValidEx(ulReloadSSDTAddress))
				{
					goto SearchSSDTHook;
				}
				ulCodeSize = GetFunctionCodeSize(ulOldSSDTAddress);

				if (ulCodeSize == GetFunctionCodeSize(ulReloadSSDTAddress) &&
					memcmp(ulReloadSSDTAddress,ulOldSSDTAddress,ulCodeSize) == NULL)
				{
					goto SearchSSDTHook;
				}
				if (DebugOn)
					KdPrint(("---->%s:%08x",lpszFunction,ulOldSSDTAddress));

				//��ʼɨ��hook
				for (p=ulOldSSDTAddress ;p< ulOldSSDTAddress+ulCodeSize; p++)
				{
					//�۰�ɨ�裬���ǰ��һ��һ������ʼɨ����һ��
					if (memcmp(ulReloadSSDTAddress,ulOldSSDTAddress,ulCodeSize/2) == NULL)
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
						ulSize = p - ulOldSSDTAddress;
						ulReloadTemp = ulReloadSSDTAddress + ulSize;
						if (MmIsAddressValidEx(ulReloadTemp))
						{
							//ͬһ�����ȵ�reload�Ͳ�reload��5�ֽ�һ����˵��û��hook
							if (*(ULONG *)p == *(ULONG *)ulReloadTemp){
								continue;
							}
						}else{
							continue;
						}
						if (DebugOn)
							KdPrint(("ulTemp:%08x %08x %08x\n",ulTemp,ulReloadTemp,p));

// 						ulTemp = ulTemp+0x5;
// 						//�򵥴���һ�¶�����
// 						if (*ulTemp == 0xe9 ||
// 							*ulTemp == 0xe8)
// 						{
// 							if (DebugOn)
// 								KdPrint(("ulTemp == 0xe9"));
// 							ulTemp = *(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5);
// 						}
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
						SSDTInfo->ulCount = count;
						SSDTInfo->SSDT[count].ulNumber = SSDTIndex;
						SSDTInfo->SSDT[count].ulMemoryFunctionBase = ulTemp;
						SSDTInfo->SSDT[count].ulRealFunctionBase = ulOldSSDTAddress;

						memset(SSDTInfo->SSDT[count].lpszFunction,0,sizeof(SSDTInfo->SSDT[count].lpszFunction));
						strncpy(SSDTInfo->SSDT[count].lpszFunction,lpszFunction,strlen(lpszFunction));

						memset(SSDTInfo->SSDT[count].lpszHookModuleImage,0,sizeof(SSDTInfo->SSDT[count].lpszHookModuleImage));
						strncpy(SSDTInfo->SSDT[count].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));
						SSDTInfo->SSDT[count].ulHookModuleBase = ulHookModuleBase;
						SSDTInfo->SSDT[count].ulHookModuleSize = ulHookModuleSize;
						SSDTInfo->SSDT[count].IntHookType = SSDTINLINEHOOK;

						if (DebugOn)
							KdPrint(("[%d]Found ssdt inline hook!!:%s-%s",
							SSDTInfo->ulCount,
							SSDTInfo->SSDT[count].lpszHookModuleImage,
							SSDTInfo->SSDT[count].lpszFunction));

						count++;

						ulTemp = NULL;
					}
				}
SearchSSDTHook:
				///////////////////////////////////////////////////////////////////////
				//��ʼ���ssdt hook
				bHooked = TRUE;

				ulMemoryFunctionBase = KeServiceDescriptorTable->ServiceTable[SSDTIndex];
				if (ulMemoryFunctionBase == ulOldSSDTAddress)
				{
					bHooked = FALSE;
				}
				memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));

				if (bHooked ||
					bSSDTAll == TRUE)
				{
					if (DebugOn)
						KdPrint(("SSDT hook[%s] %d:%08x\r\n",lpszFunction,SSDTIndex,ulMemoryFunctionBase));

					if (strlen(lpszFunction) < 10)
					{
						memset(lpszFunction,0,sizeof(lpszFunction));
						if (!GetFunctionNameByIndex(ulNtDllModuleBase,&SSDTIndex,lpszFunction))
						{
							strcat(lpszFunction,"Unknown");
						}
					}
					if (!IsAddressInSystem(
						ulMemoryFunctionBase,
						&ulHookModuleBase,
						&ulHookModuleSize,
						lpszHookModuleImage))
					{
						memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
						strcat(lpszHookModuleImage,"Unknown");
						ulHookModuleBase = 0;
						ulHookModuleSize = 0;
					}
					SSDTInfo->ulCount = count;
					SSDTInfo->SSDT[count].ulNumber = SSDTIndex;
					SSDTInfo->SSDT[count].ulMemoryFunctionBase = ulMemoryFunctionBase;
					SSDTInfo->SSDT[count].ulRealFunctionBase = ulOldSSDTAddress;

					memset(SSDTInfo->SSDT[count].lpszFunction,0,sizeof(SSDTInfo->SSDT[count].lpszFunction));
					strncpy(SSDTInfo->SSDT[count].lpszFunction,lpszFunction,strlen(lpszFunction));

					memset(SSDTInfo->SSDT[count].lpszHookModuleImage,0,sizeof(SSDTInfo->SSDT[count].lpszHookModuleImage));
					strncpy(SSDTInfo->SSDT[count].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));

					SSDTInfo->SSDT[count].ulHookModuleBase = ulHookModuleBase;
					SSDTInfo->SSDT[count].ulHookModuleSize = ulHookModuleSize;

					if (bHooked == FALSE)
					{
						SSDTInfo->SSDT[count].IntHookType = NOHOOK;
					}else
						SSDTInfo->SSDT[count].IntHookType = SSDTHOOK;  //ssdt hook

					count++;
				}
			}
			//ɨ��KiFastCallEntry��hook
			_asm
			{
				pushad;
				mov ecx, 0x176;
				rdmsr;
				mov ulKiFastCallEntry, eax;
				popad;
			}
			ulReloadKiFastCallEntry = ulKiFastCallEntry - SystemKernelModuleBase + ImageModuleBase;
			if (DebugOn)
				KdPrint(("ulReloadKiFastCallEntry:%08x\r\n",ulReloadKiFastCallEntry));

			if (!MmIsAddressValidEx(ulReloadKiFastCallEntry) ||
				!MmIsAddressValidEx(ulKiFastCallEntry))
			{
				return 0;
			}
			if (GetFunctionCodeSize(ulKiFastCallEntry) == GetFunctionCodeSize(ulReloadKiFastCallEntry) &&
				memcmp(ulReloadKiFastCallEntry,ulKiFastCallEntry,GetFunctionCodeSize(ulKiFastCallEntry)) != NULL)
			{
				if (DebugOn)
					KdPrint(("KiFastCallEntry:%08x %x\n",ulKiFastCallEntry,GetFunctionCodeSize(ulKiFastCallEntry)));

				//��ʼɨ��hook
				for (p=ulKiFastCallEntry ;p< ulKiFastCallEntry+0x100; p++)
				{
					//�Ƿ������
					if (*p == 0xcc)
					{
						break;
					}
					ulHookFunctionAddress = (*(PULONG)(p + 1) + (ULONG)p + 5);  //�õ�hook�ĵ�ַ
					if (!MmIsAddressValidEx(ulHookFunctionAddress))
					{
						continue;
					}
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
					if (ulTemp && 
						ulTemp != p &&
						MmIsAddressValidEx(ulTemp))   //hook�ĵ�ַҲҪ��Ч�ſ���Ŷ
					{
						//�õ�����
						ulSize = p - ulKiFastCallEntry;
						ulReloadTemp = ulReloadKiFastCallEntry + ulSize;
						if (MmIsAddressValidEx(ulReloadTemp))
						{
							if (memcmp(ulReloadTemp,p,0x5) == 0){
								continue;
							}
						}
						if (DebugOn)
							KdPrint(("ulTemp:%08x %08x\n",ulTemp,p));

// 						ulTemp = ulTemp+0x5;
// 						//�򵥴���һ�¶�����
// 						if (*ulTemp == 0xe9 ||
// 							*ulTemp == 0xe8)
// 						{
// 							if (DebugOn)
// 								KdPrint(("ulTemp == 0xe9"));
// 							ulTemp = *(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5);
// 						}
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
						SSDTInfo->ulCount = count;
						SSDTInfo->SSDT[count].ulNumber = 0xffffffff;
						SSDTInfo->SSDT[count].ulMemoryFunctionBase = ulTemp + 0x5;
						SSDTInfo->SSDT[count].ulRealFunctionBase = ulKiFastCallEntry;

						memset(SSDTInfo->SSDT[count].lpszFunction,0,sizeof(SSDTInfo->SSDT[count].lpszFunction));
						strncpy(SSDTInfo->SSDT[count].lpszFunction,"KiFastCallEntry",strlen("KiFastCallEntry"));

						memset(SSDTInfo->SSDT[count].lpszHookModuleImage,0,sizeof(SSDTInfo->SSDT[count].lpszHookModuleImage));
						strncpy(SSDTInfo->SSDT[count].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));

						SSDTInfo->SSDT[count].ulHookModuleBase = ulHookModuleBase;
						SSDTInfo->SSDT[count].ulHookModuleSize = ulHookModuleSize;
						SSDTInfo->SSDT[count].IntHookType = INLINEHOOK;

						if (DebugOn)
							KdPrint(("[%d]Found KiFastCallEntry hook!!:%s",SSDTInfo->ulCount,SSDTInfo->SSDT[count].lpszHookModuleImage));

						count++;
						ulTemp = NULL;
					}
				}
			}
		}

	}__except(EXCEPTION_EXECUTE_HANDLER){
		goto _FunctionRet;
	}
_FunctionRet:
	SSDTInfo->ulCount = count;
	return TRUE;
}
//0x1111Ϊȫ��������Ϊ����
BOOL RestoreAllSSDTFunction(ULONG IntType)
{
	PSERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;

	ULONG ulMemoryFunctionBase;
	ULONG ulRealMemoryFunctionBase;
	ULONG ulReloadFunctionBase;

	BOOL bHooked = FALSE;
	int i = 0;
	BOOL bReSetOne = FALSE;

	KeServiceDescriptorTable = GetSystemRoutineAddress(1,L"KeServiceDescriptorTable");
	if (KeServiceDescriptorTable)
	{
		for (i=0 ;i<KeServiceDescriptorTable->TableSize ;i++)
		{
			bHooked = TRUE;
			ulMemoryFunctionBase = KeServiceDescriptorTable->ServiceTable[i];

			ulReloadFunctionBase = *(ULONG *)(OriginalKiServiceTable + i*4);
			ulRealMemoryFunctionBase = ulReloadFunctionBase - (ULONG)ImageModuleBase + SystemKernelModuleBase;

			if (ulMemoryFunctionBase > SystemKernelModuleBase &&
				ulMemoryFunctionBase < SystemKernelModuleBase + SystemKernelModuleSize)
			{
				bHooked = FALSE;
			}
			if (bHooked == TRUE)
			{
				if (!MmIsAddressValidEx(ulRealMemoryFunctionBase))
				{
					continue;
				}
				if (DebugOn)
					KdPrint(("%d[%d]%08x  %08x",IntType,i,OriginalKiServiceTable,ulRealMemoryFunctionBase));

				//��ʼ�ָ�
				//�ָ�ȫ��
				if (IntType == 8888)
				{
					if (DebugOn)
						KdPrint(("reset all[%d:%08x]",i,ulRealMemoryFunctionBase));

					__asm
					{
						cli
							push eax
							mov eax,cr0
							and eax,not 0x10000
							mov cr0,eax
							pop eax
					}
					KeServiceDescriptorTable->ServiceTable[i] = ulRealMemoryFunctionBase;
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
						KeServiceDescriptorTable->ServiceTable[i] = ulRealMemoryFunctionBase;
						__asm
						{
							push eax
								mov eax,cr0
								or eax,0x10000
								mov cr0,eax
								pop eax
								sti
						}
						break;
					}
				}
			}
			if (IntType == 8888)
			{
				//����Ƿ�inline hook
				if (MmIsAddressValidEx(ulReloadFunctionBase) &&
					MmIsAddressValidEx(ulRealMemoryFunctionBase))
				{
					if (memcmp(ulRealMemoryFunctionBase,ulReloadFunctionBase,GetFunctionCodeSize(ulReloadFunctionBase)) != NULL)
					{
						if (GetFunctionCodeSize(ulReloadFunctionBase) != GetFunctionCodeSize(ulRealMemoryFunctionBase))
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
						memcpy(ulRealMemoryFunctionBase,ulReloadFunctionBase,GetFunctionCodeSize(ulReloadFunctionBase));
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
			}else  //�ָ�����
			{
				if (IntType == i)
				{
					//����Ƿ�inline hook
					if (MmIsAddressValidEx(ulReloadFunctionBase) &&
						MmIsAddressValidEx(ulRealMemoryFunctionBase))
					{
						if (memcmp(ulRealMemoryFunctionBase,ulReloadFunctionBase,GetFunctionCodeSize(ulReloadFunctionBase)) != NULL)
						{
							if (GetFunctionCodeSize(ulReloadFunctionBase) != GetFunctionCodeSize(ulRealMemoryFunctionBase))
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
							memcpy(ulRealMemoryFunctionBase,ulReloadFunctionBase,GetFunctionCodeSize(ulReloadFunctionBase));
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
					break;
				}
			}
		}
	}
	return TRUE;
}

BOOL RestoreInlineHook(ULONG ulRealBase,ULONG ulRealModuleBase,ULONG ulReloadModuleBase)
{
	ULONG ulFunction = 0;
	ULONG ulReloadFunction = 0;
	BOOL bInit = FALSE;
	int i=0;
	ULONG ulCodeSize,ulReloadCodeSize;

	if (!MmIsAddressValidEx(ulRealBase))
	{
		return bInit;
	}
	ulFunction = ulRealBase;
	ulReloadFunction = ulFunction - ulRealModuleBase + ulReloadModuleBase;

	if (MmIsAddressValidEx(ulFunction) &&
		MmIsAddressValidEx(ulReloadFunction))
	{
		ulCodeSize = GetFunctionCodeSize(ulFunction);
		ulReloadCodeSize = GetFunctionCodeSize(ulReloadFunction);
		if (ulCodeSize != ulReloadCodeSize)
		{
			//�ų���������������ƹ�hook�����������nop������GetFunctionCodeSize�ͻ���Ϊ�ú����Ѿ���������ʵû��,�������棺
			/*
				lkd> u NtCreateSection
				nt!NtCreateSection:
				805a176c e9fbdf507f      jmp     ffaaf76c      <----A�ܵ��ƹ�����
				805a1771 90              nop
				805a1772 90              nop                   <---������������Ϊ����������ͽ�����
				805a1773 e8289b4c78      call    f8a6b2a0      <---------------��hook��
				805a1778 8b551c          mov     edx,dword ptr [ebp+1Ch]
				805a177b f7c2ffff9fe2    test    edx,0E29FFFFFh
				805a1781 7528            jne     nt!NtCreateSection+0x3f (805a17ab)
				805a1783 f7c20000000d    test    edx,0D000000h
			*/
			if (ulCodeSize == 0x7 && ulReloadCodeSize > ulCodeSize){
				goto Check;
			}
			return FALSE;
		}
Check:
		__asm
		{
			cli
				push eax
				mov eax,cr0
				and eax,not 0x10000
				mov cr0,eax
				pop eax
		}
		memcpy(ulFunction,ulReloadFunction,ulReloadCodeSize);
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
	return TRUE;
}