#include"prod.h"

bool RecvAll(
	SOCKET s,
	char* buf,
	int len)
{
	int total = 0;

	while (total < len)
	{
		int n = recv(
			s,
			buf + total,
			len - total,
			0);

		if (n <= 0)
			return false;

		total += n;
	}

	return true;
}