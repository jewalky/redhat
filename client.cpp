#include "client.hpp"
#include "utils.hpp"
#include <winsock2.h>
#include <ctime>

#include "config.hpp"
#include "login.hpp"
#include "session.hpp"
#include "version.hpp"
#include <algorithm>

#include "server.hpp"
#include "character.hpp"

#include "session.hpp"
#include "CRC_32.h"

std::vector<Client*> Clients;

bool CL_AddConnection(SOCKET socket, sockaddr_in addr)
{
    Client* cl = new Client();
    if(!cl) return false;

    cl->HisIP = inet_ntoa(addr.sin_addr);
    cl->HisPort = ntohs(addr.sin_port);
    cl->HisAddr = Format("%s:%u", cl->HisIP.c_str(), cl->HisPort);

    cl->Version = 0;
    cl->Flags = CLIENT_CONNECTED;

    cl->GameMode = 0;
    cl->Socket = socket;

    cl->Receiver.Connect(cl->Socket);

    cl->JoinTime = GetTickCount();
    cl->IsBot = false;

    Printf(LOG_Trivial, "[CL] %s - Connected.\n", cl->HisAddr.c_str());

    Clients.push_back(cl);
    return true;
}

bool CL_Process(Client* conn)
{
    if(!(conn->Flags & CLIENT_CONNECTED)) return false;
    if(!conn->Socket) return false;
    if(!conn->Receiver.Receive(conn->Version)) return false;
    if(GetTickCount()-conn->JoinTime > 5000 && !(conn->Flags & (CLIENT_LOGGED_IN|CLIENT_PATCHFILE)))
    {
        Printf(LOG_Warning, "[CL] %s - Client timed out.\n", conn->HisAddr.c_str());
        return false; // todo: add timeout to config
    }

    if((conn->Flags & CLIENT_LOGGED_IN) &&
       (conn->Flags & CLIENT_COMPLETE)) return CL_ServerProcess(conn);

    Packet pack;
    while(conn->Receiver.GetPacket(pack))
    {
        if(!(conn->Flags & CLIENT_LOGGED_IN))
        {
            if(!CL_Login(conn, pack)) return false;
            continue;
        }

        uint8_t packet_id;
        pack >> packet_id;
        pack.ResetPosition();

        switch(packet_id)
        {
            default:
                Printf(LOG_Error, "[CL] %s%s - Received unknown packet %02X.\n", conn->HisAddr.c_str(), (conn->Login.length() ? Format(" (%s)", conn->Login.c_str()).c_str() : ""), packet_id);
                return false;
            case 0xCA: // character request
                if(!CL_Character(conn, pack)) return false;
                break;
            case 0xC8: // server list request
                if(!CL_ServerList(conn, pack)) return false;
                break;
            case 0xCB: // enter server
                if(!CL_EnterServer(conn, pack)) return false;
                break;
            case 0x4E: // nickname check
                if(!CL_CheckNickname(conn, pack)) return false;
                break;
            case 0xCC: // delete character
                if(!CL_DeleteCharacter(conn, pack)) return false;
                break;
        }
    }

    return true;
}

bool CL_TransferProcess(Client* conn)
{

    return true;
}

bool CL_PatchDownload(Client* conn, Packet& pack)
{
    return true;
}

bool CL_ServerProcess(Client* conn)
{
    if(conn->IsBot)
    {
        CLCMD_Kick(conn, P_WRONG_VERSION);
        return false;
    }

    uint32_t dtime = time(NULL);
    if((dtime - conn->SessionTime) > 15)
    {
        SESSION_DelLogin(conn->SessionID1, conn->SessionID2);
        return false;
    }

    unsigned long result = SESSION_GetLogin(conn->SessionID1, conn->SessionID2);
    if(result == 0xFFFFFFFF) return true;

    SESSION_DelLogin(conn->SessionID1, conn->SessionID2);

    if(result == 0xBADFACE0) // db error
    {
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }
    else if(result == 0xBADFACE1) // server lost
    {
        CLCMD_Kick(conn, P_SERVER_LOST);
        return false;
    }
    else if(result == 0xBADFACE2) // hacking
    {
        CLCMD_Kick(conn, P_FUCK_OFF);
        return false;
    }

    std::string cas = "unknown";
    unsigned long retval = 0;
    switch(result)
    {
        case 1:
            cas = "server full";
            retval = P_SERVER_FULL;
            break;
        case 2:
            cas = "duplicated nickname";
            retval = P_CHARACTER_EXISTS;
            break;
        case 3:
            cas = "invalid nickname";
            retval = P_WRONG_NAME;
            break;
        case 4:
            cas = "too short nickname";
            retval = P_SHORT_NAME;
            break;
        case 5:
            cas = "invalid character data";
            retval = P_BAD_CHARACTER;
            break;
        case 6:
            cas = "too strong for this map";
            retval = P_S_TOO_STRONG;
            break;
        case 7:
            cas = "too weak for this map";
            retval = P_S_TOO_WEAK;
            break;
        case 8:
            cas = "teamplay already started";
            retval = P_TEAMPLAY_STARTED;
            break;
        case 9:
            cas = "shutdown initiated";
            retval = P_SERVER_SHUTDOWN;
            break;
        default: break;
    }

    if(retval != 0)
    {
        Printf(LOG_Error, "[CL] %s (%s) - Character \"%s\" rejected by server ID %u (reason: %s).\n", conn->HisAddr.c_str(), conn->Login.c_str(), conn->SessionNickname.c_str(), conn->SessionServer->Number, cas.c_str());
        CLCMD_Kick(conn, retval);
        return false;
    }

    Login_SetLocked(conn->Login, true, conn->SessionID1, conn->SessionID2, conn->SessionServer->Number);
    if(!CLCMD_EnterSuccess(conn, conn->SessionID1, conn->SessionID2))
        return false;
    Printf(LOG_Info, "[CL] %s (%s) - Character \"%s\" entered server ID %u.\n", conn->HisAddr.c_str(), conn->Login.c_str(), conn->SessionNickname.c_str(), conn->SessionServer->Number);
    return false;
}

void CL_Disconnect(Client* conn)
{
    Printf(LOG_Trivial, "[CL] %s%s - Disconnected.\n", conn->HisAddr.c_str(), (conn->Login.length() ? Format(" (%s)", conn->Login.c_str()).c_str() : ""));
    SOCK_Destroy(conn->Socket);
    conn->Socket = 0;
    conn->Flags &= ~CLIENT_CONNECTED;
}

void Net_ProcessClients()
{
    for(std::vector<Client*>::iterator it = Clients.begin(); it != Clients.end(); ++it)
    {
        Client* conn = (*it);
        if(!CL_Process(conn))
        {
            std::vector<Client*>::iterator do_it = it;
            it--;
            Clients.erase(do_it);

            CL_Disconnect(conn);
            delete conn;
        }
    }
}

void CL_VersionInfo(Client* conn, Packet& pack)
{
    uint16_t key1, key2;
    srand(time(NULL));
    key1 = rand();
    srand(key1);
    key2 = rand();
    uint32_t key = key1;
    key <<= 16;
    key |= key2;
    //conn->ClientKey = key;
    uint32_t sesskey = key ^ 0xDEADFACE;

    uint32_t sessid = V_AddSession(key) ^ sesskey;
    //conn->ClientID = sessid;
    Packet pck;
    pck << (uint8_t)0xFF;
    pck << sessid;
    pck << key;

    SOCK_SendPacket(conn->Socket, pck, conn->Version);
}

bool CL_Login(Client* conn, Packet& pack)
{
    std::ifstream ifs;
    ifs.open("redhat.ipf", std::ios::in);
    int32_t admin_level = 0;
    int32_t access_level = 0;
    std::string admin_name = "";
    IPFilter::IPFFile ipf;
    if(ifs.is_open())
    {
        std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ipf.ReadIPF(str);
        int32_t result = ipf.CheckAddress(conn->HisIP, admin_name);
        if(result == 2 || result == 3)
        {
            admin_level = result - 1;
        }
        else if(result == -10)
        {
            admin_level = -10;
        }
        access_level = result;
        ifs.close();
    }

    admin_name = Trim(admin_name);
    if(admin_level >= 1 && !admin_name.length()) admin_name = "unknown";

    // detect version
    uint8_t cr;
    pack >> cr;
    uint32_t uoth;
    pack >> uoth;
    pack.ResetPosition();
    uint8_t datatmp[0x41];
    pack.GetData(datatmp, 0x41);
    uint32_t utth;
    pack >> utth;
    pack.ResetPosition();

    if(cr == (0xFF ^ key_20[0])) // 2.0 version info, not auth at all
    {
        conn->Version = 20;
        PACKET_Crypt(pack, conn->Version);
        CL_VersionInfo(conn, pack);
        return false;
    }
    /*else if(cr == (0xFC ^ key_20[0])) // 2.0 patch download request
    {
        conn->Version = 20;
        conn->Flags |= CLIENT_PATCHFILE;
        PACKET_Crypt(pack, conn->Version);
        return CL_PatchDownload(conn, pack);
    }*/
    else if(cr == key_20[0]) // 2.0 auth
    {
        utth = utth ^ *(uint32_t*)(key_20+0x41);
        unsigned char ver = (utth & 0xFF000000) >> 24;
        if(ver >= 20) conn->Version = ver;
    }
    /*else if(cr == key_15[0])
    {
        utth = utth ^ *(uint32_t*)(key_15+0x41);
        unsigned char ver = (utth & 0xFF000000) >> 24;
        if(ver >= 15 && ver <= 19) conn->Version = ver;
    }*/ // absent protocol version
    else if(cr == key_11[0]) // 1.11 auth
    {
        utth = utth ^ *(uint32_t*)(key_11+0x41);
        unsigned char ver = (utth & 0xFF000000) >> 24;
        if(ver >= 11 && ver <= 15) conn->Version = ver;
    }
    else if(cr == (0x29 ^ key_10[0]))
    {
        uoth = uoth ^ *(uint32_t*)(key_10+1);
        if((uoth & 0x8C000000) == 0x8C000000)
            conn->Version = 10;
    }
    else if(cr == (0xC9 ^ key_08[0]))
    {
        uoth = uoth ^ *(uint32_t*)(key_08+1);
        unsigned char ver = (uoth & 0xFF000000) >> 24;
        if(ver > 0 && ver <= 7) conn->Version = ver;
        else if(ver == 0x0A5) conn->Version = 8;
        conn->Version = 8;
    }

    if(conn->Version == 0)
    {
        Printf(LOG_Error, "[CL] %s - Unknown client version (signature %02X).\n", conn->HisAddr.c_str(), cr);
        CLCMD_Kick(conn, P_WRONG_VERSION);
        return false;
    }

    PACKET_Crypt(pack, conn->Version);

    uint8_t packet_id;
    pack >> packet_id;
    if(conn->Version >= 11) pack.GetData(datatmp, 0x40);
    pack >> uoth;

    uint8_t prc_uuid[20];
    memset(prc_uuid, 0, 20);

    // check version
    if(conn->Version >= 20)
    {
        uint32_t prc_allods2_exe = *(uint32_t*)(datatmp),
                 prc_a2mgr_dll = *(uint32_t*)(datatmp + 0x04),
                 prc_patch_res = *(uint32_t*)(datatmp + 0x08),
                 prc_world_res = *(uint32_t*)(datatmp + 0x0C),
                 prc_graphics_res = *(uint32_t*)(datatmp + 0x10),
                 prc_sessid = *(uint32_t*)(datatmp + 0x14);

        uint32_t lrc_sessid = prc_sessid ^ 0xDEADBEEF;
        uint32_t net_key = 0x6CDB248D;//V_GetSession(lrc_sessid);

        memcpy(prc_uuid, datatmp + 0x18, 20);
        for(int i = 0; i < 5; i++)
            *(uint32_t*)(prc_uuid + i * 4) ^= net_key;

        CRC_32 crc;
        uint32_t prc_uuid_crc = crc.CalcCRC(prc_uuid, 20);

        uint32_t prc_uuid_crc_original = *(uint32_t*)(datatmp + 0x2C) ^ net_key;
        if(prc_uuid_crc_original != prc_uuid_crc)
        {
            Printf(LOG_Error, "[CL] %s - Hacking: UUID has been tampered with.\n", conn->HisAddr.c_str());
            CLCMD_Kick(conn, P_FHTAGN);
            return false;
        }

        uint32_t lrc_allods2_exe = prc_allods2_exe ^ net_key,
                 lrc_a2mgr_dll = (prc_a2mgr_dll - lrc_allods2_exe) ^ net_key,
                 lrc_patch_res = prc_patch_res ^ lrc_a2mgr_dll,
                 lrc_world_res = (prc_world_res + lrc_patch_res) ^ net_key,
                 lrc_graphics_res = (prc_graphics_res ^ net_key) - lrc_allods2_exe;

        std::string crcstr = "";
        bool badcrc = false;

        if(std::find(Config::ExecutableCRC.begin(), Config::ExecutableCRC.end(), lrc_allods2_exe) == Config::ExecutableCRC.end())
        {
            badcrc = true;
            crcstr += Format("allods2.exe: %08X", lrc_allods2_exe);
        }

        if(std::find(Config::LibraryCRC.begin(), Config::LibraryCRC.end(), lrc_a2mgr_dll) == Config::LibraryCRC.end())
        {
            if(badcrc) crcstr += "; ";
            crcstr += Format("a2mgr.dll: %08X", lrc_a2mgr_dll);
            badcrc = true;
        }

        if(std::find(Config::PatchCRC.begin(), Config::PatchCRC.end(), lrc_patch_res) == Config::PatchCRC.end())
        {
            if(badcrc) crcstr += "; ";
            crcstr += Format("patch.res: %08X", lrc_patch_res);
            badcrc = true;
        }

        if(std::find(Config::WorldCRC.begin(), Config::WorldCRC.end(), lrc_world_res) == Config::WorldCRC.end())
        {
            if(badcrc) crcstr += "; ";
            crcstr += Format("world.res: %08X", lrc_world_res);
            badcrc = true;
        }

        /*if(std::find(Config::GraphicsCRC.begin(), Config::GraphicsCRC.end(), lrc_graphics_res) == Config::GraphicsCRC.end())
        {
            if(badcrc) crcstr += "; ";
            crcstr += Format("graphics.res: %08X", lrc_graphics_res);
            badcrc = true;
        }*/

        V_DelSession(lrc_sessid);

        if(badcrc)
        {
            Printf(LOG_Error, "[CL] %s - Client CRC mismatch (%s).\n", conn->HisAddr.c_str(), crcstr.c_str());
            if(admin_level < 1)
            {
                CLCMD_Kick(conn, P_WRONG_VERSION);
                return false;
            }
            else Printf(LOG_Info, "[CL] %s - Admin access used (auth: %s)\n", conn->HisAddr.c_str(), admin_name.c_str());
        }
    }

    std::string uuid = "";
    for(int i = 0; i < 20; i++)
        uuid += Format("%02x", prc_uuid[i]);

    if(conn->Version != Config::ProtocolVersion)
    {
        if(admin_level == -10) // ex-lend
        {
            Printf(LOG_Warning, "[CL] %s - Switching to bot mode...\n", conn->HisAddr.c_str());
            conn->IsBot = true;
        }
        else
        {
            Printf(LOG_Error, "[CL] %s - Client connected with wrong protocol version (%u).\n", conn->HisAddr.c_str(), conn->Version);
            CLCMD_Kick(conn, P_WRONG_VERSION);
            return false;
        }
    }

    uint8_t p_version = (uoth & 0xFF000000) >> 24;
    uint8_t p_gamemode = (uoth & 0x00FF0000) >> 16;
    uint16_t p_loginlen = (uoth & 0x0000FFFF);

    if(p_gamemode != GAMEMODE_Arena && p_gamemode != GAMEMODE_Cooperative && p_gamemode != GAMEMODE_Softcore)
    {
        Printf(LOG_Error, "[CL] %s - Bad game mode %u.\n", conn->HisAddr.c_str(), p_gamemode);
        CLCMD_Kick(conn, P_BAD_GAMEMODE);
        return false;
    }

    if(conn->IsBot) // ex-lend
    {
        conn->GameMode = p_gamemode;
        conn->Flags |= CLIENT_LOGGED_IN;
        return CLCMD_SendCharacterList(conn);
    }

    std::string logstring;
    pack >> logstring;

    std::string s_login = logstring;
    s_login.erase(p_loginlen);
    std::string s_password = logstring;
    s_password.erase(0, p_loginlen);

    if(access_level == -1)
    {
        Printf(LOG_Error, "[CL] %s (%s) - IP blocked by global rules.\n", conn->HisAddr.c_str(), s_login.c_str());
        CLCMD_Kick(conn, P_FHTAGN); // "Ктулху фхтагн!"
        return false;
    }

    // 09.09.2013 - added check for UUID
    int32_t result = ipf.CheckUUID(uuid);
    access_level = result;

    if(access_level == -100)
    {
        Printf(LOG_Error, "[CL] %s (%s) - UUID blocked by global rules.\n", conn->HisAddr.c_str(), s_login.c_str());
        Printf(LOG_Info, "[CL] %s (%s) - UUID: %s.\n", conn->HisAddr.c_str(), s_login.c_str(), uuid.c_str());
        CLCMD_Kick(conn, P_FHTAGN);
        return false;
    }

    if(!Login_Exists(s_login))
    {
        if(Config::AutoRegister && Login_Create(s_login, s_password))
            Printf(LOG_Info, "[CL] %s - Auto-registered login %s.\n", conn->HisAddr.c_str(), s_login.c_str(), s_login.c_str());
        else
        {
            Printf(LOG_Error, "[CL] %s - Tried to open non-existent login %s.\n", conn->HisAddr.c_str(), s_login.c_str());
            CLCMD_Kick(conn, P_WRONG_CREDENTIALS);
            return false;
        }
    }

    std::string l_ipf;
    if(!Login_GetIPF(s_login, l_ipf))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetIPF(\"%s\", <ipf>).\n", s_login.c_str());
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    if(l_ipf.length())
    {
        IPFilter::IPFFile ipf;
        ipf.ReadIPF(l_ipf);
        if(ipf.CheckAddress(conn->HisIP, admin_name) != 1)
        {
            if(CheckInt(s_login) && (admin_level >= 1))
            {
                Printf(LOG_Warning, "[CL] %s (%s) - IP blocked by local rules, GM access used (auth: %s)\n", conn->HisAddr.c_str(), s_login.c_str(), admin_name.c_str());
            }
            else if(!CheckInt(s_login) && (admin_level >= 2))
            {
                Printf(LOG_Warning, "[CL] %s (%s) - IP blocked by local rules, admin access used (auth: %s)\n", conn->HisAddr.c_str(), s_login.c_str(), admin_name.c_str());
            }
            else if(!admin_level)
            {
                Printf(LOG_Error, "[CL] %s (%s) - IP blocked by local rules.\n", conn->HisAddr.c_str(), s_login.c_str());
                CLCMD_Kick(conn, P_IP_BLOCKED);
                return false;
            }
        }
    }

    std::string passwd_1;
    if(!Login_GetPassword(s_login, passwd_1))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetPassword(\"%s\", <password>).\n", s_login.c_str());
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    std::string passwd_2 = Login_MakePassword(s_password);
    if(passwd_1 != passwd_2)
    {
        if(CheckInt(s_login) && (admin_level >= 1))
        {
            Printf(LOG_Warning, "[CL] %s (%s) - Password mismatch, GM access used (auth: %s)\n", conn->HisAddr.c_str(), s_login.c_str(), admin_name.c_str());
        }
        else if(!CheckInt(s_login) && (admin_level >= 2))
        {
            Printf(LOG_Warning, "[CL] %s (%s) - Password mismatch, admin access used (auth: %s)\n", conn->HisAddr.c_str(), s_login.c_str(), admin_name.c_str());
        }
        else
        {
            Printf(LOG_Error, "[CL] %s (%s) - Password mismatch.\n", conn->HisAddr.c_str(), s_login.c_str());
            CLCMD_Kick(conn, P_WRONG_CREDENTIALS);
            return false;
        }
    }

    bool l_locked;
    unsigned long l_id1, l_id2, l_srvid;
    if(!Login_GetLocked(s_login, l_locked, l_id1, l_id2, l_srvid))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetLocked(\"%s\", <locked>, <id1>, <id2>, <srvid>).\n", s_login.c_str());
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    unsigned long ban_time, unban_time;
    std::string ban_reason;
    bool ban_active;
    unsigned long ctime = time(NULL);
    if(!Login_GetBanned(s_login, ban_active, ban_time, unban_time, ban_reason))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetBanned(\"%s\", <banned>, <date_ban>, <date_unban>, <reason>).\n", s_login.c_str());
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    if(ban_active)
    {
        bool ban_intime = false;

        if((ban_time > unban_time) || (unban_time > 0x7FFFFFFF))
        {
            Printf(LOG_Error, "[CL] %s (%s) - Login banned forever (reason: %s).\n", conn->HisAddr.c_str(), s_login.c_str(), ban_reason.c_str());
            if(conn->Version >= 20 && conn->Version <= 10) CLCMD_Kick(conn, P_LOGIN_BLOCKED_FVR);
            else CLCMD_Kick(conn, P_LOGIN_BLOCKED);
            ban_intime = true;
        }
        else if(ctime < unban_time)
        {
            if(ban_time > ctime) Printf(LOG_Warning, "[CL] %s (%s) - Ban date is bigger than current date (by %us)!\n", conn->HisAddr.c_str(), s_login.c_str(), ban_time - ctime);

            Printf(LOG_Error, "[CL] %s (%s) - Login banned (reason: %s).\n", conn->HisAddr.c_str(), s_login.c_str(), ban_reason.c_str());
            CLCMD_Kick(conn, P_LOGIN_BLOCKED);
            ban_intime = true;
        }

        if(!ban_intime)
        {
            if(!Login_SetBanned(s_login, false, 0, 0, ""))
            {
                Printf(LOG_Error, "[DB] Error: Login_SetBanned(\"%s\", false, 0, 0, \"\").\n", s_login.c_str());
                CLCMD_Kick(conn, P_UPDATE_ERROR);
                return false;
            }
        }
        else return false;
    }

    if(l_locked)
    {
        bool r_cancel_lock = false;
        for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
        {
            Server* srv = (*it);
            if(!srv) continue;

            if(srv->Number == l_srvid)
            {
                if(!srv->Connection || !srv->Connection->Active)
                {
                    Printf(LOG_Error, "[CL] %s (%s) - Locked on offline server ID %u!\n", conn->HisAddr.c_str(), s_login.c_str(), srv->Number);
                    CLCMD_Kick(conn, P_SERVER_OFFLINE);
                    return false;
                }

                if((((srv->Info.ServerMode & SVF_SOFTCORE) == SVF_SOFTCORE) != (p_gamemode == GAMEMODE_Softcore)) || (((srv->Info.ServerMode & SVF_SOFTCORE) != SVF_SOFTCORE) && (srv->Info.GameMode != p_gamemode)))
                {
                    Printf(LOG_Error, "[CL] %s (%s) - Locked on server ID %u with different game mode (%u != %u)!\n", conn->HisAddr.c_str(), s_login.c_str(), srv->Number, p_gamemode, srv->Info.GameMode);
                    CLCMD_Kick(conn, P_WRONG_GAMEMODE);
                    return false;
                }

                if(srv->Info.ServerCaps & SERVER_CAP_DETAILED_INFO)
                {
                    bool char_on_server = false;
                    for(std::vector<ServerPlayer>::iterator jt = srv->Info.Players.begin(); jt != srv->Info.Players.end(); ++jt)
                    {
                        ServerPlayer& player = (*jt);
                        if(player.Login == s_login && player.Id1 == l_id1 && player.Id2 == l_id2)
                            char_on_server = true;
                    }

                    for(std::vector<std::string>::iterator jt = srv->Info.Locked.begin(); jt != srv->Info.Locked.end(); ++jt)
                    {
                        std::string& login = (*jt);
                        if(login == s_login)
                            char_on_server = true;
                    }

                    if(!char_on_server && srv->Info.Time <= 15) char_on_server = true;

                    if(!char_on_server)
                    {
                        Printf(LOG_Info, "[CL] %s (%s) - Login lock dropped (not on server ID %u).\n", conn->HisAddr.c_str(), s_login.c_str(), srv->Number);
                        r_cancel_lock = true;
                    }
                    /// ДЮП!!!!!
                    // todo: пофиксить проверку логинов на сервере!
                    /// сервера сохраняют персонажей напрямую в базу, вернули на место
                }

                if(r_cancel_lock)
                {
                    l_locked = false;
                    if(!Login_SetLocked(s_login, false, 0, 0, 0))
                    {
                        Printf(LOG_Error, "[DB] Error: Login_SetLocked(\"%s\", <locked>, <id1>, <id2>, <srvid).\n", s_login.c_str());
                        CLCMD_Kick(conn, P_UPDATE_ERROR);
                        return false;
                    }
                    break;
                }

                char* c_data = NULL;
                unsigned long c_size = 0;
                std::string c_nickname;
                if(!Login_GetCharacter(s_login, l_id1, l_id2, c_size, c_data, c_nickname) || !c_data)
                {
                    Printf(LOG_Error, "[DB] Error: Login_GetCharacter(\"%s\", %u, %u, <size>, <data>, <nickname>).\n", s_login.c_str(), l_id1, l_id2);
                    CLCMD_Kick(conn, P_UPDATE_ERROR);
                    return false;
                }

                if(c_size != 0x30)
                {
                    Character chr;
                    chr.LoadFromBuffer(c_data, c_size);
                    delete[] c_data;

                    c_nickname = chr.Nick;
                    if(chr.Clan.length()) c_nickname += "|" + chr.Clan;
                }
                else
                {
                    char* nik = new char[(unsigned char)c_data[4] + 1];
                    nik[(unsigned char)c_data[4]] = 0;
                    memcpy(nik, c_data + 20, (unsigned char)c_data[4]);

                    c_nickname = std::string(nik);
                    delete[] nik;
                }

                if(!CLCMD_SendReconnect(conn, l_id1, l_id2, Format("%s:%u", srv->Address.c_str(), srv->Port)))return false;
                Printf(LOG_Info, "[CL] %s (%s) - Character \"%s\" entered server ID %u (reconnected).\n", conn->HisAddr.c_str(), s_login.c_str(), c_nickname.c_str(), srv->Number);
                return false;
            }
        }

        if(!r_cancel_lock)
        {
            Printf(LOG_Error, "[CL] %s (%s) - Login locked on invalid server ID %u!\n", conn->HisAddr.c_str(), s_login.c_str(), l_srvid);
            CLCMD_Kick(conn, P_SERVER_INVALID);
            return false;
        }
    }

    Printf(LOG_Info, "[CL] %s (%s) - Logged in successfully.\n", conn->HisAddr.c_str(), s_login.c_str());
    Printf(LOG_Info, "[CL] %s (%s) - UUID: %s.\n", conn->HisAddr.c_str(), s_login.c_str(), uuid.c_str());
    conn->Login = s_login;
    conn->GameMode = p_gamemode;
    conn->Flags |= CLIENT_LOGGED_IN;
    return CLCMD_SendCharacterList(conn);
}

bool CL_Character(Client* conn, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xCA) return false;

    uint32_t id1 = 0, id2 = 0;
    pack >> id1 >> id2;
    return CLCMD_SendCharacter(conn, id1, id2);
}

bool CL_Authorize(Client* conn, Packet& pack)
{
    return false;
}

void CLCMD_Kick(Client* conn, uint8_t reason)
{
    if(reason == P_FUCK_OFF && conn->Version >= 20)
        reason = P_FHTAGN;

    Packet pack;
    pack << (uint8_t)0x0B;
    pack << reason;
    pack << (uint32_t)0;
    SOCK_SendPacket(conn->Socket, pack, conn->Version);
}

bool CLCMD_SendCharacterList(Client* conn)
{
    std::vector<CharacterInfo> chars;
    if(!conn->IsBot)
    {
        if(!Login_GetCharacterList(conn->Login, chars, (conn->GameMode == GAMEMODE_Softcore)))
        {
            Printf(LOG_Error, "[DB] Error: Login_GetCharacterList(\"%s\", <info>).\n", conn->Login.c_str());
            CLCMD_Kick(conn, P_UPDATE_ERROR);
            return false;
        }
    }

    Packet pack;
    pack << (uint8_t)0xCE;
    pack << (uint32_t)chars.size() * 8 + 4;
    pack << (uint32_t)Config::HatID;
    for(size_t i = 0; i < chars.size(); i++)
    {
        pack << (uint32_t)chars[i].ID1;
        pack << (uint32_t)chars[i].ID2;
    }

    return (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
}

bool CLCMD_SendCharacter(Client* conn, unsigned long id1, unsigned long id2)
{
    if(conn->IsBot)
    {
        CLCMD_Kick(conn, P_WRONG_VERSION);
        return false;
    }

    char* data;
    unsigned long size;
    std::string nickname;

    if(!Login_GetCharacter(conn->Login, id1, id2, size, data, nickname, (conn->GameMode == 2)) || !data)
    {
        Printf(LOG_Error, "[DB] Error: Login_GetCharacter(\"%s\", %u, %u, <size>, <data>, <nickname>).\n", conn->Login.c_str(), id1, id2);
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    Packet pack;
    pack << (uint8_t)0xCF;
    pack << (uint32_t)size + 8;
    pack << (uint32_t)id1;
    pack << (uint32_t)id2;
    pack.AppendData((uint8_t*)data, (uint32_t)size);

    delete[] data;

    int ss = SOCK_SendPacket(conn->Socket, pack, conn->Version);
    return (ss == 0);
}

bool CL_ServerList(Client* conn, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xC8) return false;

    return CLCMD_SendServerList(conn);
}

bool CLCMD_SendServerList(Client* conn)
{
    Packet pack;
    pack << (uint8_t)0xCD;
    std::string list = "";
    unsigned long totalcnt = 0, currentcnt = 0;
    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        totalcnt++;

        Server* srv = (*it);
        if(!srv) continue;
        if(!srv->Connection) continue;
        if(!srv->Connection->Active) continue;
        if(srv->Info.GameMode != conn->GameMode &&
           conn->GameMode != GAMEMODE_Softcore) continue;
        if((conn->GameMode == GAMEMODE_Softcore) !=
           ((srv->Info.ServerMode & SVF_SOFTCORE) == SVF_SOFTCORE)) continue;

        std::string srv_name = srv->Name.c_str();
        std::string map_name = srv->Info.MapName;
        if(srv->Info.ServerCaps & SERVER_CAP_DETAILED_INFO)
        {
            unsigned long tme = srv->Info.Time;
            unsigned long tm_h = tme / 3600;
            unsigned long tm_m = tme / 60 - tm_h * 60;
            //unsigned long tm_s = tme - (tm_h * 3600 + tm_m * 60);
            srv_name = Format("[%u:%02u] %s", tm_h, tm_m, srv_name.c_str());

            if(srv->Info.ServerMode & 2)
                map_name = "PvM: " + map_name;
        }

        list += Format("|%s|1.02|%s|%ux%u|%u|%u|%s:%u\n", srv_name.c_str(),
                                                          map_name.c_str(),
                                                          srv->Info.MapWidth,
                                                          srv->Info.MapHeight,
                                                          srv->Info.MapLevel,
                                                          srv->Info.PlayerCount,
                                                          srv->Address.c_str(),
                                                          srv->Port);
        currentcnt++;
    }
    list = Format("CURRENTCOUNT|%2u$BREAK\nTOTALSERVERS|%2u$BREAK\n\n", currentcnt, totalcnt) + list;
    pack << (uint32_t)list.length();
    pack.AppendData((uint8_t*)list.c_str(), (uint32_t)list.length()+1);

    (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
    return true;
}

uint32_t CheckNickname(std::string nickname, bool softcore)
{
    size_t w = nickname.find_first_of('|');
    if(w == nickname.npos)
        w = nickname.length();

    bool in_clan = false;

    if(nickname[nickname.length()-1] == '|')
        return P_WRONG_NAME;

    unsigned long result = 0;
    if(w < 3) result = P_SHORT_NAME;
    else if(w > 10) result = P_BAD_CHARACTER;
    else
    {
        for(size_t i = 0; i < nickname.length(); i++)
        {
            uint8_t ch = nickname[i];
            if(ch == '|' && !in_clan)
            {
                in_clan = true;
                continue;
            }

            if(ch < 0x20)
                return P_WRONG_NAME;

            bool allowed = ((ch >= 0x30 && ch <= 0x39) || // 0..9
                            (ch >= 0x41 && ch <= 0x5A) || // A..Z
                            (ch >= 0x61 && ch <= 0x7A) || // a..z
                            (ch == '-') ||
                            (ch == ' ') ||
                            (ch >= 0x80 && ch <= 0x9F) || // А..Я
                            (ch >= 0xA0 && ch <= 0xAF) || // а..п
                            (ch >= 0xE0 && ch <= 0xEF) || // р..я
                            (ch == 0xF0 || ch == 0xF1) || // Ё ё
                            (ch == '!' || ch == '?') ||
                            (ch == '@') ||
                            (ch == '.') ||
                            (ch == '=') ||
                            (ch == '+') ||
                            (ch == '_') ||
                            (ch == '$') ||
                            (ch == '(') || (ch == ')') ||
                            (in_clan && ( // ТОЛЬКО В ПРИПИСАХ
                                (ch == 0x7F) || // 0x7F квадрат
                                (ch == '{' || ch == '}' || ch == '|') ||
                                (ch == '[' || ch == ']') ||
                                (ch == ':' || ch == ';') ||
                                (ch == '*' || ch == '^') ||
                                (ch == '#') ||
                                (ch == '<' || ch == '>') ||
                                (ch == '&'))));

            if(!allowed) return P_WRONG_NAME;
        }
    }

    if(w < nickname.length())
        nickname.erase(w);
    if(Login_NickExists(nickname, softcore)) result = P_NAME_EXISTS;
    return result;
}

bool CL_CheckEasyQuestItem(const CItem& item)
{
    switch(item.Id)
    {
        case 0x0000:    // None
        case 0x0813:    // Iron Plate Cuirass
        case 0x0C1E:    // Iron Plate Boots
        case 0x0916:    // Iron Plate Bracers
        case 0x0A1A:    // Iron Scale Gauntlets
        case 0xB70F:    // Hard Leather Mail
        case 0xF72D:    // Uncommon Robe
        case 0xF82B:    // Uncommon Cloak
        case 0x812D:    // Uncommon Wood Staff
        case 0x810D:    // Wood Staff
        case 0x0104:    // Iron Long Sword
        case 0x0112:    // Iron Axe
        case 0x0109:    // Iron Mace
        case 0x010F:    // Iron Pike
            break;
        default:
            return false;
    }

    if(item.Id == 0x810D ||
       item.Id == 0x812D)
    {
        if(item.Effects.size() != 1)
            return false;
        if(item.Effects[0].Id1 != 0x29) return false;
        if(item.Effects[0].Value2 > 20) return false;
        switch(item.Effects[0].Value1)
        {
            case 1:
            case 5:
            case 10:
            case 16:
                break;
            default:
                return false;
        }
    }
    else if(item.Effects.size())
        return false;

    return true;
}

float CL_NeedPointsFrom(uint8_t what)
{
    if(what >= 43)
        return 16;
    if(what >= 42)
        return 15;
    if(what >= 41)
        return 12;
    if(what >= 39)
        return 10;
    if(what >= 38)
        return 8;
    if(what >= 37)
        return 7;
    if(what >= 36)
        return 6;
    if(what >= 35)
        return 5;
    if(what >= 32)
        return 4;
    if(what >= 30)
        return 3;
    if(what >= 27)
        return 2;
    if(what >= 20)
        return 1;
    if(what >= 16)
        return 0.5;
    return 0;
}

bool CL_EnterServer(Client* conn, Packet& pack)
{
    if(conn->IsBot)
    {
        CLCMD_Kick(conn, P_WRONG_VERSION);
        return false;
    }

    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xCB) return false;
    uint32_t p_szu = 0;
    pack >> p_szu;
    uint32_t p_id1 = 0, p_id2 = 0;
    pack >> p_id1 >> p_id2;
    uint8_t p_body, p_reaction, p_mind, p_spirit, p_base, p_picture, p_sex;
    pack >> p_body >> p_reaction >> p_mind >> p_spirit >> p_base >> p_picture >> p_sex;
    uint8_t p_nicklen;
    pack >> p_nicklen;
    char* p_nickname_c = new char[p_nicklen + 1];
    p_nickname_c[p_nicklen] = 0;
    pack.GetData((uint8_t*)p_nickname_c, p_nicklen);
    std::string p_nickname(p_nickname_c);
    delete[] p_nickname_c;
    uint32_t p_srvlen = p_szu - p_nicklen - 16;
    char* p_srvname_c = new char[p_srvlen + 1];
    p_srvname_c[p_srvlen] = 0;
    pack.GetData((uint8_t*)p_srvname_c, p_srvlen);
    std::string p_srvname(p_srvname_c);
    delete[] p_srvname_c;

    bool l_locked;
    unsigned long l_id1, l_id2, l_srvid;
    if(!Login_GetLocked(conn->Login, l_locked, l_id1, l_id2, l_srvid))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetLocked(\"%s\", <locked>, <id1>, <id2>, <srvid>).\n", conn->Login.c_str());
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    if(l_locked)
    {
        CLCMD_Kick(conn, P_FUCK_OFF);
        return false;
    }

    bool is_created = !Login_CharExists(conn->Login, p_id1, p_id2);

    if(is_created)
    {
        if(CheckNickname(p_nickname, (conn->GameMode == GAMEMODE_Softcore)) != 0)
        {
            Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: tried to create bad nickname \"%s\"!\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str());
            CLCMD_Kick(conn, P_FUCK_OFF);
            return false;
        }

        static const uint8_t pics[] = {
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
            0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2F, 0x30, 0x31,
            0x32, 0x33, 0x34, 0x05, 0x02, 0x03, 0x04, 0x01,
            0x06, 0x07, 0x08, 0x09, 0x0A, 0x16, 0x17, 0x18,
            0x19, 0x1F,
            0x4F, 0x51, 0x52, 0x43, 0x42, 0x41, 0x44, 0x48,
            0x49,
            0x8B, 0x8C, 0x91, 0x81, 0x82, 0x83, 0x84, 0x85,
            0x86, 0x87, 0x8A,
            0xC6, 0xC8, 0xC9, 0xCF, 0xD0, 0xD2, 0xD4, 0xC1,
            0xC2, 0xC3, 0xC5,
        };

        if((p_id2 & 0x3F000000) == 0x3F000000)
        {
            Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: tried to create GM character from \"%s\" (rights: %08X)!\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), p_id2);
            CLCMD_Kick(conn, P_FUCK_OFF);
            return false;
        }

        if((p_body < 15 || p_reaction < 15 || p_mind < 15 || p_spirit < 15) ||
           (p_body + p_reaction + p_mind + p_spirit > 136))
        {
            Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: tried to create character \"%s\" with invalid stats (%u, %u, %u, %u)!\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), p_body, p_reaction, p_mind, p_spirit);
            CLCMD_Kick(conn, P_FUCK_OFF);
            return false;
        }

        if(p_base < 1 || p_base > 4)
        {
            Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: tried to create character \"%s\" with invalid base skill %u!\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), p_base);
            CLCMD_Kick(conn, P_FUCK_OFF);
            return false;
        }

        if(!memchr(pics, p_picture, sizeof(pics)))
        {
            switch((p_picture & 0x40) >> 6)
            {
                case 0: p_picture = 0x20 | 0x00;
                case 1: p_picture = 0x0F | 0x40;
                case 2: p_picture = 0x0B | 0x80;
                case 3: p_picture = 0x06 | 0xC0;
            }
        }

        char* data = new char[0x30];
        memset(data, 0, 0x30);
        *(unsigned long*)(data) = 0xFFDDAA11;
        *(unsigned char*)(data + 4) = p_nickname.length();
        *(unsigned char*)(data + 5) = p_body;
        *(unsigned char*)(data + 6) = p_reaction;
        *(unsigned char*)(data + 7) = p_mind;
        *(unsigned char*)(data + 8) = p_spirit;
        *(unsigned char*)(data + 9) = p_base;
        *(unsigned char*)(data + 10) = p_picture;
        *(unsigned char*)(data + 11) = p_sex;
        *(unsigned long*)(data + 12) = p_id1;
        *(unsigned long*)(data + 16) = p_id2;
        memcpy(data + 20, p_nickname.c_str(), p_nickname.length());

        if(!Login_SetCharacter(conn->Login, p_id1, p_id2, 0x30, data, p_nickname))
        {
            delete[] data;
            Printf(LOG_Error, "[DB] Error: Login_SetCharacter(\"%s\", %u, %u, 0x30, <data>, \"%s\").\n", conn->Login.c_str(), p_id1, p_id2, p_nickname.c_str());
            CLCMD_Kick(conn, P_UPDATE_ERROR);
            return false;
        }

        delete[] data;
        Printf(LOG_Info, "[CL] %s (%s) - Created character \"%s\".\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str());
    }
    else
    {
        if(CheckNickname(p_nickname, (conn->GameMode == GAMEMODE_Softcore)) == P_WRONG_NAME)
        {
            Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: tried to join with bad nickname \"%s\"!\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str());
            CLCMD_Kick(conn, P_WRONG_NAME);
            return false;
        }
    }

    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if(!srv) continue;
        if(!srv->Connection) continue;
        else if(!srv->Connection->Active) continue;

        std::string addr = Format("%s:%u", srv->Address.c_str(), srv->Port);
        if(addr == p_srvname)
        {
            if(!is_created && ((p_id2 & 0x3F000000) != 0x3F000000))
            {
                CCharacter chrtc;
                if(!Login_GetCharacter(conn->Login, p_id1, p_id2, chrtc))
                {
                    Printf(LOG_Error, "[DB] Error: Login_GetCharacter(\"%s\", %u, %u, <character>).\n", conn->Login.c_str(), p_id1, p_id2);
                    CLCMD_Kick(conn, P_UPDATE_ERROR);
                    return false;
                }

                bool srv_softcore = ((srv->Info.ServerMode & SVF_SOFTCORE) == SVF_SOFTCORE);
                if(((chrtc.HatId == Config::HatID) && srv_softcore) ||
                   ((chrtc.HatId == Config::HatIDSoftcore) && !srv_softcore))
                {
                    Printf(LOG_Hacking, "[CL] %s (%s) - Hacking: character \"%s\" rejected by hat from server ID %u (reason: invalid HatID).\n", conn->HisAddr.c_str(), conn->Login.c_str(), chrtc.Nick.c_str(), srv->Number);
                    CLCMD_Kick(conn, P_FHTAGN);
                    return false;
                }

                if((srv->Info.ServerMode & SVF_NOOBSRV) == SVF_NOOBSRV)
                {
                    if(chrtc.Spells & ~0x09010422)
                    {
                        Printf(LOG_Error, "[CL] %s (%s) - Character \"%s\" rejected by hat from server ID %u (reason: EQuest check - strong spells %08X).\n", conn->HisAddr.c_str(), conn->Login.c_str(), chrtc.Nick.c_str(), srv->Number, chrtc.Spells);
                        CLCMD_Kick(conn, P_TOO_STRONG);
                        return false;
                    }

                    uint32_t exp_total = 0;
                    exp_total += chrtc.ExpFireBlade;
                    exp_total += chrtc.ExpWaterAxe;
                    exp_total += chrtc.ExpAirBludgeon;
                    exp_total += chrtc.ExpEarthPike;
                    exp_total += chrtc.ExpAstralShooting;

                    if(exp_total > 7320)
                    {
                        Printf(LOG_Error, "[CL] %s (%s) - Character \"%s\" rejected by hat from server ID %u (reason: EQuest check - strong experience %u).\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), srv->Number, exp_total);
                        CLCMD_Kick(conn, P_TOO_STRONG);
                        return false;
                    }

                    float points_total = 132.0;
                    for(int i = 0; i < chrtc.Body; i++)
                        points_total -= CL_NeedPointsFrom(i);
                    for(int i = 0; i < chrtc.Reaction; i++)
                        points_total -= CL_NeedPointsFrom(i);
                    for(int i = 0; i < chrtc.Mind; i++)
                        points_total -= CL_NeedPointsFrom(i);
                    for(int i = 0; i < chrtc.Spirit; i++)
                        points_total -= CL_NeedPointsFrom(i);

                    if(points_total < 0)
                    {
                        Printf(LOG_Error, "[CL] %s (%s) - Character \"%s\" rejected by hat from server ID %u (reason: EQuest check - strong stats, %.1f points left).\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), srv->Number, points_total);
                        CLCMD_Kick(conn, P_TOO_STRONG);
                        return false;
                    }

                    bool items_ok = true;
                    for(uint32_t i = 0; i < chrtc.Bag.Items.size(); i++)
                    {
                        if(!CL_CheckEasyQuestItem(chrtc.Bag.Items[i]))
                        {
                            items_ok = false;
                            break;
                        }
                    }

                    if(items_ok)
                    {
                        for(uint32_t i = 0; i < chrtc.Dress.Items.size(); i++)
                        {
                            if(!CL_CheckEasyQuestItem(chrtc.Dress.Items[i]))
                            {
                                items_ok = false;
                                break;
                            }
                        }
                    }

                    if(!items_ok)
                    {
                        Printf(LOG_Error, "[CL] %s (%s) - Character \"%s\" rejected by hat from server ID %u (reason: EQuest check - strong items).\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_nickname.c_str(), srv->Number);
                        CLCMD_Kick(conn, P_TOO_STRONG);
                        return false;
                    }
                }
            }

            SV_TryClient(srv->Connection, p_id1, p_id2, conn->Login, p_nickname, p_sex);

            conn->SessionID1 = p_id1;
            conn->SessionID2 = p_id2;
            conn->SessionTime = time(NULL);
            conn->SessionServer = srv;
            conn->SessionNickname = p_nickname;
            conn->Flags |= CLIENT_COMPLETE;
            return true;
        }
    }

    Printf(LOG_Error, "[CL] %s (%s) - Server not found: %s.\n", conn->HisAddr.c_str(), conn->Login.c_str(), p_srvname.c_str());
    CLCMD_Kick(conn, P_SERVER_INVALID);
    return false;
}

bool CL_CheckNickname(Client* conn, Packet& pack)
{
    if(conn->IsBot)
    {
        CLCMD_SendNicknameResult(conn, P_WRONG_VERSION);
        return false;
    }

    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0x4E) return false;

    uint32_t unk1;
    pack >> unk1;

    std::string nickname;
    pack >> nickname;

    unsigned long result = CheckNickname(nickname, (conn->GameMode == GAMEMODE_Softcore));
    if(result) Printf(LOG_Error, "[CL] %s (%s) - Nickname \"%s\" rejected.\n", conn->HisAddr.c_str(), conn->Login.c_str(), nickname.c_str());

    return CLCMD_SendNicknameResult(conn, result);
}

bool CLCMD_SendNicknameResult(Client* conn, unsigned long result)
{
    Packet pack;
    pack << (uint8_t)0xDF;
    pack << (uint32_t)result;

    return (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
}

bool CL_DeleteCharacter(Client* conn, Packet& pack)
{
    if(conn->IsBot)
    {
        CLCMD_Kick(conn, P_WRONG_VERSION);
        return false;
    }

    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xCC) return false;

    uint32_t id1 = 0, id2 = 0;
    pack >> id1 >> id2;

    std::string nickname;
    char* data = NULL;
    unsigned long size;
    if(!Login_GetCharacter(conn->Login, id1, id2, size, data, nickname) || !data)
    {
        if(data) delete[] data;
        Printf(LOG_Error, "[DB] Error: Login_GetCharacter(\"%s\", %u, %u, <size>, <data>, <nickname>).\n", conn->Login.c_str(), id1, id2);
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }
    delete[] data;

    if(!Login_DelCharacter(conn->Login, id1, id2))
    {
        Printf(LOG_Error, "[DB] Error: Login_DelCharacter(\"%s\", %u, %u).\n", conn->Login.c_str(), id1, id2);
        CLCMD_Kick(conn, P_UPDATE_ERROR);
        return false;
    }

    Printf(LOG_Info, "[CL] %s (%s) - Character \"%s\" deleted.\n", conn->HisAddr.c_str(), conn->Login.c_str(), nickname.c_str());

    return true;
}

bool CLCMD_EnterSuccess(Client* conn, unsigned long id1, unsigned long id2)
{
    Packet pack;
    pack << (uint8_t)0xD0;
    pack << (uint32_t)id1 << (uint32_t)id2;

    return (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
}

bool CLCMD_SendReconnect(Client* conn, unsigned long id1, unsigned long id2, std::string addr)
{
    Packet pack;
    pack << (uint8_t)0xDA;
    pack << (uint32_t)addr.length() + 5 * 4;
    pack << (uint32_t)Config::HatID;
    pack << (uint32_t)id1 << (uint32_t)id2;
    pack << (uint32_t)id1 << (uint32_t)id2;
    pack.AppendData((uint8_t*)addr.c_str(), addr.length());

    return (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
}
