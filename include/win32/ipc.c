#include "ipc.h"

int ipc_read(char* exec, char* buffer, int buflen)
{
	char cmd[256];
	DWORD len;
	HANDLE hread, hwrite;
	SECURITY_ATTRIBUTES pa; 
	STARTUPINFO sui;
	PROCESS_INFORMATION pi; 

	pa.nLength = sizeof(SECURITY_ATTRIBUTES);
	pa.lpSecurityDescriptor = NULL;
	pa.bInheritHandle = TRUE;

	if (CreatePipe(&hread, &hwrite, &pa, 0) == 0)
		return 0;

	sui.cb = sizeof(STARTUPINFO);
	GetStartupInfo(&sui);
	sui.hStdInput = 0;
	sui.hStdOutput = hwrite;
	sui.hStdError = hwrite;
	sui.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	sui.wShowWindow = SW_HIDE;

	if (strlen(exec) < 240)
	{   
		sprintf(cmd, "cmd.exe /A /C \"%s\"", exec);

		if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &sui, &pi))
		{
			CloseHandle(hwrite);

			ReadFile(hread, buffer, buflen, &len, NULL);
			buffer[len] = 0;

			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}	   
	}   

	CloseHandle(hread);
	CloseHandle(hwrite);
	return len;
}
