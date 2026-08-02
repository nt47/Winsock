// P2pServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<WS2tcpip.h>
#include<unordered_map>
#include<mutex>
#include<random>

#pragma comment(lib,"ws2_32.lib")

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

void SendToLogoutPacket(SOCKET sock, uint32_t user_id, const sockaddr* to, int len) {
	ChatPacketHeader header;

	header.magic = 0x12345678;
	header.version = 1;
	header.type = PacketType::PK_LOGOUT;
	header.user_id = user_id;

	// 发头
	sendto(sock,
		(char*)&header,
		sizeof(header),
		0, to, len);
}

bool is_recv(SOCKET s)
{
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

typedef struct _Client_t {
	int user_id;
	SOCKADDR_IN client_addr_in;
	uint64_t last_heartbeat;

}Client_t, * PClient_t;

typedef struct _PacketCache_t {
	uint32_t user_id;
	uint32_t target_user_id;
	uint32_t sequence;
	SOCKADDR_IN user_addr_in;
	bool isAck;
}PacketCache_t, * PPacketCache_t;

std::unordered_map<int, Client_t> clients;
std::unordered_map<int, std::unordered_map<int, PacketCache_t>> g_caches;

std::mutex g_clientsMutex;
int main()
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	printf("server:\n");

	WSADATA wsadata;
	int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (nError != 0) {
		printf("Initialization Error! \n");
		return -1;
	}


	unsigned short CPORT = 6556;
	SOCKADDR_IN serverAddr, clientAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(6556);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);


	SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//0,根据type自动选择
	if (server == INVALID_SOCKET) {
		printf("Socket creation failed! \n");
		WSACleanup();
		return -1;
	}


	int retVal = bind(server, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR) {
		printf("Bind failed! \n");
		closesocket(server);
		WSACleanup();
		return -1;
	}

	std::thread h([]() {//Heartbeat packet
		while (1) {
			//return;//调试
			[]() {
				std::lock_guard lock(g_clientsMutex);//锁读写
				for (auto it = clients.begin(); it != clients.end();) {
					auto& pair = *it;
					auto& client = pair.second;

					uint64_t now = GetTickCount64();
					if (now - client.last_heartbeat > 5000) {
						printf("[!] Clinet disconnected,UserId:%d \n", client.user_id);
						it = clients.erase(it);//必须赋值，指向下一个
					}
					else {
						it++;
					}
				}


				}();

			Sleep(3000);
		}

		});
	h.detach();






	std::thread r([=]() {

		while (1) {

			[=]() {//接受消息

				std::lock_guard lock(g_clientsMutex);//锁读写

				if (!is_recv(server))
					return;
				int len = sizeof(clientAddr);
				char buffer[65535];
				ChatPacketHeader header;

				int bytesReceived = recvfrom(server, (char*)&buffer, sizeof(buffer), 0, (sockaddr*)&clientAddr, &len);
				memcpy(&header, buffer, sizeof(header));

				if (bytesReceived > 0) {
					if (header.magic != 0x12345678)
						return;
				}

				switch (header.type) {

				case PacketType::PK_HEARTBEAT:
				{
					//printf("Client heartbeat: UserId: %d \n", header.user_id);
					if (clients.find(header.user_id) == clients.end())//id不存在就跳出
					{
						//printf("Client logout: UserId: %d \n", header.user_id);
						SendToLogoutPacket(server, header.user_id, (const sockaddr*)&clientAddr, len);
						break;
					}
					clients[header.user_id].last_heartbeat = GetTickCount64();
				}
				break;
				case PacketType::PK_LOGIN:
				{

					[=]() {
						Client_t client{};
						client.user_id = header.user_id;
						client.client_addr_in = clientAddr;
						client.last_heartbeat = GetTickCount64();

						if (clients.count(header.user_id) == 0) {
							clients[header.user_id] = client;
						}
						else {
							//客户端丢包了
							SendToAckPacket(server, header.user_id, header.sequence, (const sockaddr*)&clientAddr, len);
							return;
						}

						char ip[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));

						printf("[Login] Client Address: %s:%d |UserId: %d\n",
							ip, ntohs(clientAddr.sin_port),
							client.user_id);
						SendToAckPacket(server, header.user_id, header.sequence, (const sockaddr*)&clientAddr, len);

						}();
				}
				break;
				case PacketType::PK_POS:
				{

					[=]() {
						PosPacketBody_t body;
						memcpy(&body, buffer + sizeof(header), sizeof(body));

						PacketCache_t cache{};
						cache.user_id = header.user_id;
						cache.target_user_id = body.target_user_id;
						cache.sequence = header.sequence;
						cache.user_addr_in = clientAddr;
						g_caches[header.user_id][header.sequence] = cache;

						SendToAckPacket(server, header.user_id, header.sequence, (const sockaddr*)&clientAddr, len);
						}();

				}
				break;
				case PacketType::PK_ACK:
				{
					g_caches[header.user_id][header.sequence].isAck = true;
				}
				break;
				default:
					break;
				}


				}();

			Sleep(100);
		}
		});
	r.detach();







	while (1) {

		[=]() {//发送消息和重发消息
			std::lock_guard lock(g_clientsMutex);//锁读写

			for (auto pair : clients) {
				auto& client = pair.second;
				for (auto it = g_caches.begin(); it != g_caches.end(); it++) {
					auto& user_caches = it->second;

					for (auto user_it = user_caches.begin(); user_it != user_caches.end();)
					{
						auto& cache = user_it->second;

						if (cache.isAck)
						{
							user_it = user_caches.erase(user_it);//必须赋值，指向下一个
						}
						else {
							if (cache.target_user_id == client.user_id) {
								SendToPosPacket(
									server,
									cache.user_id,
									cache.target_user_id,
									client.client_addr_in,
									cache.sequence,
									(const sockaddr*)&cache.user_addr_in,
									sizeof(cache.user_addr_in)
								);
							}
							user_it++;
						}
					}


				}
			}

			}();

		Sleep(1000);
	}

	closesocket(server);
	clients.clear();
	WSACleanup();
	system("pause");
}

