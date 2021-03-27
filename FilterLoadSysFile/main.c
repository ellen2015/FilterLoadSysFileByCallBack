#include <ntddk.h>
#include <ntstrsafe.h>

#define DUMP_FILE_TAG 'pmuD'


#define uSize (260*2)
///
///  读取指定路径的文件
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

	// 获取文件句柄
	status = ZwCreateFile(&hFile,	//句柄指针
		0x81,
		&oa,						// OA
		&ioStatusBlock,				// io状态
		NULL,						// 一般添NULL
		FILE_ATTRIBUTE_NORMAL,		// 文件属性一般写NORMAL
		FILE_SHARE_READ,			// 文件共享性，
									// 一般只填写FILE_SHARE_READ就行，
									// 但是特殊情况下需要写入0x7也就是全共享模式
		FILE_OPEN,//当文件不存在时，返回失败
		FILE_SYNCHRONOUS_IO_NONALERT, // 文件非目录性质
									 // 并且文件操作直接写入文件系统（直接写入不会产生缓冲延时问题，但IO占用很多）
		NULL, //EA属性填写NULL，这是文件创建不是驱动设备的交互，所以EA写NULL,EA长度也是0
		0);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("ZwCreateFile failed in %s in %d line error code : %08x\n", __FUNCTION__, __LINE__, status);
		return status;
	}

	// 根据文件句柄文件获取文件大小
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

	// 申请对应大小的数据
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
	// 读取文件数据
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
	// 获取文件名称
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
	

	
	// 创建一个dump的文件
	UNICODE_STRING ustrDumpFile = { 0 };
	// 拼接一个格式的

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
		GENERIC_WRITE,//读写访问
		&objectAttributes,//OA
		&ioStatusBlock,//io状态
		NULL,//一般添NULL
		FILE_ATTRIBUTE_NORMAL,//文件属性一般写NORMAL
		FILE_SHARE_VALID_FLAGS,//文件共享性，
										   // 一般只填写FILE_SHARE_READ就行，
										   // 但是特殊情况下需要写入0x7也就是全共享模式
		FILE_OPEN_IF,//当文件不存在时，创建，存在打开
		FILE_SYNCHRONOUS_IO_NONALERT,//
															   // 文件非目录性质
															   // 并且文件操作直接写入文件系统（直接写入不会产生缓冲延时问题，但IO占用很多）
		NULL, //EA属性填写NULL，这是文件创建不是驱动设备的交互，所以EA写NULL,EA长度也是0
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