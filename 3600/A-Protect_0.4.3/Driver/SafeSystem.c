/*
д��ǰ��Ļ���

���YYYͬѧ�ڷ���ĳһ�������ʱ�򣬱�XXX��Ϊ����͵ZZZ�Ĵ��룬Ȼ��XXX�ͺܲ�ˬ��������YYYͬѧ����Ϣ����������ľ�����
����˵���ǣ�����Ա�ǵ��£�˭д���벻��A��Aȥ�ġ�
XXXͬѧûA����ZZZͬѧ��û���ù�ctrl+c��ctrl+v�𣿻������˼ҵĸ�����Ϣ�������ַ���˽����Ϊ��ʮ����Ľ�����������ʦ�ˡ�

�������ڵĻ����治�ʺ�free��open��share��

�Һܴ󷽵ĳ��ϣ�A�ܵĴ���98%����A�ģ����ڴ���ų������Ͻ��������~~

PS���İ�Ȩ��ͬѧ�Ժ������׶��Ǵ��׵�~

�м������ⷢemail��hack.x86.asm@gmail.com
or QQ:136618866

*/
#include "SafeSystem.h"

VOID DriverUnload(
	IN PDRIVER_OBJECT		DriverObject
	)
{
	KdPrint(("Driver Unload Called\n"));

}
VOID IsKernelBooting(IN PVOID Context)
{
	NTSTATUS status;
	PUCHAR KiFastCallEntry;
	ULONG EProcess;
	int i=0;
	ULONG ImageBase;

	if (PsGetProcessCount() <= 2)
		bKernelBooting = TRUE;
	else 
		goto _InitThread;  //ϵͳû�иո�������������

	while (1)
	{
		//ϵͳ�ո�����
		if (bKernelBooting){

			//�ȴ���ʼ���ɹ�
			//��ʼ���ɹ���Ϊ����������
			//1��ע����ʼ�����ˣ����Է��ʡ�д��
			//2���ļ�ϵͳ��ʼ�����ˣ����Է����ļ���д���ļ�
			//3���û��ռ���̻����Ѿ���ʼ������      <== �����������ж���bug�������жϲ��ǱȽ�׼ȷ��

			if (PsGetProcessCount() >= 3){
				break;
			}
		}
		WaitMicroSecond(88);
	}
	/*
		ϵͳ�ո�������ִ�е��������˵�������ɨ�衣�Ϳ�ʼ���
		��������ɨ�裬�Ϳ����������
		ԭ��д��KeBugCkeckֵ��ring3������ʱ��ɾ�������ֵ�����������ɹ�������
		����´�������ʱ����KeBugCkeck���ڣ���˵���������ɹ�����ֱ�ӷ��أ������κβ���
		������������ֵ��˵���������ɹ���ֱ�ӷ��أ������κ�hook
	*/
	if (IsRegKeyInSystem(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\KeBugCheck")){
		/*

			�������������ܵ���PsTerminateSystemThread�����߳�
			Ӧ�����߳��Լ�����

		*/
		return;
	}
	//д��һ��ֵ���ڽ�������֮��ɾ�������������ɹ���
	//���ִ�е����˵���ȶ����в�������
	KeBugCheckCreateValueKey(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\KeBugCheck");

_InitThread:
	_asm
	{
		pushad;
		mov ecx, 0x176;
		rdmsr;
		mov KiFastCallEntry, eax;
		popad;
	}
	/*
	  
	  ��ֹ�ظ�����
	  ��ע�����ע�Ὺ������������U�̾ͻ�ڶ��μ���
	*/
	if (*KiFastCallEntry == 0xe9)
	{
		KdPrint(("Terminate System Thread"));

		/*

			�������������ܵ���PsTerminateSystemThread�����߳�
			Ӧ�����߳��Լ�����

		*/
		return;
	}
	if (ReLoadNtos(PDriverObject,RetAddress) == STATUS_SUCCESS)
	{
		//����������صı������������������ɨ��
		PsSetLoadImageNotifyRoutine(ImageNotify);
		//*******************************************************************
		//test
		//*******************************************************************
		//ImageBase = 0xF83E2000;
		//IatHookCheck(&ImageBase);
		//MsGetMsgHookInfo();
		//*******************************************************************
		//---------------------------------------
		//demo����ȷ���ɨ������
		//---------------------------------------
		if (bKernelBooting)
		{
			DepthServicesRegistry = (PSERVICESREGISTRY)ExAllocatePool(NonPagedPool,sizeof(SERVICESREGISTRY)*1024);
			if (DepthServicesRegistry)
			{
				memset(DepthServicesRegistry,0,sizeof(SERVICESREGISTRY)*1024);
				status = QueryServicesRegistry(DepthServicesRegistry);
				if (status == STATUS_SUCCESS)
				{
// 					Safe_CreateValueKey(
// 						L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\A-Protect",
// 						REG_SZ,
// 						L"QueryServicesRegistry",
// 						L"success");
				}
			}
		}
	}
	bKernelBooting = FALSE;
}
NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING theRegistryPath )
{
	ULONG ulSize;
	ULONG ulKeServiceDescriptorTable;
	int i = 0;
	HANDLE HThreadHandle;
	HANDLE ThreadHandle;

	DriverObject->DriverUnload = DriverUnload;

	PDriverObject = DriverObject;

	RetAddress=*(DWORD*)((DWORD)&DriverObject-4);

	ulMyDriverBase = DriverObject->DriverStart;
	ulMyDriverSize = DriverObject->DriverSize;

	DebugOn = FALSE;  //������ʽ��Ϣ

	KdPrint(("//***************************************//\r\n"
	       	"//   A-Protect Anti-Rootkit Kernel Module  //\r\n"
			"//   Kernel Module Version LE 2012-0.4.3  //\r\n"
		     "//  website:http://www.3600safe.com       //\r\n"
	         "//***************************************//\r\n"));

	SystemEProcess = PsGetCurrentProcess();

	WinVersion = GetWindowsVersion();  //��ʼ��ϵͳ�汾
	if (WinVersion)
		KdPrint(("Init Windows version Success\r\n"));

	DepthServicesRegistry = NULL;
	//-----------------------------------------
	//����һ��ϵͳ�߳�������
	//-----------------------------------------
	if (PsCreateSystemThread(
		&HThreadHandle,
		0,
		NULL,
		NULL,
		NULL,
		IsKernelBooting,
		NULL) == STATUS_SUCCESS)
	{
		ZwClose(HThreadHandle);
	}
	return STATUS_SUCCESS;
}