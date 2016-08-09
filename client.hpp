#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <string>
#include <vector>
#include "socket.hpp"
#include "server.hpp"

#define SCREENSHOT_PID      0x5C0EE250

#define CLIENT_CONNECTED  0x00000001
#define CLIENT_LOGGED_IN  0x00000002
#define CLIENT_AUTHORIZED 0x00000004
#define CLIENT_COMPLETE   0x00000008
#define CLIENT_PATCHFILE  0x00000010
#define CLIENT_SCREENSHOT 0x00000020

#define P_SERVER_LOST       0
#define P_SERVER_INVALID    3
#define P_CHARACTER_ABSENT  11
#define P_CHARACTER_PLAYING 12
#define P_UPDATE_ERROR      13
#define P_TOO_STRONG        14
#define P_TOO_WEAK          15
#define P_HAT_LOST          18
#define P_HAT_INVALID       19
#define P_WRONG_CREDENTIALS 20
#define P_LOGIN_EXISTS      21
#define P_LOGIN_BLOCKED     22
#define P_SERVER_OFFLINE    23
#define P_SERVER_FULL       25
#define P_CHARACTER_EXISTS  26
#define P_WRONG_NAME        27
#define P_SHORT_NAME        28
#define P_BAD_CHARACTER     29
#define P_S_TOO_STRONG      30
#define P_S_TOO_WEAK        31
#define P_TEAMPLAY_STARTED  32
#define P_SERVER_SHUTDOWN   33
#define P_IP_BLOCKED        34
#define P_NAME_EXISTS       35
#define P_WRONG_VERSION     36
#define P_OUTAGE            214
#define P_LOGIN_BLOCKED_FVR 219
#define P_BAD_GAMEMODE      220
#define P_RETRY_LATER       221
#define P_WRONG_GAMEMODE    222
#define P_FUCK_OFF          102
#define P_FHTAGN            118

#include "listener.hpp"
#include <fstream>

struct Client
{
    uint32_t Flags;
    SOCKET Socket;

    uint32_t Version;

    std::string HisIP;
    uint16_t HisPort;

    std::string HisAddr;

    uint32_t GameMode;

    PacketReceiver Receiver;
    std::string Login;
    uint32_t LoginID;

    uint32_t SessionID1;
    uint32_t SessionID2;
    std::string SessionNickname;
    Server* SessionServer;
    uint32_t SessionTime;

    uint32_t ClientID;
    uint32_t ClientKey;

    uint32_t HatID;

    std::ifstream PatchFile;
    uint32_t PatchPieces;
    uint32_t PatchPiece;
    uint32_t PatchSize;

    uint32_t JoinTime;

    bool IsBot;
    bool DoNotUnlock;
};

extern std::vector<Client*> Clients;

void Net_ProcessClients();

bool CL_AddConnection(SOCKET sock, sockaddr_in addr);
bool CL_Process(Client* conn);
void CL_Disconnect(Client* conn);

std::string TrimNickname(std::string nickname);
uint32_t CheckNickname(std::string nickname, int hatId, bool secondary = false);

bool CL_Login(Client* conn, Packet& pack);
bool CL_Authorize(Client* conn, Packet& pack);
bool CL_Character(Client* conn, Packet& pack);
bool CL_ServerList(Client* conn, Packet& pack);
bool CL_EnterServer(Client* conn, Packet& pack);
bool CL_CheckNickname(Client* conn, Packet& pack);
bool CL_DeleteCharacter(Client* conn, Packet& pack);
bool CL_PatchDownload(Client* conn, Packet& pack);
void CL_CheckCRC(Client* conn, Packet& pack);

bool CL_ServerProcess(Client* conn);
bool CL_TransferProcess(Client* conn);

void CLCMD_Kick(Client* conn, uint8_t message);
bool CLCMD_SendCharacterList(Client* conn);
bool CLCMD_SendCharacter(Client* conn, unsigned long id1, unsigned long id2);
bool CLCMD_SendServerList(Client* conn);
bool CLCMD_SendNicknameResult(Client* conn, unsigned long result);
bool CLCMD_EnterSuccess(Client* conn, unsigned long id1, unsigned long id2);
bool CLCMD_SendReconnect(Client* conn, unsigned long id1, unsigned long id2, std::string addr);

#endif // CLIENT_HPP_INCLUDED
