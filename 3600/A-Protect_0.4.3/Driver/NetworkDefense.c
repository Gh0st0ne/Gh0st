#include "NetworkDefense.h"

//���ļ�·����ȡ�ļ���
BOOL GetFileName(__in WCHAR *FilePath,__in int len,__out WCHAR *FileName)
{
	int i=0;
	WCHAR lpPath[260*2];
	BOOL bRetOK = FALSE;

	//wcslen(L"x:\\")*2
	//Ч��Ϸ��ĳ��ȣ���Ȼ�������©����Ҫ����Ŷ
	if (len < 6 || len > 260)
		return bRetOK;

	memset(lpPath,0,sizeof(lpPath));
	memset(FileName,0,sizeof(FileName));

	memcpy(lpPath,FilePath,len);
	for(i=0;i<len;i++)
	{
		if (wcsstr(lpPath,L"\\") == 0)
		{
			bRetOK = TRUE;

			//FileName����󳤶�������lpPath��ʵ�ʳ��ȣ���Ȼ������
			if (sizeof(FileName) > wcslen(lpPath))
			{
				memcpy(FileName,lpPath,wcslen(lpPath)*2);
			}
			break;
		}
		memset(lpPath,0,sizeof(lpPath));
		memcpy(lpPath,FilePath+i,wcslen(FilePath+i)*2);
	}
	return bRetOK;
}

ULONG CheckExeFileOrDllFileBySectionHandle(HANDLE hSection)
{
	NTSTATUS status;
	PVOID BaseAddress = NULL;
	SIZE_T size=0;
	PIMAGE_NT_HEADERS PImageNtHeaders;

	if (!hSection)
		return NULL;

	ReLoadNtosCALL(&RZwMapViewOfSection,L"ZwMapViewOfSection",SystemKernelModuleBase,ImageModuleBase);
	if (!RZwMapViewOfSection)
		return NULL;

	status = RZwMapViewOfSection(
		hSection, 
		NtCurrentProcess(),
		&BaseAddress, 
		0,
		1000, 
		0,
		&size,
		(SECTION_INHERIT)1,
		MEM_TOP_DOWN, 
		PAGE_READWRITE
		); 
	if(NT_SUCCESS(status))
	{
		if (DebugOn)
			KdPrint(("ZwMapViewOfSection success"));

		PImageNtHeaders = RtlImageNtHeader(BaseAddress);
		if (PImageNtHeaders)
		{
			if (DebugOn)
				KdPrint(("Characteristics:%08x\r\n",PImageNtHeaders->FileHeader.Characteristics));

			return PImageNtHeaders->FileHeader.Characteristics;
		}
	}
	return NULL;
}
////////////////////////////////////////////////
/*
ZwCreateSection hook, DKOM type.
*/
NTSTATUS _stdcall NewZwCreateSection(
	__out     PHANDLE SectionHandle,
	__in      ACCESS_MASK DesiredAccess,
	__in_opt  POBJECT_ATTRIBUTES ObjectAttributes,
	__in_opt  PLARGE_INTEGER MaximumSize,
	__in      ULONG SectionPageProtection,
	__in      ULONG AllocationAttributes,
	__in_opt  HANDLE FileHandle)
{
	NTSTATUS status;
	PEPROCESS Eprocess;
	PFILE_OBJECT FileObject;
	PVOID object_temp;
	POBJECT_HEADER ObjectHeader;
	POBJECT_TYPE FileObjectType;
	WIN_VER_DETAIL WinVer;
	BOOL bRetOK = FALSE;
	BOOL bInherited = FALSE;
	int i;
	WCHAR *lpwzExeFile = NULL;
	WCHAR *lpwzExeNtFile = NULL;
	KPROCESSOR_MODE PreviousMode;
	UNICODE_STRING UnicodeDNSAPI_DLL;
	UNICODE_STRING UnicodeExeNtFilePath;
	UNICODE_STRING UnicodeFunction;
	char *lpszProName = NULL;
	BOOL bNetworkDefence = FALSE;
	BOOL bInitAPISuccess = FALSE;
	POBJECT_NAME_INFORMATION DosFullPath=NULL;
	ULONG ulExeFileCharacteristics,ulDllFileCharacteristics;
	ULONG ulIsExeDllModule;
	STRING lpszProString;
	STRING lpszSvchostString;
	STRING lpszWinlogonString;
	STRING lpszServicesString;
	STRING lpszCmdString;
	STRING lpszExplorer;
	WCHAR lpwzDirFile[260];
	WCHAR FileName[260*2];
	WCHAR SystemFile[260];
	BOOL bIsInjectDllInto3600 = FALSE;
	ULONG ulPathSize;
	ZWCREATESECTION OldZwCreateSection;

	ReLoadNtosCALL(&RObReferenceObjectByHandle,L"ObReferenceObjectByHandle",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RPsGetCurrentProcessId,L"PsGetCurrentProcessId",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RPsGetProcessImageFileName,L"PsGetProcessImageFileName",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RZwClose,L"ZwClose",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RExAllocatePool,L"ExAllocatePool",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RExFreePool,L"ExFreePool",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RIoQueryFileDosDeviceName,L"IoQueryFileDosDeviceName",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RRtlCompareString,L"RtlCompareString",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RRtlInitString,L"RtlInitString",SystemKernelModuleBase,ImageModuleBase);
	ReLoadNtosCALL(&RRtlCompareUnicodeString,L"RtlCompareUnicodeString",SystemKernelModuleBase,ImageModuleBase);
	if (RPsGetCurrentProcess &&
		RObReferenceObjectByHandle &&
		RPsGetCurrentProcessId &&
		RPsGetProcessImageFileName &&
		RZwClose &&
		RExAllocatePool &&
		RExFreePool &&
		RIoQueryFileDosDeviceName &&
		RRtlCompareString &&
		RRtlInitString &&
		RRtlCompareUnicodeString)
	{
		bInitAPISuccess = TRUE;
	}
	if (!bInitAPISuccess){
		return STATUS_UNSUCCESSFUL;
	}
	if (bDisCreateProcess == FALSE)  //��ֹ��������
	{
		//�����Լ���
		if (IsExitProcess(ProtectEProcess)){
			if (RPsGetCurrentProcess() != ProtectEProcess){
				return STATUS_UNSUCCESSFUL;
			}
		}
	}
	//���飬���ɨ���ʱ������OriginalServiceDescriptorTable->ServiceTable[ZwCreateSectionIndex]����������
	//��ԭʼϵͳ��ȴû�£����갡���鰡~��
	//OldZwCreateSection = OriginalServiceDescriptorTable->ServiceTable[ZwCreateSectionIndex];
	OldZwCreateSection = KeServiceDescriptorTable->ServiceTable[ZwCreateSectionIndex];
	status = OldZwCreateSection(
		SectionHandle,
		DesiredAccess,
		ObjectAttributes,
		MaximumSize,
		SectionPageProtection,
		AllocationAttributes,
		FileHandle
		);
	if (!NT_SUCCESS(status)){
		return status;
	}
	//��ʼ��OK
	if (!bIsInitSuccess){
		return status;
	}
	if (KeGetCurrentIrql() != PASSIVE_LEVEL){
		return status;
	}
	if ((AllocationAttributes == 0x1000000) && (SectionPageProtection & PAGE_EXECUTE))
	{
		if (!ARGUMENT_PRESENT(FileHandle)){
			return status;
		}
		PreviousMode = KeGetPreviousMode();
		if (PreviousMode != KernelMode)
		{
			__try{
				ProbeForRead(FileHandle,sizeof(HANDLE),sizeof(ULONG));
			}__except (EXCEPTION_EXECUTE_HANDLER) {
				goto _FunctionRet;
			}
		}
		status = RObReferenceObjectByHandle(
			FileHandle,
			NULL,
			*IoFileObjectType,
			KernelMode,
			(PVOID *)&object_temp,
			NULL);
		if (!NT_SUCCESS(status))
		{
			//�ָ��������ֵ
			status = STATUS_SUCCESS;

			goto _FunctionRet;
		}
		ObDereferenceObject(object_temp);  //������ö���

		Eprocess = RPsGetCurrentProcess();
		//���������������object_temp���ж�type�Ÿ�׼ȷ~��
		WinVer = GetWindowsVersion();
		switch (WinVer)
		{
		case WINDOWS_VERSION_XP:
		case WINDOWS_VERSION_2K3_SP1_SP2:
			ObjectHeader = OBJECT_TO_OBJECT_HEADER(object_temp);
			FileObjectType = ObjectHeader->Type;
			break;
		case WINDOWS_VERSION_7_7600_UP:
		case WINDOWS_VERSION_7_7000:
			RtlInitUnicodeString(&UnicodeFunction,L"ObGetObjectType");
			MyGetObjectType=(OBGETOBJECTTYPE)MmGetSystemRoutineAddress(&UnicodeFunction);  //xp~2008���޴˺��������ֱ�ӵ��ã�������������ʧ�ܣ������Ҫ��̬��ȡ��ַ
			//MyGetObjectType = GetSystemRoutineAddress(1,L"ObGetObjectType");
			if(MyGetObjectType)
			{
				FileObjectType = MyGetObjectType((PVOID)object_temp);
			}
			break;
		}
		if (FileObjectType != *IoFileObjectType)
		{
			goto _FunctionRet;
		}
		FileObject = (PFILE_OBJECT)object_temp;
		//KdPrint(("FileObject --> %ws\n",FileObject->FileName.Buffer));

		if (MmIsAddressValidEx(FileObject) &&
			ValidateUnicodeString(&FileObject->FileName))
		{
			ulIsExeDllModule = 0;
			//xp/2003
			//0x10f = ExeFile
			//0x210e = DllFile

			//win7 ��ֻ�ܵõ����ص�dll���ò���exe���Ͳ�Ҫwin7��
			WinVer = GetWindowsVersion();
			switch (WinVer)
			{
			case WINDOWS_VERSION_XP:
			case WINDOWS_VERSION_2K3_SP1_SP2:
				ulExeFileCharacteristics = 0x10f;
				ulDllFileCharacteristics = 0x2102;
				ulIsExeDllModule = CheckExeFileOrDllFileBySectionHandle(*SectionHandle);
				break;
			}
			RRtlInitUnicodeString(&UnicodeDNSAPI_DLL,L"\\windows\\system32\\DNSapi.DLL");
			if (RRtlCompareUnicodeString(&FileObject->FileName,&UnicodeDNSAPI_DLL,TRUE) == 0)
			{
				if (LogDefenseInfo->ulCount < 1000)   //��¼����1000�����򲻼�¼��
				{
					LogDefenseInfo->LogDefense[ulLogCount].EProcess = RPsGetCurrentProcess();
					LogDefenseInfo->LogDefense[ulLogCount].ulPID = RPsGetCurrentProcessId();
					LogDefenseInfo->LogDefense[ulLogCount].Type = 2;
					ulLogCount++;
				}
			}
			//------------------------------------------------
			//DLLЮ�ֵķ���
			//KdPrint(("DLL --> [%x] %ws\n",ulIsExeDllModule,FileObject->FileName.Buffer));

			if (bDisDllFuck &&   //����DLLЮ�֣����û�����
				ulIsExeDllModule == ulDllFileCharacteristics &&
				IsExitProcess(ProtectEProcess) &&  //A�ܳ�ʼ����֮�󣬲ſ�ʼ����Ȼ���ɨ��ϵͳ����֮���޷�����A��
				Eprocess != ProtectEProcess &&  //�ų�A���Լ�Ŷ
				ulIsExeDllModule)        //�ų�win7
			{

				memset(lpwzDirFile,0,sizeof(lpwzDirFile));
				//������ȳ���Ŀ¼���ַ�
				if (wcslen(L"\\windows\\system")*2 > FileObject->FileName.Length)
					ulPathSize = FileObject->FileName.Length;
				else
					ulPathSize = wcslen(L"\\windows\\system")*2;

				memcpy(lpwzDirFile,FileObject->FileName.Buffer,ulPathSize);
				if (_wcsnicmp(lpwzDirFile,L"\\windows\\system",wcslen(L"\\windows\\system")) != 0 &&
					_wcsnicmp(lpwzDirFile,L"\\windows\\WinSxS",wcslen(L"\\windows\\WinSxS")) != 0)  //WinSxS��������пؼ�dll��Ҫ�Ź�
				{
					//�����ǰĿ¼����system32������system32Ŀ¼���Ƿ��и���ǰ·��һ����ͬ���ֵ��ļ����������dllЮ�֣�
					if (GetFileName(FileObject->FileName.Buffer,FileObject->FileName.Length,FileName))
					{
						memset(SystemFile,0,sizeof(SystemFile));
						wcscat(SystemFile,L"\\SystemRoot\\system32\\");
						wcscat(SystemFile,FileName);   //

						if (IsFileInSystem(SystemFile))
						{
							if (DebugOn)
								KdPrint(("%ws  <-->  %ws\n",lpwzDirFile,SystemFile));

							//DLLЮ�֣�
							LogDefenseInfo->LogDefense[ulLogCount].Type = 4; //DLLЮ��

							memset(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,0,sizeof(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));
							SafeCopyMemory(FileObject->FileName.Buffer,LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,FileObject->FileName.Length);
							LogDefenseInfo->LogDefense[ulLogCount].EProcess = RPsGetCurrentProcess();
							LogDefenseInfo->LogDefense[ulLogCount].ulPID = RPsGetCurrentProcessId();
							ulLogCount++;

							//dllЮ�֣�ֱ��ɱ��~
							RZwClose(*SectionHandle);
							return STATUS_UNSUCCESSFUL;
						}
					}
				}
			}
			//------------------------------------------------
			//��¼������Щ������Ϊ�����̴����ӽ��̵���Ϊ
			lpszProName = (char *)RPsGetProcessImageFileName(Eprocess);
			RRtlInitString(&lpszProString,lpszProName);

			RRtlInitString(&lpszSvchostString,"svchost.exe");
			RRtlInitString(&lpszWinlogonString,"winlogon.exe");
			RRtlInitString(&lpszServicesString,"services.exe");
			RRtlInitString(&lpszCmdString,"cmd.exe");
			RRtlInitString(&lpszExplorer,"explorer.exe");

			if (RRtlCompareString(&lpszSvchostString,&lpszProString,TRUE) == 0 ||
				RRtlCompareString(&lpszWinlogonString,&lpszProString,TRUE) == 0 ||
				RRtlCompareString(&lpszServicesString,&lpszProString,TRUE) == 0 ||
				RRtlCompareString(&lpszCmdString,&lpszProString,TRUE) == 0 ||
				RRtlCompareString(&lpszExplorer,&lpszProString,TRUE) == 0)
			{
				if (LogDefenseInfo->ulCount < 1000 &&
					ulLogCount < 1000)   //��¼����1000�����򲻼�¼��
				{
					if (FileObject->FileName.Buffer != NULL &&
						FileObject->FileName.Length >30 &&
						RIoQueryFileDosDeviceName(FileObject,&DosFullPath) == STATUS_SUCCESS)
					{
						ulPathSize = DosFullPath->Name.Length;

						lpwzExeFile = RExAllocatePool(NonPagedPool,ulPathSize);
						if (!lpwzExeFile)
						{
							if (DosFullPath)
								RExFreePool(DosFullPath);
							goto _FunctionRet;
						}
						memset(lpwzExeFile,0,ulPathSize);
						SafeCopyMemory(DosFullPath->Name.Buffer,lpwzExeFile,ulPathSize);

						if (DosFullPath)
							RExFreePool(DosFullPath);

						//KdPrint(("EXE --> [%x]%s %ws\n",ulIsExeDllModule,lpszProName,DosFullPath->Name.Buffer));

						//�ų�dll��ֻҪexe·�����ַ���
						if (ulIsExeDllModule == ulExeFileCharacteristics)        //�ų�win7
						{
							__try
							{
								LogDefenseInfo->LogDefense[ulLogCount].Type = 3;

								memset(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,0,sizeof(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));
								SafeCopyMemory(lpwzExeFile,LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,ulPathSize);

								if (DebugOn)
									KdPrint(("ExePath:%ws\r\n",LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));

								LogDefenseInfo->LogDefense[ulLogCount].EProcess = RPsGetCurrentProcess();
								LogDefenseInfo->LogDefense[ulLogCount].ulPID = RPsGetCurrentProcessId();
								ulLogCount++;

							}__except (EXCEPTION_EXECUTE_HANDLER) {

							}

						}
						//��������svchost��dll����
						if (RRtlCompareString(&lpszSvchostString,&lpszProString,TRUE) == 0)
						{
							__try
							{
								memset(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,0,sizeof(LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));
								SafeCopyMemory(lpwzExeFile,LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess,ulPathSize);

								if (DebugOn)
									KdPrint(("DLLPath:%ws\r\n",LogDefenseInfo->LogDefense[ulLogCount].lpwzCreateProcess));

								LogDefenseInfo->LogDefense[ulLogCount].EProcess = RPsGetCurrentProcess();
								LogDefenseInfo->LogDefense[ulLogCount].ulPID = RPsGetCurrentProcessId();
								LogDefenseInfo->LogDefense[ulLogCount].Type = 3;
								ulLogCount++;

							}__except (EXCEPTION_EXECUTE_HANDLER) {

							}
						}
						if (lpwzExeFile)
							RExFreePool(lpwzExeFile);
					}
				}
			}
		}
	}
_FunctionRet:
	return status;
}
BOOL InitNetworkDefence()
{
	if (SystemCallEntryTableHook(
		"ZwCreateSection",
		&ZwCreateSectionIndex,
		NewZwCreateSection) == TRUE)
	{
		if (DebugOn)
			KdPrint(("Create Control Thread success 3\r\n"));
	}
}