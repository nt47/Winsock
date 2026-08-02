#pragma once
#include <winsock2.h>
#include <unordered_map>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

class SocketEventChecker
{
private:
	std::unordered_map<SOCKET, WSAEVENT> events;
	std::mutex mtx;

public:

	bool is_recv(SOCKET s)
	{
		WSAEVENT ev;

		{
			std::lock_guard<std::mutex> lock(mtx);

			auto it = events.find(s);

			if (it == events.end())
			{
				ev = WSACreateEvent();

				if (ev == WSA_INVALID_EVENT)
					return false;

				// 绑定socket事件
				if (WSAEventSelect(
					s,
					ev,
					FD_READ | FD_CLOSE) == SOCKET_ERROR)
				{
					WSACloseEvent(ev);
					return false;
				}

				events[s] = ev;
			}
			else
			{
				ev = it->second;
			}
		}


		DWORD ret = WSAWaitForMultipleEvents(
			1,
			&ev,
			FALSE,
			0,       // 不阻塞
			FALSE
		);


		if (ret == WSA_WAIT_EVENT_0)
		{
			WSANETWORKEVENTS ne{};

			WSAEnumNetworkEvents(
				s,
				ev,
				&ne
			);


			if (ne.lNetworkEvents & FD_READ)
				return true;


			if (ne.lNetworkEvents & FD_CLOSE)
				return true;
		}


		return false;
	}


	void remove(SOCKET s)
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto it = events.find(s);

		if (it != events.end())
		{
			WSACloseEvent(it->second);
			events.erase(it);
		}
	}


	~SocketEventChecker()
	{
		for (auto& p : events)
		{
			WSACloseEvent(p.second);
		}
	}
};