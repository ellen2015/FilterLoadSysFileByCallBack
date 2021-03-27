#include <ntddk.h>
#include <ntstrsafe.h>

#define DUMP_FILE_TAG 'pmuD'


#define uSize (260*2)
///
///  ��ȡָ��·�����ļ�
///
NTSTATUS
MyCopyFile(IN PUNICODE_STRING Path)
{
	// DbgBreakPoint();
	NTSTATUS status = STATUS_SUCCESS;
	HANDLE hFile = NULL;
	HANDLE hWriteFile = NULL;
	OBJECT_ATTRIBUTES oa;
	InitializeObjectAttributes(&oa,
		Path,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);
	IO_STATUS_BLOCK ioStatusBlock = { 0 };

	// ��ȡ�ļ����
	status = ZwCreateFile(&hFile,	//���ָ��
		0x81,
		&oa,						// OA
		&ioStatusBlock,				// io״̬
		NULL,						// һ����NULL
		FILE_ATTRIBUTE_NORMAL,		// �ļ�����һ��дNORMAL
		FILE_SHARE_READ,			// �ļ������ԣ�
									// һ��ֻ��дFILE_SHARE_READ���У�
									// ���������������Ҫд��0x7Ҳ����ȫ����ģʽ
		FILE_OPEN,//���ļ�������ʱ������ʧ��
		FILE_SYNCHRONOUS_IO_NONALERT, // �ļ���Ŀ¼����
									 // �����ļ�����ֱ��д���ļ�ϵͳ��ֱ��д�벻�����������ʱ���⣬��IOռ�úࣩܶ
		NULL, //EA������дNULL�������ļ��������������豸�Ľ���������EAдNULL,EA����Ҳ��0
		0);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("ZwCreateFile failed in %s in %d line error code : %08x\n", __FUNCTION__, __LINE__, status);
		return status;
	}

	// �����ļ�����ļ���ȡ�ļ���С
	SIZE_T uFileSize = 0;
	FILE_STANDARD_INFORMATION fileStandardInformation = { 0 };
	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));
	status = ZwQueryInformationFile(hFile,
		&ioStatusBlock,
		&fileStandardInformation,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation);
	if (NT_SUCCESS(status))
	{
		uFileSize = fileStandardInformation.EndOfFile.QuadPart;
	}

	// �����Ӧ��С������
	PVOID pBuffer = NULL;
	pBuffer = ExAllocatePoolWithTag(PagedPool, uFileSize, DUMP_FILE_TAG);
	if (!pBuffer)
	{
		DbgPrint("ExAllocatePoolWithTag failed in %s in %d line\n", __FUNCTION__, __LINE__);
		goto END_FREE_RES;
	}

	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));
	
	LARGE_INTEGER offset = { 0 };
	offset.QuadPart = 0;
	// ��ȡ�ļ�����
	status = ZwReadFile(hFile, 
		NULL,
		NULL,
		NULL,
		&ioStatusBlock,
		pBuffer,
		uFileSize,
		&offset,
		0);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("ZwReadFile failed in %s in %d status : %08x\n", __FUNCTION__, __LINE__, status);
		return status;
	}


	PWCHAR pTempBuf = NULL;
	USHORT uTempSize = 0;
	// ��ȡ�ļ�����
	USHORT i = (Path->Length - sizeof(WCHAR)) / sizeof(WCHAR);
	for (; i != 0; i--)
	{
		if (Path->Buffer[i] == L'\\')
		{
			pTempBuf = &(Path->Buffer[i + 1]);
			uTempSize = Path->Length - i * sizeof(WCHAR);
			break;
		}
	}

	WCHAR wTemp[uSize] = { 0 };
	// RtlCopyMemory(wTemp, pTempBuf, uTempSize);
	UNICODE_STRING ustrTemp = { 0 };
	RtlInitEmptyUnicodeString(&ustrTemp, pTempBuf, uTempSize);
	

	
	// ����һ��dump���ļ�
	UNICODE_STRING ustrDumpFile = { 0 };
	// ƴ��һ����ʽ��

	WCHAR szBuffer[uSize] = { 0 };
	RtlInitEmptyUnicodeString(&ustrDumpFile, szBuffer, uSize);
	RtlUnicodeStringCatString(&ustrDumpFile, L"\\??\\c:\\dump_");
    RtlUnicodeStringCatString(&ustrDumpFile, ustrTemp.Buffer);
	OBJECT_ATTRIBUTES objectAttributes;
	InitializeObjectAttributes(&objectAttributes,
		&ustrDumpFile,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);
	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));
	
	
	status = ZwCreateFile(&hWriteFile,
		GENERIC_WRITE,//��д����
		&objectAttributes,//OA
		&ioStatusBlock,//io״̬
		NULL,//һ����NULL
		FILE_ATTRIBUTE_NORMAL,//�ļ�����һ��дNORMAL
		FILE_SHARE_VALID_FLAGS,//�ļ������ԣ�
										   // һ��ֻ��дFILE_SHARE_READ���У�
										   // ���������������Ҫд��0x7Ҳ����ȫ����ģʽ
		FILE_OPEN_IF,//���ļ�������ʱ�����������ڴ�
		FILE_SYNCHRONOUS_IO_NONALERT,//
															   // �ļ���Ŀ¼����
															   // �����ļ�����ֱ��д���ļ�ϵͳ��ֱ��д�벻�����������ʱ���⣬��IOռ�úࣩܶ
		NULL, //EA������дNULL�������ļ��������������豸�Ľ���������EAдNULL,EA����Ҳ��0
		0);

	if (NT_SUCCESS(status))
	{
		status = ZwWriteFile(hWriteFile,
			NULL,
			NULL,
			NULL,
			&ioStatusBlock,
			pBuffer,
			uFileSize,
			NULL,
			NULL);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("ZwWriteFile failed in %s in %d line error code : %08x\n", __FUNCTION__, __LINE__, status);
			goto END_FREE_RES;
		}
		else
		{
			DbgPrint("dump success\n");
		}
	}
	else
	{
		DbgPrint("ZwCreateFile failed in %s in %d line error code : %08x\n", __FUNCTION__, __LINE__, status);
		goto END_FREE_RES;
	}

	
END_FREE_RES:
	if (hWriteFile)
	{
		ZwClose(hWriteFile);
		hWriteFile = NULL;
	}
	if (hFile)
	{
		ZwClose(hFile);
		hFile = NULL;
	}
	if (pBuffer)
	{
		ExFreePoolWithTag(pBuffer, DUMP_FILE_TAG);
		pBuffer = NULL;
	}
	
	
	
	return status;
}

VOID
FilterLoadImageNotifyRoutine(
	PUNICODE_STRING FullImageName,
	HANDLE ProcessId,
	PIMAGE_INFO ImageInfo)
{
	// filter system process image file
	if ((HANDLE)0 == ProcessId)
	{
		DbgPrint("%wZ\n", FullImageName);
		MyCopyFile(FullImageName);
	}
}

VOID
DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	PsRemoveLoadImageNotifyRoutine(FilterLoadImageNotifyRoutine);
}

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject, 
	IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	DriverObject->DriverUnload = DriverUnload;

	status = PsSetLoadImageNotifyRoutine(FilterLoadImageNotifyRoutine);

	return status;
}