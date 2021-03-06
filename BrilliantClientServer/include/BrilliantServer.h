#pragma once

#include <iostream>
#include <memory>
#include "asio.hpp"
#include "detail/Connection.h"
#include "detail/Queue.h"
#include "detail/Message.h"

namespace BrilliantNetwork
{
	template<class T>
	class Server
	{
	public:
		Server(std::uint16_t iPort) : mAcceptor(mContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), iPort))
		{

		}

		virtual ~Server()
		{
			Stop();
		}

		bool Start()
		{
			try
			{
				WaitForClientConnection();
				mContextThread = std::thread([&]() { mContext.run(); });
			}
			catch (const std::exception& e)
			{
				std::cerr << "[SERVER] Exception: " << e.what() << '\n';
				return false;
			}

			std::cout << "Server connected\n";
			return true;
		}

		void Stop()
		{
			mContext.stop();
			if (mContextThread.joinable())
			{
				mContextThread.join();
			}
			std::cout << "Server stopped\n";
		}

		void WaitForClientConnection()
		{
			mAcceptor.async_accept([&](std::error_code ec, asio::ip::tcp::socket socket) {
				if (!ec)
				{
					std::cout << "[Server] New connection: " << socket.remote_endpoint() << '\n';
					auto pNewConnection = std::make_shared<Connection<T>>(Connection<T>::owner::server, mContext, std::move(socket), mMsgQueueIn);

					if (OnClientConnect(pNewConnection))
					{
						mConnections.push_back(std::move(pNewConnection));
						mConnections.back()->ConnectToClient(this, iIdCounter++);
						std::cout << "[" << mConnections.back()->GetId() << "] Connection approved\n";
					}
					else
					{
						std::cout << "Connection denied\n";
					}
				}
				else
				{
					std::cout << "[Server] New connection error: " << ec.message() << '\n';
				}

				WaitForClientConnection();
				});
		}

		void MessageClient(std::shared_ptr<Connection<T>> client, const Message<T>& msg)
		{
			if (client && client->IsConnected())
			{
				client->Send(msg);
			}
			else
			{
				OnClientDisconnect(client);
				client.reset();
				mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), client), mConnections.end());
			}
		}

		void MessageAllClients(const Message<T>& msg, std::shared_ptr<Connection<T>> ignore = nullptr)
		{
			bool bInvalidClientExists = false;
			for (auto& client : mConnections)
			{
				if (client && client->IsConnected())
				{
					if (client != ignore)
					{
						client->Send(msg);
					}
				}
				else
				{
					OnClientDisconnect();
					client.reset();
					bInvalidClientExists = true;
				}
			}

			if (bInvalidClientExists)
			{
				mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), nullptr), mConnections.end());
			}
		}

		void Update(std::size_t iMaxMsgs = -1, bool bWait = false)
		{
			if (bWait)
			{
				mMsgQueueIn.Wait();
			}

			std::size_t iNumMsgs = 0;
			while (iNumMsgs < iMaxMsgs && !mMsgQueueIn.Empty())
			{
				auto msg = mMsgQueueIn.PopFront();
				OnMessage(msg.conn, msg.msg);
				iNumMsgs++;
			}
		}

		virtual void OnClientValidated(std::shared_ptr<Connection<T>>  client)
		{

		}

	protected:
		virtual bool OnClientConnect(std::shared_ptr<Connection<T>> client)
		{
			return false;
		}

		virtual void OnClientDisconnect(std::shared_ptr<Connection<T>> client)
		{
			
		}

		virtual void OnMessage(std::shared_ptr<Connection<T>> client, Message<T>& msg)
		{

		}

		Queue<OwnedMessage<T>> mMsgQueueIn;
		std::deque<std::shared_ptr<Connection<T>>> mConnections;
		asio::io_context mContext;
		std::thread mContextThread;
		asio::ip::tcp::acceptor mAcceptor;
		std::uint32_t iIdCounter = 10000;
	};
}