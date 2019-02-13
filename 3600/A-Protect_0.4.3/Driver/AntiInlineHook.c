#include "AntiInlineHook.h"

/*
__declspec(naked) VOID __stdcall NewHookFunctionProcess()
{
	_asm
	{
		jmp [ulReloadAddress];   //ֱ��������reload�ĺ�����ַȥ~~������
	}
}
*/
//�������ǰ�����ѣ�
__declspec(naked) VOID HookFunctionProcessHookZone(,...)
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
		jmp [JmpFunctionAddress];
	}
}
/*

ͨ��Hook��ת��reload�������Կ�inline hook

*/
VOID AntiInlineHook(ULONG ulRealBase,ULONG ulModuleBase,ULONG ulReloadModuleBase)
{
	ULONG ulTemp = NULL;
	PUCHAR p;
	BOOL bIsHooked = FALSE;
	INSTRUCTION	Inst;
	INSTRUCTION	Instb;
	BOOL bInit = FALSE;
	ULONG ulHookFunctionAddress;
	ULONG JmpReloadFunctionAddress;
	int i=0;
	BOOL bIsCallHook = FALSE;
	__try
	{
		
// 		JmpFunctionAddress = GetSystemRoutineAddress(1,FunctionName);  //�õ�������ַ
// 
// 		if (DebugOn)
// 			KdPrint(("Get System Routine Address:%ws:%08x\r\n",FunctionName,JmpFunctionAddress));

		JmpFunctionAddress = ulRealBase;
		JmpReloadFunctionAddress = JmpFunctionAddress - ulModuleBase + ulReloadModuleBase;

		if (MmIsAddressValidEx(JmpFunctionAddress) &&
			MmIsAddressValidEx(JmpReloadFunctionAddress))
		{
			if (GetFunctionCodeSize(JmpFunctionAddress) == GetFunctionCodeSize(JmpReloadFunctionAddress) &&
				memcmp(JmpFunctionAddress,JmpReloadFunctionAddress,GetFunctionCodeSize(JmpFunctionAddress)) != NULL)
			{
				//KdPrint(("---->%s:%08x",functionName,ulOldAddress));
				//��ʼɨ��hook
				for (p=JmpFunctionAddress ;p< JmpFunctionAddress+GetFunctionCodeSize(JmpFunctionAddress); p++)
				{
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
						bIsCallHook = TRUE;
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
						MmIsAddressValidEx(ulTemp))
					{
						if (ulTemp > SystemKernelModuleBase &&
							ulTemp < SystemKernelModuleBase+SystemKernelModuleSize)   //̫������Ҳ����
						{
							continue;
						}
						//ulTempҲ����С�� SystemKernelModuleBase
						if (ulTemp < SystemKernelModuleBase)
						{
							continue;
						}
						if (*(ULONG *)ulTemp == 0x00000000 ||
							*(ULONG *)ulTemp == 0x00000005)
						{
							continue;
						}
						if (ulTemp > ulMyDriverBase &&
							ulTemp < ulMyDriverBase + ulMyDriverSize)
						{
							if (DebugOn)
								KdPrint(("my hook, denied access��"));
							return;
						}
						if (ulTemp > ImageModuleBase &&
							ulTemp < ImageModuleBase + SystemKernelModuleSize)
						{
							if (DebugOn)
								KdPrint(("new kernel hook, denied access��"));
							return;
						}
						//�����call hook����Hook��ǰ��ַ��ͷ����~~������
						if (bIsCallHook)
						{
							if (DebugOn)
								KdPrint(("the hook is a call hook!\n"));

							HookFunctionByHeaderAddress(
								(DWORD)JmpReloadFunctionAddress,
								JmpFunctionAddress,
								(PVOID)HookFunctionProcessHookZone,
								&HookFunctionProcessPatchCodeLen,
								&HookFunctionProcessRet
								);
							return;
						}
						ulRunAddress = (ULONG)p - (ULONG)JmpFunctionAddress;   //ִ�е���hook���ʱ��һ��ִ���˶��ٳ��ȵĴ���
						JmpReloadFunctionAddress = JmpReloadFunctionAddress + ulRunAddress;     //����ǰ��ִ�еĴ��룬��������ִ�� 

						if (DebugOn)
							KdPrint(("found hook---->%08x:%08x-%x-%08x",ulTemp + 0x5,p,ulRunAddress,JmpReloadFunctionAddress));


						//�õ���ȷ����ת��ַ��ֱ��hook�˼ҵ�hook������ͷ����Ȼ����������reload�����ulReloadAddress��ַ������ִ��ʣ�µĴ��룬�������ƹ�hook��
						ulTemp = ulTemp + 0x5;

						HookFunctionByHeaderAddress(
							(DWORD)JmpReloadFunctionAddress,
							ulTemp,
							(PVOID)HookFunctionProcessHookZone,
							&HookFunctionProcessPatchCodeLen,
							&HookFunctionProcessRet
							);
					}
				}
			}
		}

	}__except(EXCEPTION_EXECUTE_HANDLER){

	}
	return;
}