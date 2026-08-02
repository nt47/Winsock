// ChatClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<ws2tcpip.h>
#include<thread>
#include<string>
#include"ini.h"
#include<filesystem>
#include<fstream>
#include"encoding.h"

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

void SendLoginPacket(SOCKET sock, uint32_t user_id) {
	ChatPacketHeader header;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_LOGIN;
	header.user_id = user_id;

	// 发头
	send(sock,
		(char*)&header,
		sizeof(header),
		0);
}

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

bool isNumber(const std::string& s)
{
	if (s.empty())
		return false;

	for (char c : s)
	{
		if (!std::isdigit((unsigned char)c))
			return false;
	}

	return true;
}


HANDLE hEvent;

int main()
{
	SetConsoleCP(CP_UTF8);        // 输入 UTF-8
	SetConsoleOutputCP(CP_UTF8);  // 输出 UTF-8

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

	IniFile ini(L"./config.ini");
	std::filesystem::path config_path = std::filesystem::current_path() / L"config.ini";
	if (!std::filesystem::exists(config_path))
	{
		if (!std::filesystem::exists(config_path.parent_path()))
			std::filesystem::create_directories(config_path.parent_path());
		std::ofstream ofs(config_path);
		if (!ofs) {
			printf("Config creation failed \n");
			WSACleanup();
			return 1;
		}
		ini.Write(L"server", L"ip", L"127.0.0.1");
		ofs.close();
	}
	std::wstring server_ip = ini.Read(L"server", L"ip");
	inet_pton(AF_INET, utils::w2s(server_ip).c_str(), &serverAddr.sin_addr);



	while (connect(sock, (const sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("Connect failed: %d \n", WSAGetLastError());
		//closesocket(sock);
		//WSACleanup();
		Sleep(1000);
	}


	int user_id = GetCurrentProcessId();
	int target_user_id = 0;
	std::cout << "Id: " << user_id << ::std::endl;
	//hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	SendLoginPacket(sock, user_id);



	std::thread t([=]() {

		while (1) {

			[=]() {

				ChatPacketHeader header;
				ChatPacketBody body;

				int bytesReceived = recv(sock, (char*)&header, sizeof(header), 0);
				if (bytesReceived > 0) {
					if (header.magic != 0x12345678)
						return;

					recv(sock, (char*)&body, sizeof(body), 0);


					std::string msg;
					msg.resize(body.payload_length);

					recv(sock, msg.data(), body.payload_length, 0);

					printf("from userId:%d | msg:%s \n",
						header.user_id, msg.c_str());


					//SetEvent(hEvent);
				}

				}();
		}

		});
	t.detach();


	while (1) {
		[=, &target_user_id]() {
			if (target_user_id == 0) {//绑定聊天对象，暂时1对1
				std::cout << "Enter a Target UserId, Please:" << std::endl;
				std::string input;
				std::cin >> input;

				if (isNumber(input))
				{
					target_user_id = std::stoi(input);
				}
				else
				{
					std::cout << "Error,UserId is not an Integer! " << std::endl;;
					return;
				}
				std::cin.ignore();
			}

			char buf_send[512];
			std::cout << "Enter a Message, Please:" << std::endl;
			std::cin.getline(buf_send, 512);


			SendChatPacket(sock, user_id, target_user_id, buf_send);
			//WaitForSingleObject(hEvent, INFINITE);

			}();


	}


	printf("[Client] exiting...\n");
	closesocket(sock);
	WSACleanup();
	CloseHandle(hEvent);
	printf("[Client] exited \n");

	getchar();
	getchar();

}