#ifndef SERVER_HPP_INCLUDED
#define SERVER_HPP_INCLUDED

#include "listener.hpp"
#include "config.hpp"
#include "socket.hpp"
#include <windows.h>

#define SERVER_CONNECTED 0x00000001
#define SERVER_LOGGED_IN 0x00000002

#define SERVER_CAP_DETAILED_INFO 0x00000001
#define SERVER_CAP_FIXED_MAPLIST 0x00000002
#define SERVER_CAP_SOFTCORE      0x00000004

#define SVF_CLOSED	        0x00000001
#define SVF_PVM		        0x00000002
#define SVF_MUTED	        0x00000004
#define SVF_ADVPVM	        0x00000040
#define SVF_ONEMAP	        0x00000080
#define SVF_NOHEALING       0x00000100
#define SVF_NOOBSRV	        0x00000200
#define SVF_SOFTCORE        0x00000400
#define SVF_NODROP	        0x00000800
#define SVF_FNODROP	        0x00001000
#define SVF_NOSAVING        0x00002000
#define SVF_SANDBOX         0x00004000
#define SVF_ENTERMAGE       0x00008000
#define SVF_ENTERWARRIOR    0x00010000

struct ServerPlayer
{
    std::string Nickname;
    std::string Login;
    unsigned long Id1;
    unsigned long Id2;
    bool Connected;
    std::string IPAddress;
};

struct ServerInfo
{
    unsigned long ServerCaps;

    unsigned long Time;
    std::string MapName;
    unsigned long MapWidth;
    unsigned long MapHeight;
    unsigned long MapLevel;
    unsigned long PlayerCount;
    std::string Address;
    unsigned long GameMode;
    unsigned long ServerMode;

    std::vector<ServerPlayer> Players;
    std::vector<std::string> Locked;
};

struct Server;

struct ServerConnection
{
    SOCKET Socket;
    unsigned long Flags;
    unsigned long Version;
    unsigned long ID;

    bool Active;

    PacketReceiver Receiver;

    Server* Parent;
};

struct ServerLayer
{
    SOCKET Socket;

    struct LayerFlags
    {
        bool Connected;
        bool Active;
        bool ShuttingDown;
        bool Closed;
    } Flags;

    PacketReceiver Receiver;

    ServerLayer()
    {
        this->Flags.Connected = false;
        this->Flags.Active = false;
        this->Flags.ShuttingDown = false;
        this->Flags.Closed = true;
        this->Socket = NULL;
    }
};

struct Server
{
    unsigned long Number;
    std::string Name;
    std::string Address;
    unsigned short Port;
    std::string IAddress;
    unsigned short IPort;
    signed int HatId;

    ServerConnection* Connection;
    ServerLayer* Layer;

    ServerInfo Info;
    bool ShuttingDown;
};

extern std::vector<Server*> Servers;

bool SV_Login(ServerConnection* conn, Packet& pack);
bool SV_UpdateInfo(ServerConnection* conn, Packet& pack);
bool SV_UpdateInfo20(ServerConnection* conn, Packet& pack);
bool SV_Initialized20(ServerConnection* conn, Packet& pack);
bool SV_ReturnCharacter(ServerConnection* conn, Packet& pack);
bool SV_ConfirmClient(ServerConnection* conn, Packet& pack);
bool SV_RejectClient(ServerConnection* conn, Packet& pack);
unsigned long SV_TryClient(ServerConnection* conn, unsigned long id1, unsigned long id2, std::string login, std::string nickname, unsigned char sex);

bool SVCMD_Welcome(ServerConnection* conn);
bool SVCMD_ReceivedCharacter(ServerConnection* conn, std::string login);

bool SV_AddConnection(SOCKET socket, sockaddr_in addr);
bool SV_Process(Server* srv);
bool SV_ProcessLayer(Server* srv);
void SV_Disconnect(Server* srv);
void SV_DisconnectLayer(Server* srv);

bool SL_Initialized(Server* layer, Packet& pack);
bool SL_Shutdown(Server* layer, Packet& pack);
bool SL_UpdateInfo(Server* srv, Packet& pack);
bool SL_Broadcast(Server* srv, Packet& pack);

bool SLCMD_Screenshot(Server* srv, std::string login, uint32_t uid, bool done, std::string url);
bool SLCMD_MutePlayer(Server* srv, std::string login, uint32_t unmutedate);

void Net_ProcessServers();

#endif // SERVER_HPP_INCLUDED
