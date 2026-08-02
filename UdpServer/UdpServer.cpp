// UdpServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<WS2tcpip.h>

#pragma comment(lib,"ws2_32.lib")


//UDP Server (信笺)
int main()
{
	printf("server:\n");
	//1、初始化套接字环境
	WSADATA wsadata;
	int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (nError != 0) {
		printf("Initialization Error! \n");
		return -1;
	}

	//2、初始化地址和端口
	unsigned short CPORT = 6556;
	SOCKADDR_IN serverAddr, clientAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(6556);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//3、创建套接字
	SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//0,根据type自动选择
	if (server == INVALID_SOCKET) {
		printf("Socket creation failed! \n");
		WSACleanup();
		return -1;
	}

	//4、绑定收信地址和端口
	int retVal = bind(server, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR) {
		printf("Bind failed! \n");
		closesocket(server);
		WSACleanup();
		return -1;
	}


	while (1) {

		[=]() {
			//5、用recvfrom接受数据
			int len = sizeof(clientAddr);
			char buf[256] = {};
			int r = recvfrom(server, buf, sizeof(buf), 0, (sockaddr*)&clientAddr, &len);

			if (r == SOCKET_ERROR) {
				printf("[server] recvfrom Error! \n");
			}
			UINT uip = clientAddr.sin_addr.S_un.S_addr;
			printf("%s from IP: %d.%d.%d.%d \n",
				buf, LOBYTE(uip), HIBYTE(uip), LOBYTE(HIWORD(uip)), HIBYTE(HIWORD(uip)));

			//6、用sendto发送数据
			char buf_sendto[256] = {};
			std::cin.getline(buf_sendto, sizeof(buf_sendto));
			sendto(server, buf_sendto, sizeof(buf_sendto), 0, (const sockaddr*)&clientAddr, sizeof(clientAddr));

			}();

	}
	WSACleanup();
	std::cin.get();
	std::cin.get();
}

