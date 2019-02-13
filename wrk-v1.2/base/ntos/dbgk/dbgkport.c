/*++

Copyright (c) Microsoft Corporation. All rights reserved. 

You may only use this code if you agree to the terms of the Windows Research Kernel Source Code License agreement (see License.txt).
If you do not agree to the terms, do not use the code.


Module Name:

    dbgkport.c

Abstract:

    This module implements the dbg primitives to access a process'
    DebugPort and ExceptionPort.

--*/

#include "dbgkp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DbgkpSendApiMessage)
#pragma alloc_text(PAGE, DbgkForwardException)
#pragma alloc_text(PAGE, DbgkpSendApiMessageLpc)
#endif


NTSTATUS
DbgkpSendApiMessage(
    IN OUT PDBGKM_APIMSG ApiMsg,
    IN BOOLEAN SuspendProcess
    )

/*++

Routine Description:

    This function sends the specified API message over the specified
    port. It is the caller's responsibility to format the API message
    prior to calling this function.

    If the SuspendProcess flag is supplied, then all threads in the
    calling process are first suspended. Upon receipt of the reply
    message, the threads are resumed.

Arguments:

    ApiMsg - Supplies the API message to send.

    SuspendProcess - A flag that if set to true, causes all of the
        threads in the process to be suspended prior to the call,
        and resumed upon receipt of a reply.

Return Value:

    NTSTATUS.

--*/

{
    NTSTATUS st;
    PEPROCESS Process;

    PAGED_CODE();

    if ( SuspendProcess ) {
        SuspendProcess = DbgkpSuspendProcess();
    }

    ApiMsg->ReturnedStatus = STATUS_PENDING;

    Process = PsGetCurrentProcess();

    PS_SET_BITS (&Process->Flags, PS_PROCESS_FLAGS_CREATE_REPORTED);

    st = DbgkpQueueMessage (Process, PsGetCurrentThread (), ApiMsg, 0, NULL);

    ZwFlushInstructionCache (NtCurrentProcess (), NULL, 0);
    if ( SuspendProcess ) {
        DbgkpResumeProcess();
    }

    return st;
}

/*!
 * \brief DbgkpSendApiMessageLpc ������ϵͳ����,��������Ϣ��0������3���ĵ���������.
 * \param IN OUT PDBGKM_APIMSG ApiMsg : Ҫ���͵ĵ�����Ϣ
 * \param IN PVOID Port	Ҫ���͵���Ŀ��˿�
 * \param IN BOOLEAN SuspendProcess �Ƿ����ǰ����(�����쳣�Ľ���)
 * \retutn NTSTATUS 
 * \sa 
 */
 NTSTATUS
DbgkpSendApiMessageLpc(
    IN OUT PDBGKM_APIMSG ApiMsg,
    IN PVOID Port,
    IN BOOLEAN SuspendProcess
    )

/*++

Routine Description:

    This function sends the specified API message over the specified
    port. It is the caller's responsibility to format the API message
    prior to calling this function.

    If the SuspendProcess flag is supplied, then all threads in the
    calling process are first suspended. Upon receipt of the reply
    message, the threads are resumed.

Arguments:

    ApiMsg - Supplies the API message to send.

    Port - Supplies the address of a port to send the api message.

    SuspendProcess - A flag that if set to true, causes all of the
        threads in the process to be suspended prior to the call,
        and resumed upon receipt of a reply.

Return Value:

    NTSTATUS.

--*/

{
    NTSTATUS st;
    ULONG_PTR MessageBuffer[PORT_TOTAL_MAXIMUM_MESSAGE_LENGTH/sizeof (ULONG_PTR)];


    PAGED_CODE();

    if ( SuspendProcess ) {
		// �������
        SuspendProcess = DbgkpSuspendProcess();
    }

    ApiMsg->ReturnedStatus = STATUS_PENDING;

    PS_SET_BITS (&PsGetCurrentProcess()->Flags, PS_PROCESS_FLAGS_CREATE_REPORTED);

	// ���Ͳ��ȴ�LPC�˿ڵĻظ�.
    st = LpcRequestWaitReplyPortEx (Port,
                    (PPORT_MESSAGE) ApiMsg,
                    (PPORT_MESSAGE) &MessageBuffer[0]);

    ZwFlushInstructionCache(NtCurrentProcess(), NULL, 0);
    if (NT_SUCCESS (st)) {
        RtlCopyMemory(ApiMsg,MessageBuffer,sizeof(*ApiMsg));
    }

    if (SuspendProcess) {
		// �ָ�����ִ��.
        DbgkpResumeProcess();
    }

    return st;
}

DECLSPEC_NOINLINE
BOOLEAN
DbgkForwardException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN DebugException,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    This function is called forward an exception to the calling process's
    debug or subsystem exception port.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    DebugException - Supplies a boolean variable that specifies whether
        this exception is to be forwarded to the process's
        DebugPort(TRUE), or to its ExceptionPort(FALSE).

Return Value:

    TRUE - The process has a DebugPort or an ExceptionPort, and the reply
        received from the port indicated that the exception was handled.

    FALSE - The process either does not have a DebugPort or
        ExceptionPort, or the process has a port, but the reply received
        from the port indicated that the exception was not handled.

--*/

{
    PEPROCESS Process;
    PVOID Port;
    PDBGKM_EXCEPTION args;
    DBGKM_APIMSG m;
    NTSTATUS st;
    BOOLEAN LpcPort;

    PAGED_CODE();

    args = &m.u.Exception;

    //
    // Initialize the debug LPC message with default information.
    //

    DBGKM_FORMAT_API_MSG(m,DbgKmExceptionApi,sizeof(*args));

    //
    // Get the address of the destination LPC port.
    //

    Process = PsGetCurrentProcess();
    if (DebugException) {
		// ����̻߳�����CrossThreadFlags��־�Ƿ���н����̴ӵ��������ص�����.
		// �����,�򽫵��Զ˿���Ϊ��,�������ĺ���ͻ��ǵĽ��̼�ʹ
		// ������,������ϢҲ���ᷢ�͸�������.
        if (PsGetCurrentThread()->CrossThreadFlags&PS_CROSS_THREAD_FLAGS_HIDEFROMDBG) {
            Port = NULL;
        } else {
			// ����ͻ�ȡ��ǰ���̵ĵ��Զ˿�
            Port = Process->DebugPort;
        }
        LpcPort = FALSE;
    } else {
		// ��ȡ���̵��쳣�˿�
        Port = Process->ExceptionPort;
        m.h.u2.ZeroInit = LPC_EXCEPTION;
        LpcPort = TRUE;
    }

    //
    // If the destination LPC port address is NULL, then return FALSE.
    // ����˿�ΪNULL,��ֱ�ӷ���FALSE.
    if (Port == NULL) {
        return FALSE;
    }

    //
    // Fill in the remainder of the debug LPC message.
    // ���쳣��Ϣ���浽��Ϣ�ṹ����(Ҳ��һ���쳣��Ϣ�Ľṹ��)
    args->ExceptionRecord = *ExceptionRecord;
    args->FirstChance = !SecondChance;

    //
    // Send the debug message to the destination LPC port.
    // ����һ��������Ϣ��Ŀ��LPC�˿�

    if (LpcPort) {
        st = DbgkpSendApiMessageLpc(&m,Port,DebugException);
    } else {
        st = DbgkpSendApiMessage(&m,DebugException);
    }


    //
    // If the send was not successful, then return a FALSE indicating that
    // the port did not handle the exception. Otherwise, if the debug port
    // is specified, then look at the return status in the message.
    //

    if (!NT_SUCCESS(st) ||
        ((DebugException) &&
        (m.ReturnedStatus == DBG_EXCEPTION_NOT_HANDLED || !NT_SUCCESS(m.ReturnedStatus)))) {
        return FALSE;

    } else {
        return TRUE;
    }
}

