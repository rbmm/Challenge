#include "stdafx.h"

_NT_BEGIN

#include "print.h"

NTSTATUS ReadFromFile(_In_ PCWSTR lpFileName, _Out_ PDATA_BLOB pdb)
{
	UNICODE_STRING ObjectName;

	NTSTATUS status = RtlDosPathNameToNtPathName_U_WithStatus(lpFileName, &ObjectName, 0, 0);

	if (0 <= status)
	{
		HANDLE hFile;
		IO_STATUS_BLOCK iosb;
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

		status = NtOpenFile(&hFile, FILE_GENERIC_READ, &oa, &iosb,
			FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);

		RtlFreeUnicodeString(&ObjectName);

		if (0 <= status)
		{
			FILE_STANDARD_INFORMATION fsi;

			if (0 <= (status = NtQueryInformationFile(hFile, &iosb, &fsi, sizeof(fsi), FileStandardInformation)))
			{
				if (PUCHAR pb = new UCHAR[fsi.EndOfFile.LowPart + 1])
				{
					if (0 > (status = NtReadFile(hFile, 0, 0, 0, &iosb, pb, fsi.EndOfFile.LowPart, 0, 0)))
					{
						delete[] pb;
					}
					else
					{
						pdb->pbData = pb;
						pdb->cbData = (ULONG)iosb.Information;
						pb[iosb.Information] = 0;
					}
				}
			}

			NtClose(hFile);
		}
	}

	return status;
}

BOOL ProcessData(PSTR psz, PULONG pn, PULONG pm, PULONG pk, PULONG pcount)
{
	ULONG k = 0, m = 0;
	while (*psz)
	{
		if (1000 <= (m = strtoul(psz, &psz, 10)) || '\n' != *psz++)
		{
			return FALSE;
		}
		pn[m]++, k++;
	}

	if (k)
	{
		*pk = k;
		ULONG i = 1000, count = 0;
		do
		{
			if (count < (k = pn[--i]))
			{
				count = k, m = i;
			}
		} while (i);

		*pm = m, * pcount = count;
		return TRUE;
	}

	return FALSE;
}

NTSTATUS run()
{
	PrintInfo pri;
	InitPrintf();

	// *filename
	DbgPrint("cmd: \"%ws\"\r\n", GetCommandLineW());

	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (PWSTR lpFileName = wcschr(GetCommandLineW(), '*'))
	{
		DATA_BLOB db;
		if (0 <= (status = ReadFromFile(1 + lpFileName, &db)))
		{
			status = STATUS_NO_MEMORY;

			if (PULONG pn = new ULONG[1000])
			{
				status = STATUS_BAD_DATA;

				RtlFillMemoryUlong(pn, 1000 * sizeof(ULONG), 0);
				ULONG m, k, count;
				ULONG64 t = GetTickCount64();
				BOOL f = ProcessData((PSTR)db.pbData, pn, &m, &k, &count);
				t = GetTickCount64() - t;
				delete[] pn;

				if (f)
				{
					status = STATUS_SUCCESS;
					DbgPrint("%u [%u time] from %u [%I64u ms]\r\n", m, count, k, t);
				}
			}

			delete[] db.pbData;
		}
	}

	return PrintError(status);
}

void WINAPI ep(void*)
{
	ExitProcess(run());
}

_NT_END