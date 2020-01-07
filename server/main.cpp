#include <WinSock2.h>
#include <iostream>
#include <list>
#include <vector>
#include <map>

#pragma comment(lib, "ws2_32.lib")

struct Player {
	std::string Name;
	SOCKET Socket;
};

std::list<Player> players;

bool select_recv(SOCKET sock, int interval_us = 1)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = interval_us;
	return (select(sock + 1, &fds, 0, 0, &tv) == 1);
}

bool check_accept(SOCKET listening, SOCKET* client_ptr)
{
	SOCKET client = INVALID_SOCKET;
	if (select_recv(listening))
	{
		client = accept(listening, nullptr, nullptr);
	}
	*client_ptr = client;
	return (client != INVALID_SOCKET);
}

bool check_recv(SOCKET client, std::vector<char>& buffer)
{
	int result = 0;
	if (select_recv(client))
	{
		const int MAX_SIZE = 512;
		buffer.resize(MAX_SIZE, 0);
		result = recv(client, &buffer[0], MAX_SIZE, 0);
		if (result < 0) 
		{
			return false;
		}
		buffer.resize(result);
		std::cout << "Receive " << result << "elements.\n";
		if (result == 0)
		{
			result = -1;
		}
	}
	return (result >= 0);
}

bool check_send(Player client, const std::map<SOCKET, std::vector<char>> &buffers)
{
	int result = 0;

	for (const auto&[user, buffer] : buffers) 
	{
		if (buffer.size() > 0 && user != client.Socket)
		{
			result = send(client.Socket, &buffer[0], buffer.size(), 0);
			std::cout << "Send " << buffer.size() << " elements.\n";
		}
	}

	
	return (result >= 0);
}

bool send_to_player(Player player, const std::string message)
{
	int result = 0;

	std::vector<char> m(message.begin(), message.end());
	result = send(player.Socket, &m[0], m.size(), 0);
	std::cout << "Send " << m.size() << " elements.\n";

	return (result >= 0);
}

bool send_to_all(const std::string message)
{
	int result = 0;

	std::vector<char> m(message.begin(), message.end());
	for (auto player : players)
	{
		result = send(player.Socket, &m[0], m.size(), 0);
		std::cout << "Send " << m.size() << " elements.\n";
	}

	return (result >= 0);
}

int main(int argc, char* argv[])
{
	WSADATA ws_data;
	if (WSAStartup(MAKEWORD(2, 0), &ws_data) != 0)
	{
		std::cerr << "cannot initialize winsock!\n";
		return -1;
	}

	SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
	if (listening == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket!\n";
		return -2;
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(42000);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR)
	{
		std::cerr << "Could not bind to address.\n";
		return -3;
	}

	if (listen(listening, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "Could not listen to socket.\n";
		return -4;
	}
	std::cout << "Wait for connection...\n";

	std::list<SOCKET> sockets;
	while(true)
	{
		SOCKET new_client;
		if (check_accept(listening, &new_client))
		{
			sockets.push_back(new_client);
			std::cout << "New client.\n";
		}

		for (auto client : sockets)
		{
			std::vector<char> buffer;
			if (!check_recv(client, buffer))
			{
				sockets.remove_if([client](SOCKET sock)
				{
					return sock == client;
				});
				closesocket(client);
				std::cout << "Client disconnected on recv.\n";
				break;
			}
			if (buffer.size() != 0)
			{
				Player player;
				player.Name = std::string(buffer.begin(), buffer.end());
				player.Socket = client;
				players.push_back(player);

				send_to_all(player.Name + " join the room");

				sockets.remove_if([client](SOCKET sock)
				{
					return sock == client;
				});
				break;
			}
		}

		std::map<SOCKET, std::vector<char>> buffers;
		for (auto player : players)
		{
			std::vector<char> buffer;
			if (!check_recv(player.Socket, buffer))
			{
				send_to_all(player.Name + " leave the room");
				players.remove_if([player](Player p)
				{
					return p.Socket == player.Socket;
				});
				closesocket(player.Socket);
				std::cout << "Client disconnected on recv.\n";
				break;
			}
			if (buffer.size() != 0)
			{
				std::vector<char> message(player.Name.begin(), player.Name.end());
				message.push_back(':');
				message.insert(message.end(), buffer.begin(), buffer.end());

				buffers.insert({ player.Socket, message });
			}
		}

		for (auto player : players)
		{
			if (buffers.size() != 0)
			{
				if (!check_send(player, buffers))
				{
					send_to_all(player.Name + " leave the room");
					players.remove_if([player](Player p)
					{
						return p.Socket == player.Socket;
					});
					closesocket(player.Socket);
					std::cout << "Client disconnected on send.\n";
					break;
				}
			}
		}
		Sleep(1);
	}

	closesocket(listening);
	WSACleanup();

	return 0;
}