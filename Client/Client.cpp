// Client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

//TCP Client
int main()
{
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		printf("WSAStartup failed: %d \n", result);
		return 1;
	}
	printf("[Client] Winsock initialized successfully. \n");

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket creation failed: %d\n", GetLastError());
		WSACleanup();
		return 1;
	}

	//Connect (Client-Side)
	struct sockaddr_in serverAddr;//C语言风格
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	//inet_pton(AF_INET, "192.168.3.201", &serverAddr.sin_addr);

	if (connect(sock, (const sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("Connect failed: %d \n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
	}


	send(sock, "Hello, Server!", 14, 0);//客户端先发数据

	char buffer[512];
	int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
	if (bytesReceived > 0) {
		printf("Received: %s \n", buffer);
	}


	printf("[Client] exiting...\n");
	closesocket(sock);
	WSACleanup();
	printf("[Client] exited \n");

	getchar();
	getchar();

}

