// P2pClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<ws2tcpip.h>
#include<string>
#include<thread>
#include"ini.h"
#include<filesystem>
#include<fstream>
#include"encoding.h"
#include<random>
#include<unordered_map>
#include<mutex>
#include"xor_encrypt.h"

#pragma comment(lib,"ws2_32.lib")

enum class AckType
{
	ACK_LOGIN = 1,
	ACK_CHAT = 2,
	ACK_POS = 3,
	ACK_PUNCH = 4,
};

enum class PacketType :uint16_t
{
	PK_LOGIN = 1,
	PK_CHAT = 2,
	PK_LOGOUT = 3,
	PK_HEARTBEAT = 4,
	PK_ACK = 5,
	PK_POS = 6,
	PK_PUNCH = 7,
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
	uint32_t sequence;     // 包序号
};
typedef struct _ChatPacketBody {
	uint32_t target_user_id;	//接收者ID
	uint32_t payload_length;     // payload长度
}ChatPacketBody, * PChatPacketBody;

typedef struct _PosPacketBody_t {
	uint32_t target_user_id;
	SOCKADDR_IN target_user_addr_in;
}PosPacketBody_t, * PPosPacketBody_t;

#pragma pack(pop)

bool SimulateDrop()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	static std::uniform_int_distribution<> dis(1, 100);


	return dis(gen) <= 50; //50%丢包
}

void SendToAckPacket(SOCKET sock, uint32_t user_id, uint32_t seqeunce, const sockaddr* to, int len) {
	ChatPacketHeader header;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_ACK;
	header.user_id = user_id;
	header.sequence = seqeunce;

	// 发头
	sendto(sock,
		(char*)&header,
		sizeof(header),
		0, to, len);
}

void SendToHeartbeatPacket(SOCKET sock, uint32_t user_id, const sockaddr* to, int len) {
	ChatPacketHeader header;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_HEARTBEAT;
	header.user_id = user_id;


	// 发头
	sendto(sock,
		(char*)&header,
		sizeof(header),
		0, to, len);
}

void SendToLoginPacket(SOCKET sock, uint32_t user_id, uint32_t sequence, const sockaddr* to, int len) {
	ChatPacketHeader header;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_LOGIN;
	header.user_id = user_id;
	header.sequence = sequence;

	// 发头
	sendto(sock,
		(char*)&header,
		sizeof(header),
		0, to, len);

}


void SendToPosPacket(SOCKET sock,
	uint32_t user_id,
	uint32_t target_user_id,
	SOCKADDR_IN addr_in,
	uint32_t sequence,
	const sockaddr* to,
	int len)
{
	ChatPacketHeader header{};
	PosPacketBody_t body{};

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_POS;
	header.user_id = user_id;
	header.sequence = sequence;

	body.target_user_id = target_user_id;
	body.target_user_addr_in = addr_in;


	//if (SimulateDrop())
	//{
	//	printf("Client drops packet, sequence is %d \n", sequence);
	//	return;
	//}

	std::vector<char> packet;
	packet.insert(packet.end(), (char*)&header, (char*)&header + sizeof(header));
	packet.insert(packet.end(), (char*)&body, (char*)&body + sizeof(body));

	sendto(sock, packet.data(), (int)packet.size(), 0, to, len);
}

void SendToPunchPacket(SOCKET sock,
	uint32_t user_id,
	uint32_t target_user_id,
	uint32_t sequence,
	const sockaddr* to,
	int len,
	const std::string& text)
{
	ChatPacketHeader header;
	ChatPacketBody body;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_PUNCH;
	header.user_id = user_id;
	header.sequence = sequence;

	body.target_user_id = target_user_id;
	body.payload_length = (uint32_t)text.size();




	std::vector<char> packet;
	packet.insert(packet.end(), (char*)&header, (char*)&header + sizeof(header));
	packet.insert(packet.end(), (char*)&body, (char*)&body + sizeof(body));
	packet.insert(packet.end(), text.begin(), text.end());


	//if (SimulateDrop())
	//{
	//	printf("Client drops packet, sequence is %d \n", sequence);
	//	return;
	//}

	sendto(sock, packet.data(), (int)packet.size(), 0, to, len);
}


void SendToChatPacket(SOCKET sock,
	uint32_t user_id,
	uint32_t target_user_id,
	uint32_t sequence,
	const sockaddr* to,
	int len,
	const std::string& text)
{
	ChatPacketHeader header;
	ChatPacketBody body;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_CHAT;
	header.user_id = user_id;
	header.sequence = sequence;

	body.target_user_id = target_user_id;
	body.payload_length = (uint32_t)text.size();




	std::vector<char> packet;
	packet.insert(packet.end(), (char*)&header, (char*)&header + sizeof(header));
	packet.insert(packet.end(), (char*)&body, (char*)&body + sizeof(body));
	packet.insert(packet.end(), text.begin(), text.end());


	//if (SimulateDrop())
	//{
	//	printf("Client drops packet, sequence is %d \n", sequence);
	//	return;
	//}

	sendto(sock, packet.data(), (int)packet.size(), 0, to, len);
}


bool is_recv(SOCKET s)
{
	static WSAEVENT ev = WSA_INVALID_EVENT;

	// 创建事件对象（只创建一次）
	if (ev == WSA_INVALID_EVENT)
	{
		ev = WSACreateEvent();
		WSAEventSelect(s, ev, FD_READ | FD_CLOSE);
	}

	// 等待事件（不阻塞）
	DWORD ret = WSAWaitForMultipleEvents(1, &ev, FALSE, 0, FALSE);
	if (ret == WSA_WAIT_EVENT_0)
	{
		WSANETWORKEVENTS ne;
		WSAEnumNetworkEvents(s, ev, &ne);

		if (ne.lNetworkEvents & FD_READ)//按位与，检查某一位是否为1
			return true;

		if (ne.lNetworkEvents & FD_CLOSE)
			return true; // TCP 断开也算可读
	}

	return false;
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

typedef struct _PacketCache_t {
	uint32_t user_id;
	uint32_t target_user_id;
	uint32_t sequence;
	std::string msg;
	bool isAck;
	AckType ack_type;
	SOCKADDR_IN addr_in;
	uint64_t last_send_time;
}PacketCache_t, * PPacketCache_t;

typedef struct _Client_t {
	uint32_t target_user_id;
	SOCKADDR_IN addr_in;

}Client_t, PClient_t;

std::unordered_map<int, PacketCache_t> g_caches;
std::unordered_map<int, Client_t> clients;
std::vector<uint32_t> g_target_sequenceList;

std::mutex g_cachesMutex;
HANDLE hEvent;

int main()
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	printf("client: \n");

	hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

	WSAData wsadata;
	int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (nError != 0) {
		printf("Initialization Error! \n");
		return -1;
	}

	unsigned short CPORT = 6556;
	SOCKADDR_IN serverAddr, remoteAdder;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(CPORT);

	IniFile ini(L"./config.ini");
	if (ini.Read(L"server", L"ip") == L"")
		ini.Write(L"server", L"ip", L"127.0.0.1");

	std::wstring server_ip = ini.Read(L"server", L"ip");
	inet_pton(AF_INET, utils::w2s(server_ip).c_str(), &serverAddr.sin_addr);



	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == SOCKET_ERROR) {
		printf("Socket creation failed! \n");
		WSACleanup();
		return -1;
	}

	int user_id = GetCurrentProcessId();
	int target_user_id = 0;
	uint32_t sequence = 101;
	std::cout << "Id: " << user_id << ::std::endl;


	[=, &sequence]() {
		std::lock_guard lock(g_cachesMutex);//锁读写

		PacketCache_t cache{};
		cache.user_id = user_id;
		cache.sequence = sequence;
		cache.ack_type = AckType::ACK_LOGIN;
		g_caches[sequence] = cache;
		SendToLoginPacket(sock, user_id, sequence++, (const sockaddr*)&serverAddr, sizeof(serverAddr));
		}();


	std::thread h([=]() {//Heartbeat packet
		while (1) {
			SendToHeartbeatPacket(sock, user_id, (const sockaddr*)&serverAddr, sizeof(serverAddr));
			Sleep(3000);
		}
		});
	h.detach();

	std::thread r([=, &target_user_id, &sequence]() {

		while (1) {//接受消息

			[=, &target_user_id, &sequence]() {

				std::lock_guard lock(g_cachesMutex);//锁读写

				if (is_recv(sock))
					return;

				int len = sizeof(remoteAdder);
				char buffer[65535];
				ChatPacketHeader header;

				int bytesReceived = recvfrom(sock, (char*)&buffer, sizeof(buffer), 0, (sockaddr*)&remoteAdder, &len);
				memcpy(&header, buffer, sizeof(header));

				if (bytesReceived > 0) {
					if (header.magic != 0x12345678)
						return;

					switch (header.type) {
					case PacketType::PK_CHAT:
					{
						[=, &len]() {

							if (std::find(g_target_sequenceList.begin(), g_target_sequenceList.end(), header.sequence) != g_target_sequenceList.end())
							{
								//printf("Duplicate packet, sequence is %d \n", header.sequence);
								SendToAckPacket(sock, header.user_id, header.sequence, (const sockaddr*)&remoteAdder, len);
								return;
							}
							else
							{
								if (g_target_sequenceList.size() > 100)//超过100个就删除最早的
								{
									g_target_sequenceList.erase(g_target_sequenceList.begin());

								}
								g_target_sequenceList.push_back(header.sequence);
							}//Sequence number deduplication

							ChatPacketBody body;
							memcpy(&body, buffer + sizeof(header), sizeof(body));

							std::string msg;
							msg.assign(buffer + sizeof(header) + sizeof(body), body.payload_length);
							//解密
							msg = xorDecrypt(msg, 'k');

							printf("from userId:%d | msg:%s \n",
								header.user_id, msg.c_str());

							}();

						SendToAckPacket(sock, header.user_id, header.sequence, (const sockaddr*)&remoteAdder, len);
					}
					break;
					case PacketType::PK_LOGOUT://重新登陆
					{

						[=, &sequence]() {
							PacketCache_t cache{};
							cache.user_id = user_id;
							cache.sequence = sequence;
							cache.ack_type = AckType::ACK_LOGIN;
							g_caches[sequence] = cache;
							SendToLoginPacket(sock, user_id, sequence++, (const sockaddr*)&remoteAdder, len);
							}();
					}
					break;
					case PacketType::PK_POS:
					{
						[=, &len, &sequence]() {

							PosPacketBody_t body{};
							memcpy(&body, buffer + sizeof(header), sizeof(body));

							Client_t client{};
							client.target_user_id = body.target_user_id;
							client.addr_in = body.target_user_addr_in;
							clients[body.target_user_id] = client;

							SendToAckPacket(sock, header.user_id, header.sequence, (const sockaddr*)&remoteAdder, len);


							//确认到货，发送PUNCH包
							PacketCache_t cache{};
							cache.user_id = user_id;
							cache.target_user_id = target_user_id;
							cache.addr_in = clients[target_user_id].addr_in;
							cache.sequence = sequence;
							cache.msg = "PUNCH!";
							cache.ack_type = AckType::ACK_PUNCH;
							g_caches[sequence] = cache;
							SendToPunchPacket(sock, user_id, target_user_id, sequence++, (const sockaddr*)&clients[target_user_id].addr_in, len, cache.msg);

							}();


					}
					break;
					case PacketType::PK_PUNCH:
					{
						[=, &target_user_id, &len]() {

							ChatPacketBody body;
							memcpy(&body, buffer + sizeof(header), sizeof(body));

							std::string msg;
							msg.assign(buffer + sizeof(header) + sizeof(body), body.payload_length);


							//printf("from userId:%d | msg:%s \n", header.user_id, msg.c_str());

							if (msg == "PUNCH!")
							{
								SetEvent(hEvent);
								SendToAckPacket(sock, header.user_id, header.sequence, (const sockaddr*)&clients[target_user_id].addr_in, len);
							}

							}();


					}
					break;
					case PacketType::PK_ACK:
					{
						g_caches[header.sequence].isAck = true;
					}
					break;
					default:
						break;
					}


				}

				}();

			Sleep(100);
		}

		});
	r.detach();


	std::thread q([=]() { //重发队列
		while (1) {

			[=]() {
				std::lock_guard lock(g_cachesMutex);//锁读写

				for (auto it = g_caches.begin(); it != g_caches.end();)
				{
					auto& cache = it->second;
					if (cache.isAck)
					{
						it = g_caches.erase(it);
					}
					else {
						if (cache.ack_type == AckType::ACK_LOGIN)
						{
							SendToLoginPacket(sock, cache.user_id, cache.sequence, (const sockaddr*)&serverAddr, sizeof(serverAddr));
						}
						if (cache.ack_type == AckType::ACK_CHAT)
						{
							uint64_t now = GetTickCount64();
							if (now - cache.last_send_time > 3000)
							{
								cache.last_send_time = now;
								SendToChatPacket(sock, cache.user_id, cache.target_user_id, cache.sequence, (const sockaddr*)&cache.addr_in, sizeof(serverAddr), cache.msg);
							}
						}
						if (cache.ack_type == AckType::ACK_POS)
						{
							//printf("SendToPosPacket \n");
							SendToPosPacket(sock, cache.user_id, cache.target_user_id, cache.addr_in, cache.sequence, (const sockaddr*)&serverAddr, sizeof(serverAddr));
						}
						if (cache.ack_type == AckType::ACK_PUNCH)
						{
							//printf("SendToPunchPacket \n");
							//char ip[INET_ADDRSTRLEN];
							//inet_ntop(AF_INET, &cache.addr_in.sin_addr, ip, sizeof(ip));

							//printf("[PUNCH] Remote Address: %s:%d |UserId: %d | sequence %d\n",
							//	ip, ntohs(cache.addr_in.sin_port),
							//	cache.target_user_id, cache.sequence);


							SendToPunchPacket(sock, cache.user_id, cache.target_user_id, cache.sequence, (const sockaddr*)&cache.addr_in, sizeof(serverAddr), cache.msg);
						}
						it++;
					}

				}

				}();

			Sleep(1000);
		}


		});
	q.detach();


	while (1) {
		[=, &target_user_id, &sequence]() {
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

				[=, &target_user_id, &sequence]() {
					std::lock_guard lock(g_cachesMutex);//锁读写

					PacketCache_t cache{};
					cache.user_id = user_id;
					cache.target_user_id = target_user_id;
					cache.addr_in = serverAddr;
					cache.sequence = sequence;
					cache.ack_type = AckType::ACK_POS;
					g_caches[sequence] = cache;
					SendToPosPacket(sock, user_id, target_user_id, serverAddr, sequence++, (const sockaddr*)&serverAddr, sizeof(serverAddr));
					}();
			}



			if (clients.count(target_user_id) == 0)
			{
				printf("Waiting for Punch...\n");
				WaitForSingleObject(hEvent, INFINITE);
			}

			int len = sizeof(serverAddr);

			char buf_sendto[256];
			std::cout << "Enter a Message, Please:" << std::endl;
			std::cin >> buf_sendto;
			std::cin.ignore();

			if (strstr(buf_sendto, "-t") || strstr(buf_sendto, "--toggle"))
			{
				target_user_id = 0;
				g_target_sequenceList.clear();//清空已接收的序列号,防止乱丢
				return;
			}

			[=, &sequence]() {
				std::lock_guard lock(g_cachesMutex);//锁读写

				PacketCache_t cache{};
				cache.user_id = user_id;
				cache.target_user_id = target_user_id;
				cache.addr_in = clients[target_user_id].addr_in;
				cache.sequence = sequence;
				cache.msg = xorEncrypt(buf_sendto, 'k');
				cache.ack_type = AckType::ACK_CHAT;
				cache.last_send_time = GetTickCount64();
				g_caches[sequence] = cache;
				SendToChatPacket(sock, user_id, target_user_id, sequence++, (const sockaddr*)&clients[target_user_id].addr_in, len, xorEncrypt(buf_sendto, 'k'));
				}();

			}();
	}



	closesocket(sock);
	WSACleanup();
	system("pause");
}

