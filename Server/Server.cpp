// Server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

//TCP Server
int main()
{
	WSADATA wsaData;
	//请求使用版本为2.2的Socket库
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		printf("WSAStartup failed: %d \n", result);
		return 1;
	}
	printf("[Server] Socket initialized successfully. \n");

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		printf("Socket creation failed: %d \n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//Bind (Server-Side)
	struct sockaddr_in serverAddr;//C语言风格
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;//已更新
	//inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

	if (bind(sock, (const sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		printf("Bind failed: %d \n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	//Listen and Accept (Sever-Side)
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed: %d \n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	SOCKET clientSock = accept(sock, NULL, NULL);
	if (clientSock == INVALID_SOCKET) {
		printf("Accept failed: %d \n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	char buffer[512];
	int bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
	if (bytesReceived > 0)
	{
		printf("Received: %s \n", buffer);
		send(clientSock, "Hello, Client!", 14, 0);
	}


	printf("[Server] exiting...");
	closesocket(clientSock);
	closesocket(sock);
	WSACleanup();
	printf("[Server] exited");

	std::cin.get();
	std::cin.get();
}

