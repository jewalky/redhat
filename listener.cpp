#include "config.hpp"
#include "listener.hpp"
#include "socket.hpp"
#include "utils.hpp"

#include "client.hpp"
#include "server.hpp"

#include <winsock2.h>

SOCKET sv_listener = 0, cl_listener = 0;

void Net_Init()
{
	WSADATA wsd;
	WSAStartup(0x0101, &wsd);

    sv_listener = SOCK_Listen(Config::IHatAddress, Config::IHatPort);
    if(sv_listener == SERR_NOTCREATED)
    {
        Printf(LOG_FatalError, "[SC] Net_Listen: Failed to open listening socket for servers!\n");
        Printf(LOG_FatalError, "[SC] Net_Listen: On %s:%u.\n", Config::IHatAddress.c_str(), Config::IHatPort);
        exit(1);
    }

    cl_listener = SOCK_Listen(Config::HatAddress, Config::HatPort);
    if(cl_listener == SERR_NOTCREATED)
    {
        Printf(LOG_FatalError, "[SC] Net_Listen: Failed to open listening socket for clients!\n");
        Printf(LOG_FatalError, "[SC] Net_Listen: On %s:%u.\n", Config::HatAddress.c_str(), Config::HatPort);
        exit(1);
    }
}

void Net_Quit()
{
    SOCK_Destroy(cl_listener);
    SOCK_Destroy(sv_listener);
}

void Net_Listen()
{
    // check server
    if(SOCK_WaitEvent(sv_listener, 0))
    {
        sockaddr_in sv_addr;
        SOCKET sv_sock = SOCK_Accept(sv_listener, sv_addr);

        if(!SV_AddConnection(sv_sock, sv_addr))
            SOCK_Destroy(sv_sock);
    }

    // check client
    if(SOCK_WaitEvent(cl_listener, 0))
    {
        sockaddr_in cl_addr;
        SOCKET cl_sock = SOCK_Accept(cl_listener, cl_addr);

        if(!CL_AddConnection(cl_sock, cl_addr))
            SOCK_Destroy(cl_sock);
    }

    // process client
    Net_ProcessClients();

    // process server
    Net_ProcessServers();
}

PacketReceiver::PacketReceiver()
{
    Socket = 0;
}

PacketReceiver::~PacketReceiver()
{
    Queue.clear();
}

void PacketReceiver::Connect(SOCKET socket)
{
    Socket = socket;
}

bool PacketReceiver::Receive(uint32_t version)
{
    if(!Socket) return false;

    Packet pack;
    pack.Reset();

    if(!Queue.size()) Queue.push_back(pack);

    int zz = SOCK_ReceivePacket(Socket, pack, version);

    if(zz == SERR_TIMEOUT) return true;
    else if(zz == SERR_CONNECTION_LOST) return false;

    uint8_t* data = NULL;
    uint32_t size;
    pack.GetAllData(data, size);
    Queue.back().AppendData(data, size);
    delete[] data;

    if(zz == 0)
    {
        // create "next" packet
        Packet pack2;
        pack2.Reset();
        Queue.push_back(pack2);
    }

    return true;
}

bool PacketReceiver::GetPacket(Packet& pack)
{
    if(!Socket) return false;
    if(Queue.size() <= 1) return false;

    Packet pck2 = Queue.front();

    pack = pck2;
    Queue.erase(Queue.begin());

    return true;
}
