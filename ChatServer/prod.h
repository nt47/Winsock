#pragma once
#include<WS2tcpip.h>

bool RecvAll(
	SOCKET s,
	char* buf,
	int len);