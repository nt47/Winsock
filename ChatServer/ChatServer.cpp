// ChatServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<ws2tcpip.h>
#include<thread>
#include<vector>
#include<unordered_map>
#include<shared_mutex>

#pragma comment(lib,"ws2_32.lib")

enum class PacketType :uint16_t
{
	PK_LOGIN = 1,
	PK_CHAT = 2,
	PK_LOGOUT = 3,
	PK_HEARTBEAT = 4
};


#pragma pack(push, 1)
struct ChatPacketHeader
{
	uint32_t magic;      // 标识协议，例如 0x12345678
	uint16_t version;    // 协议版本
	PacketType type;       // 消息类型
	uint32_t user_id;     // 发送者ID

	//uint32_t packetSize;   // 整包大小
	//uint64_t timestamp;    // 时间戳
	//uint32_t sequence;     // 包序号		
};
typedef struct _ChatPacketBody {
	uint32_t target_user_id;	//接收者ID
	uint32_t payload_length;     // payload长度
}ChatPacketBody, * PChatPacketBody;

#pragma pack(pop)



void SendChatPacket(SOCKET sock,
	uint32_t user_id,
	uint32_t target_user_id,
	const std::string& text)
{
	ChatPacketHeader header;
	ChatPacketBody body;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_CHAT;
	header.user_id = user_id;

	body.target_user_id = target_user_id;
	body.payload_length = (uint32_t)text.size();


	// 发头
	send(sock,
		(char*)&header,
		sizeof(header),
		0);


	// 发内容
	send(sock,
		(char*)&body,
		sizeof(body),
		0);

	send(sock,
		text.data(),
		body.payload_length,
		0);
}


bool is_recv(SOCKET s) {
	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(s, &readfds);

	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10000; //10ms

	int ret = select(0, &readfds, nullptr, nullptr, &tv);
	if (ret > 0)
		return true;

	return false;
}


bool IsClientDisconnected(SOCKET sock)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(sock, &set);

	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;   // 立即返回

	int ret = select(0, &set, NULL, NULL, &tv);

	if (ret > 0 && FD_ISSET(sock, &set))
	{
		char buf;
		int len = recv(sock, &buf, 1, MSG_PEEK);

		if (len == 0)
			return true;   // 客户端关闭

		if (len == SOCKET_ERROR)
		{
			int err = WSAGetLastError();

			if (err == WSAECONNRESET)
				return true; // 异常断开
		}
	}

	return false;
}


typedef struct _ChatInfo_t {
	uint32_t user_id;
	uint32_t target_user_id;
	std::string msg;
}ChatInfo_t, * PChatInfo_t;

typedef struct _Client_t {
	int user_id;
	SOCKET sock;

}Client_t, * PClient_t;


std::unordered_map<int, Client_t> clients;
std::vector<ChatInfo_t>g_chatInfos;

std::shared_mutex g_clientsMutex;

int main()
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

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
	printf("[Server] Listening... \n");
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed: %d \n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	std::thread t([=]() {

		while (1) {

			[=]() {

				SOCKET clientSock;
				struct sockaddr_in clientAddr;
				int addrLen = sizeof(clientAddr);

				clientSock = accept(sock, NULL, NULL);//也能获取对方地址信息
				if (clientSock == INVALID_SOCKET) {
					printf("Accept failed: %d \n", WSAGetLastError());
					closesocket(sock);
					WSACleanup();
					return;
				}

				getpeername(clientSock,
					(struct sockaddr*)&clientAddr,
					&addrLen);


				char ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));



				ChatPacketHeader header;
				int bytesReceived = recv(clientSock, (char*)&header, sizeof(header), 0);

				if (bytesReceived > 0) {
					if (header.magic != 0x12345678)
						return;
				}

				Client_t client{};
				client.user_id = header.user_id;
				client.sock = clientSock;

				std::unique_lock lock(g_clientsMutex);//锁写
				if (clients.count(header.user_id) == 0) {
					clients[header.user_id] = client;
				}

				printf("Client Address: %s:%d |UserId: %d\n",
					ip, ntohs(clientAddr.sin_port),
					client.user_id);
				}();


			Sleep(100);
		}
		});


	while (1) {



		[]() {
			std::unique_lock lock(g_clientsMutex);//锁写

			for (auto it = clients.begin(); it != clients.end();) {
				auto& client = it->second;
				if (IsClientDisconnected(client.sock)) {
					printf("Id: %d Client Exited \n", client.user_id);
					it = clients.erase(it);//必须赋值，指向下一个
				}
				else {
					it++;
				}

			}//检查退出的客户端
			}();


		[]() {

			std::shared_lock lock(g_clientsMutex);//锁读


			for (auto pair : clients)
			{
				auto& client = pair.second;

				if (!is_recv(client.sock))
					continue;


				[=]() {//接受请求
					ChatPacketHeader header;
					ChatPacketBody body;

					int bytesReceived = recv(client.sock, (char*)&header, sizeof(header), 0);
					if (bytesReceived > 0) {
						if (header.magic != 0x12345678)
							return;

						recv(client.sock, (char*)&body, sizeof(body), 0);


						std::string msg;
						msg.resize(body.payload_length);

						recv(client.sock, msg.data(), body.payload_length, 0);

						printf("userId:%d | TargetUserId:%d | msg:%s \n",
							header.user_id, body.target_user_id, msg.c_str());

						ChatInfo_t info;
						info.user_id = header.user_id;
						info.target_user_id = body.target_user_id;
						info.msg = msg;
						g_chatInfos.push_back(info);

					}
					}();

			}


			for (auto pair : clients) {
				auto& client = pair.second;

				[=]() {//发送请求
					for (auto it = g_chatInfos.begin(); it != g_chatInfos.end();) {
						auto& info = *it;

						if (client.user_id == info.target_user_id) {
							SendChatPacket(
								client.sock,
								info.user_id,
								info.target_user_id, info.msg);
							it = g_chatInfos.erase(it);//必须赋值，指向下一个

						}
						else {
							it++;
						}
					}

					}();
			}

			}();

		Sleep(100);
	}




	printf("[Server] exiting...");

	for (auto pair : clients) {
		auto& client = pair.second;

		closesocket(client.sock);
	}
	clients.clear();
	WSACleanup();
	printf("[Server] exited");

	std::cin.get();
	std::cin.get();
}


