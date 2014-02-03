#ifndef LISTENER_HPP_INCLUDED
#define LISTENER_HPP_INCLUDED

#include <vector>
#include "socket.hpp"

class PacketReceiver
{
    public:
        PacketReceiver();
        ~PacketReceiver();

        void Connect(SOCKET sock);
        bool Receive(uint32_t version);

        bool GetPacket(Packet& pack);

    private:
        std::vector<Packet> Queue;

        SOCKET Socket;
};

void Net_Init();
void Net_Quit();
void Net_Listen();

#endif // LISTENER_HPP_INCLUDED
