#include "DriverHips.h"

__declspec(naked) BOOLEAN SeSinglePrivilegeCheckHookZone(,...)
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
		jmp [SeSinglePrivilegeCheckRet];
	}
}
//Ȩ�޼���ʱ�򷵻�ʧ�����ﵽ��ֹ��������
BOOLEAN __stdcall NewSeSinglePrivilegeCheck(
	__in  LUID PrivilegeValue,
	__in  KPROCESSOR_MODE PreviousMode
	)
{
	ULONG ulPage;

	if (!bIsInitSuccess)
		goto _FunctionRet;

	//ȡ���ص�ַ
	_asm
	{
		mov eax,dword ptr[ebp+4]
		mov ulPage,eax
	}
	if (RPsGetCurrentProcess() == ProtectEProcess){
		goto _FunctionRet;
	}
	//�����ں˰�ȫģʽ�����жϵ���Reload��ĵ�ַ
	if (bKernelSafeModule){
		if (ulPage >= ulReloadNtLoadDriverBase && ulPage <= ulReloadNtLoadDriverBase+ulNtLoadDriverSize)
			return FALSE;

		if (ulPage >= ulReloadZwSetSystemInformationBase && ulPage <= ulReloadZwSetSystemInformationBase+ulZwSetSystemInformationSize)
			return FALSE;
	}else{

		//û�п����ں˰�ȫģʽ�����жϵ���ԭʼ��ַ
		if (ulPage >= ulNtLoadDriverBase && ulPage <= ulNtLoadDriverBase+ulNtLoadDriverSize)
			return FALSE;

		if (ulPage >= ulZwSetSystemInformationBase && ulPage <= ulZwSetSystemInformationBase+ulZwSetSystemInformationSize)
			return FALSE;
	}

_FunctionRet:
	OldSeSinglePrivilegeCheck = (SeSinglePrivilegeCheck_1)SeSinglePrivilegeCheckHookZone;
	return OldSeSinglePrivilegeCheck(
		PrivilegeValue,
		PreviousMode
		);
}
//��ֹ��������
NTSTATUS DisEnableDriverLoading()
{
	int bRet;

	ulZwSetSystemInformationBase = GetSystemRoutineAddress(1,L"ZwSetSystemInformation");
	ulNtLoadDriverBase = GetSystemRoutineAddress(1,L"ZwLoadDriver");
	if (ulNtLoadDriverBase &&
		ulZwSetSystemInformationBase)
	{
		ulNtLoadDriverSize = SizeOfProc(ulNtLoadDriverBase);
		ulZwSetSystemInformationSize = SizeOfProc(ulZwSetSystemInformationBase);
	}

	ulSeSinglePrivilegeCheck = GetSystemRoutineAddress(1,L"SeSinglePrivilegeCheck");
	if (!ulSeSinglePrivilegeCheck ||
		!ulNtLoadDriverBase ||
		!ulZwSetSystemInformationBase)
	{
		return STATUS_UNSUCCESSFUL;
	}
	//����������ں˰�ȫģʽ����Ҫ����reload��ĵ�ַ����Ȼ�жϲ���
	ulReloadNtLoadDriverBase = ulNtLoadDriverBase - SystemKernelModuleBase+ImageModuleBase;
	ulReloadZwSetSystemInformationBase = ulZwSetSystemInformationBase - SystemKernelModuleBase+ImageModuleBase;

	ulReloadSeSinglePrivilegeCheck = ulSeSinglePrivilegeCheck - SystemKernelModuleBase+ImageModuleBase;

	//hook reload SeSinglePrivilegeCheck

	bRet = HookFunctionByHeaderAddress(ulReloadSeSinglePrivilegeCheck,ulSeSinglePrivilegeCheck,SeSinglePrivilegeCheckHookZone,&SeSinglePrivilegeCheckPatchCodeLen,&SeSinglePrivilegeCheckRet);
	if(bRet)
	{
		bRet = FALSE;
		bRet = HookFunctionByHeaderAddress(
			NewSeSinglePrivilegeCheck,
			ulReloadSeSinglePrivilegeCheck,
			SeSinglePrivilegeCheckHookZone,
			&SeSinglePrivilegeCheckPatchCodeLen,
			&SeSinglePrivilegeCheckRet
			);
		if (bRet)
		{
			SeSinglePrivilegeCheckHooked = TRUE;
			//DbgPrint("hook SeSinglePrivilegeCheck success\n");
		}
	}
	return STATUS_SUCCESS;
}
//������������
NTSTATUS EnableDriverLoading()
{
	if (SeSinglePrivilegeCheckHooked == TRUE)
	{
		SeSinglePrivilegeCheckHooked = FALSE;
		UnHookFunctionByHeaderAddress((DWORD)ulReloadSeSinglePrivilegeCheck,SeSinglePrivilegeCheckHookZone,SeSinglePrivilegeCheckPatchCodeLen);
		UnHookFunctionByHeaderAddress((DWORD)ulSeSinglePrivilegeCheck,SeSinglePrivilegeCheckHookZone,SeSinglePrivilegeCheckPatchCodeLen);
	}
}
//***************************************
//����һ��ImageNotify������ص�����
//***************************************
VOID ImageNotify(
	PUNICODE_STRING  FullImageName,
	HANDLE  ProcessId,
	PIMAGE_INFO  ImageInfo
	)
{
	//�ų�������ģ��ļ���
	if (ProcessId != 0 || PsGetCurrentProcessId() != 4)
	{
		return;
	}
	//����Ƿ��Ǽ�������
	if (ImageInfo->ImageBase < MmUserProbeAddress)
	{
		return;
	}
	//���UNICODE_STRING�Ƿ���Է���
	if (!ValidateUnicodeString(FullImageName))
	{
		return;
	}
	//KdPrint(("%d:%08x --> %ws\r\n",PsGetCurrentProcessId(),ImageInfo->ImageBase,FullImageName->Buffer));

	if (LogDefenseInfo->ulCount < 1000 &&
		ulLogCount < 1000)   //��¼����1000�����򲻼�¼��
	{
		LogDefenseInfo->LogDefense[ulLogCount].ulPID = ImageInfo->ImageSize;
		LogDefenseInfo->LogDefense[ulLogCount].Type = 6;  //��������
		LogDefenseInfo->LogDefense[ulLogCount].EProcess = ImageInfo->ImageBase;  //������ַ

		memset(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,0,sizeof(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));
		memset(LogDefenseInfo->LogDefense[ulLogCount].lpwzMoreEvents,0,sizeof(LogDefenseInfo->LogDefense[ulLogCount].lpwzMoreEvents));

		SafeCopyMemory(FullImageName->Buffer,LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,FullImageName->Length);
		ulLogCount++;
	}
	return;
}