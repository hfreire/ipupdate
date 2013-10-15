#include "ipc.h"
#include "../../ipupdate.h"

int ipc_read(char* exec, char* buffer, int buflen)
{
	int fd[2], pid, len;

	if (pipe(fd) == -1)
		return 0;

	switch (pid = fork()) {
	case 0:							//child
		if (fd[1] != 1)
		{
			close(1);
			dup(fd[1]);
		}
		close(2);
		dup(fd[1]);
		close(fd[0]);
		execlp("sh", "sh", "-c", exec, NULL);
		exit(-1);
	case -1:						//fork failed
		return 0;
	}

	close(fd[1]);

	if ((len = read(fd[0], buffer, buflen-1)) > 0)
		buffer[len] = 0;

	close(fd[0]);

	waitpid(pid, NULL, 0);

	return len;
}
