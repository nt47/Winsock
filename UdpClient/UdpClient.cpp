// UdpClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include<ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

int main()
{
	printf("client: \n");
	//1、初始化套接字环境
	WSAData wsadata;
	int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (nError != 0) {
		printf("Initialization Error! \n");
		return -1;
	}

	//2、初始化地址和端口
	unsigned short CPORT = 6556;
	SOCKADDR_IN serverAddr, remoteAdder;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(CPORT);
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	//serverAddr.sin_addr.S_un.S_addr = inet_addr("192.168.3.201");

	//3、创建套接字
	SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client == SOCKET_ERROR) {
		printf("Socket creation failed! \n");
		WSACleanup();
		return -1;
	}

	while (1) {
		[=]() {
			//4、用sendto发送数据
			char buf_sendto[256];
			std::cin >> buf_sendto;
			sendto(client, buf_sendto, sizeof(buf_sendto), 0, (const sockaddr*)&serverAddr, sizeof(serverAddr));

			//5、用recvfrom接受数据
			int len = sizeof(remoteAdder);
			char buf[256];
			int r = recvfrom(client, buf, sizeof(buf), 0, (sockaddr*)&remoteAdder, &len);
			if (r == SOCKET_ERROR) {
				printf("[client] recvfrom Error! \n");
			}
			UINT uip = remoteAdder.sin_addr.S_un.S_addr;
			printf("%s from IP: %d.%d.%d.%d \n",
				buf, LOBYTE(uip), HIBYTE(uip), LOBYTE(HIWORD(uip)), HIBYTE(HIWORD(uip)));
			}();

	}

	WSACleanup();
	getchar();
	getchar();
}
