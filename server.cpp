#include "server.hpp"
#include "login.hpp"
#include "utils.hpp"
#include "session.hpp"
#include "character.hpp"
#include "socket.hpp"
#include "status.hpp"

std::vector<Server*> Servers;

unsigned long SV_TryClient(ServerConnection* conn, unsigned long id1, unsigned long id2, std::string login, std::string nickname, unsigned char sex)
{
    char* data = NULL;
    unsigned long size = 0;

    std::string nickname1 = nickname;
    std::string nickname2;

    if(!Login_GetCharacter(login, id1, id2, size, data, nickname2) || !data)
    {
        Printf(LOG_Error, "[DB] Error: Login_GetCharacter(\"%s\", %u, %u, <size>, <data>, <nickname>).\n", login.c_str(), id1, id2);
        return 0xBADFACE0;
    }

    size_t n1w = nickname1.find_first_of('|');
    if(n1w != nickname1.npos) nickname1.erase(n1w);
    if(nickname1 != nickname2)
    {
        Printf(LOG_Hacking, "[CS] %s - Hacking: tried to change nickname from \"%s\" to \"%s\"!\n", login.c_str(), nickname2.c_str(), nickname1.c_str());
        return 0xBADFACE2;
    }

    SESSION_AddLogin(id1, id2);

    Packet pack;
    pack << (uint8_t)0xDD;
    pack << (uint32_t)(size + nickname.length() + login.length() + 1 + 4 * 7 + 5 - 5 - 5);
    pack << (uint32_t)id1 << (uint32_t)id2;
    pack << (uint32_t)login.length();
    pack << (uint32_t)nickname.length();
    pack << (uint32_t)size;
    pack << (uint32_t)sex;
    pack.AppendData((uint8_t*)data, size);
    pack.AppendData((uint8_t*)login.c_str(), login.length());
    pack.AppendData((uint8_t*)nickname.c_str(), nickname.length());

    delete[] data;

    if(SOCK_SendPacket(conn->Socket, pack, conn->Version) != 0)
    {
        SESSION_DelLogin(id1, id2);
        return 0xBADFACE1;
    }

    return 0;
}


bool SV_Login(ServerConnection* conn, Packet& pack)
{
    uint8_t cr;
    pack >> cr;
    cr ^= 0xD1;
    if(cr == key_20[0]) conn->Version = 20;
    else if(cr == key_11[0]) conn->Version = 11;
    else if(cr == key_10[0]) conn->Version = 10;
    else if(cr == key_08[0]) conn->Version = 7;

    if(conn->Version == 0)
    {
        Printf(LOG_Error, "[SV] Server ID %u tried to connect with unknown version (sig %02X).\n", conn->ID, cr^0xD1);
        return false;
    }

    conn->Flags |= SERVER_LOGGED_IN;

    Printf(LOG_Trivial, "[SV] Server ID %u connected.\n", conn->ID);

    return SVCMD_Welcome(conn);
}

bool SVCMD_Welcome(ServerConnection* conn)
{
    Packet pack;
    pack << (uint8_t)0xD5;
    /*
    if((conn->Parent->Info.ServerCaps & SERVER_CAP_SOFTCORE) != SERVER_CAP_SOFTCORE)
        pack << (uint32_t)Config::HatID;
    else pack << (uint32_t)Config::HatIDSoftcore;*/
    /*
    if (conn->Parent->Info.ServerMode & SVF_SOFTCORE)
        pack << (uint32_t)Config::HatIDSoftcore;
    else if (conn->Parent->Info.ServerMode & SVF_SANDBOX)
        pack << (uint32_t)Config::HatIDSandbox;
    else pack << (uint32_t)Config::HatID;*/
    pack << (uint32_t)Config::HatID;
    pack << (uint32_t)1;

    return (SOCK_SendPacket(conn->Socket, pack, conn->Version) == 0);
}

bool SV_UpdateInfo(ServerConnection* conn, Packet& pack)
{
    uint8_t packet_id;

    uint8_t p_size, p_plcount, p_level, p_type;
    pack >> packet_id;

    if(packet_id != 0xD2) return false;
    pack >> p_plcount >> p_level >> p_type >> p_size;

    if(p_plcount == 0xFF &&
       p_level == 0xFF &&
       p_type == 0xFF &&
       p_size == 0xFF)
    {
        conn->Parent->ShuttingDown = true;
        Printf(LOG_Trivial, "[SV] Server ID %u is shutting down.\n", conn->ID);
        return true;
    }

    std::string p_mapname;
    pack >> p_mapname;

    if(p_plcount != conn->Parent->Info.PlayerCount ||
       p_level != conn->Parent->Info.MapLevel ||
       p_type != conn->Parent->Info.GameMode ||
       p_size != conn->Parent->Info.MapWidth ||
       p_mapname != conn->Parent->Info.MapName)
    {
        conn->Parent->Info.ServerCaps &= ~SERVER_CAP_DETAILED_INFO;
        conn->Parent->Info.PlayerCount = p_plcount;
        conn->Parent->Info.MapName = p_mapname;
        conn->Parent->Info.MapWidth = p_size;
        conn->Parent->Info.MapHeight = p_size;
        conn->Parent->Info.MapLevel = p_level;
        conn->Parent->Info.GameMode = p_type;
        conn->Parent->Info.ServerMode = 0;

        if(conn->Parent->Info.GameMode != GAMEMODE_Arena && conn->Parent->Info.GameMode != GAMEMODE_Cooperative)
            Printf(LOG_Warning, "[SV] Warning: server ID %u is serving with unknown gamemode (%u).\n", conn->ID, conn->Parent->Info.GameMode);
    }

    ST_ScheduleGeneration();

    return true;
}

bool SV_ReturnCharacter(ServerConnection* conn, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xCF) return false;

    uint32_t p_size;
    uint32_t p_id1, p_id2, p_loglen, p_chrlen;
    pack >> p_size >> p_id1 >> p_id2 >> p_loglen >> p_chrlen;
    if(!p_chrlen)
    {
        Printf(LOG_Error, "[SV] Received NULL character from server ID %u!\n", conn->ID);
        return true;
    }

    char* p_logname_c = new char[p_loglen + 1];
    p_logname_c[p_loglen] = 0;
    char* p_chrdata = new char[p_chrlen];
    pack.GetData((uint8_t*)p_chrdata, p_chrlen);
    pack.GetData((uint8_t*)p_logname_c, p_loglen);
    std::string p_logname(p_logname_c);
    delete[] p_logname_c;

    CCharacter chr;
    BinaryStream bs;
    std::vector<uint8_t>& bsb = bs.GetBuffer();
    for(uint32_t i = 0; i < p_chrlen; i++)
        bsb.push_back(p_chrdata[i]);

    delete[] p_chrdata;

    if(!chr.LoadFromStream(bs))
    {
        Printf(LOG_Error, "[SV] Received bad character from server ID %u!\n", conn->ID);
        return true;
    }

    if(!Login_Exists(p_logname))
    {
        Printf(LOG_Error, "[SV] Received character \"%s\" for non-existent login \"%s\" from server ID %u!\n", chr.Nick.c_str(), p_logname.c_str(), conn->ID);
        delete[] p_chrdata;
        return true;
    }

    bool sent_s = true;

    if(!Login_CharExists(p_logname, chr.Id1, chr.Id2, true))
    {
        if (conn->Parent->Info.GameMode == GAMEMODE_Softcore)
            chr.HatId = Config::HatIDSoftcore;
        else if (conn->Parent->Info.GameMode == GAMEMODE_Sandbox)
            chr.HatId = Config::HatIDSandbox;
        else chr.HatId = Config::HatID;
        if (conn->Parent->HatId >= 0)
            chr.HatId = conn->Parent->HatId;
    }

    bool should_unlock = true;
    bool should_save = true;
    if(should_save && ((chr.Id2 & 0x3F000000) != 0x3F000000))
    {
        int srvHatId;
        if (conn->Parent->Info.GameMode == GAMEMODE_Softcore)
            srvHatId = Config::HatIDSoftcore;
        else if (conn->Parent->Info.GameMode == GAMEMODE_Sandbox)
            srvHatId = Config::HatIDSandbox;
        else srvHatId = Config::HatID;
        if (conn->Parent->HatId >= 0)
            srvHatId = conn->Parent->HatId;

        bool server_nosaving = ((conn->Parent->Info.ServerMode & SVF_NOSAVING) == SVF_NOSAVING);

        if(server_nosaving)
        {
            should_save = false;
            Printf(LOG_Trivial, "[SV] Not saving login \"%s\" (SVG_NOSAVING is set, flags %08X).\n", p_logname.c_str(), conn->Parent->Info.ServerMode);
        }
        else if(conn->Parent->Info.GameMode == GAMEMODE_Arena)
        {
            should_save = false;
            Printf(LOG_Trivial, "[SV] Not saving login \"%s\" (Arena).\n", p_logname.c_str());
        }
        else if(chr.HatId != srvHatId)
        {
            //should_save = false;
            //Printf(LOG_Trivial, "[SV] Not saving login \"%s\" (chr.HatId==%d != srvHatId==%d; GameMode=%d)\n", chr.HatId, srvHatId, conn->Parent->Info.GameMode);
            Printf(LOG_Error, "[SV] Server ID %u sent login \"%s\" with bad HatID (chr.HatId==%d != srvHatId==%d; GameMode==%d); saving with srvHatId.\n",
                conn->ID, p_logname.c_str(), chr.HatId, srvHatId, conn->Parent->Info.GameMode);
            chr.HatId = srvHatId;
        }
    }

    BinaryStream obs;
    if(!chr.SaveToStream(obs))
    {
        Printf(LOG_Error, "[SV] Internal error: unable to save A2C for character \"%s\", login \"%s\" from server ID %u!\n", chr.Nick.c_str(), p_logname.c_str(), conn->ID);
        return true;
    }

    std::vector<uint8_t>& obsb = obs.GetBuffer();
    p_chrdata = new char[obsb.size()];
    for(uint32_t i = 0; i < obsb.size(); i++)
        p_chrdata[i] = obsb[i];
    p_chrlen = obsb.size();

    bool p__locked_hat, p__locked;
    unsigned long p__id1, p__id2, p__srvid;

    if(!Login_GetLocked(p_logname, p__locked_hat, p__locked, p__id1, p__id2, p__srvid))
    {
        Printf(LOG_Error, "[DB] Error: Login_GetLocked(\"%s\", ..., ..., ..., ...).\n", p_logname.c_str());
        return true;
    }

    if(p__locked_hat)
    {
        Printf(LOG_Error, "[SV] Warning: server tried to return character for hat-locked login \"%s\".\n", p_logname.c_str());
        should_save = false;
        should_unlock = false;
    }
    else if(!p__locked)
    {
        Printf(LOG_Error, "[SV] Warning: server tried to return character for unlocked login \"%s\".\n", p_logname.c_str());
        should_save = false;
        should_unlock = false;
    }
    else if(p__id1 != p_id1 || p__id2 != p_id2)
    {
        Printf(LOG_Error, "[SV] Warning: server tried to return different character (%u:%u as opposed to locked %u:%u).\n", p_id1, p_id2, p__id1, p__id2);
        should_save = false;
        should_unlock = false;
    }
    else if(p__srvid != conn->ID)
    {
        Printf(LOG_Error, "[SV] Warning: server tried to return character while not owning it!\n");
        should_save = false;
        should_unlock = false;
    }

    if(should_save && !Login_SetCharacter(p_logname, p_id1, p_id2, p_chrlen, p_chrdata, chr.Nick))
        Printf(LOG_Error, "[DB] Error: Login_SetCharacter(\"%s\", %u, %u, %u, %08X, \"%s\").\n", p_logname.c_str(), p_id1, p_id2, p_chrlen, p_chrdata, chr.Nick.c_str());
    else
    {
        Printf(LOG_Info, "[SV] Received character \"%s\" for login \"%s\" from server ID %u.\n", chr.Nick.c_str(), p_logname.c_str(), conn->ID);
        sent_s = SVCMD_ReceivedCharacter(conn, p_logname);
        should_unlock = true;
    }

    if(should_unlock) Login_SetLocked(p_logname, false, false, 0, 0, 0); // character left the server, so unlock it
    //conn->Parent->Layer->
    if(conn->Parent->Info.ServerCaps & SERVER_CAP_DETAILED_INFO)
    {
        for(size_t i = 0; i < conn->Parent->Info.Locked.size(); i++)
        {
            if(conn->Parent->Info.Locked[i] == p_logname)
            {
                conn->Parent->Info.Locked.erase(conn->Parent->Info.Locked.begin()+i);
                i--;
            }
        }
    }


    delete[] p_chrdata;
    return sent_s;
}

bool SVCMD_ReceivedCharacter(ServerConnection* conn, std::string login)
{
    Packet pck;
    pck << (uint8_t)0xD3;
    pck << (uint32_t)0;
    pck << login;

    return (SOCK_SendPacket(conn->Socket, pck, conn->Version) == 0);
}

bool SV_ConfirmClient(ServerConnection* conn, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xD8) return false;
    uint32_t p_id1 = 0, p_id2 = 0;
    pack >> p_id1 >> p_id2;
    SESSION_SetLogin(p_id1, p_id2, 0);

    return true;
}

bool SV_RejectClient(ServerConnection* conn, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0xD9) return false;
    uint32_t p_size = 0, p_id1 = 0, p_id2 = 0, p_reason = 0;
    pack >> p_size >> p_id1 >> p_id2 >> p_reason;
    SESSION_SetLogin(p_id1, p_id2, p_reason);

    return true;
}

bool SV_AddConnection(SOCKET socket, sockaddr_in addr)
{
    std::string saddr = inet_ntoa(addr.sin_addr);
    uint16_t sport = ntohs(addr.sin_port);

    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if((srv->IAddress == saddr) && (srv->IPort == sport))
        {
            ServerConnection* conn = new ServerConnection();
            if(!conn) return false;

            conn->Parent = srv;
            conn->Parent->ShuttingDown = false;
            conn->Parent->Info.PlayerCount = 0;
            conn->Active = true;
            conn->Version = 0;
            conn->Socket = socket;
            conn->Flags = SERVER_CONNECTED;
            conn->Receiver.Connect(socket);
            conn->ID = srv->Number;
            srv->Connection = conn;
            return true;
        }
        else if((srv->IAddress == saddr) && ((srv->IPort+1000) == sport))
        {
            ServerLayer* layer = new ServerLayer();
            if(!layer) return false;

            layer->Socket = socket;
            layer->Flags.Connected = true;
            layer->Receiver.Connect(socket);
            srv->Layer = layer;

            Printf(LOG_Trivial, "[SV] Server layer (for ID %u) connected.\n", srv->Number);
            return true;
        }
    }

    return false;
}

bool SV_Process(Server* srv)
{
    ServerConnection* conn = srv->Connection;
    if(!conn) return false;
    if(!(conn->Flags & SERVER_CONNECTED)) return false;
    if(!conn->Receiver.Receive(conn->Version)) return false;

    Packet pack;
    while(conn->Receiver.GetPacket(pack))
    {
        if(!(conn->Flags & SERVER_LOGGED_IN))
        {
            if(!SV_Login(conn, pack)) return false;
            return true;
        }

        uint8_t packet_id;
        pack >> packet_id;
        pack.ResetPosition();

        switch(packet_id)
        {
            default:
                Printf(LOG_Error, "[SV] Received unknown packet %02X (from server ID %u).\n", packet_id, srv->Number);
                return false;
            case 0x64:
                break;
            case 0xD2: // info update
                if(!SV_UpdateInfo(conn, pack)) return false;
                break;
            case 0x03: // initialized notification
                //Printf("[SV] Server ID %u started.\n", conn->ID);
                conn->Active = true;
                srv->ShuttingDown = false;
                break;
            case 0xE0: // ? char update? dunno what's it, needs investigation
                break;
            case 0xCF: // character return
                if(!SV_ReturnCharacter(conn, pack)) return false;
                break;
            case 0xD8: // client accept
                if(!SV_ConfirmClient(conn, pack)) return false;
                break;
            case 0xD9: // client reject
                if(!SV_RejectClient(conn, pack)) return false;
                break;
        }
    }

    return true;
}

bool SV_ProcessLayer(Server* srv)
{
    ServerLayer* layer = srv->Layer;
    if(!layer) return false;
    if(!layer->Flags.Connected) return false;
    if(!layer->Receiver.Receive(20)) return false; // layer connection version is ALWAYS 2.0 version

    Packet pack;
    while(layer->Receiver.GetPacket(pack))
    {
        uint8_t packet_id;
        pack >> packet_id;
        pack.ResetPosition();

        switch(packet_id)
        {
            case 0x10: // initialized notification
                if(!SL_Initialized(srv, pack)) return false;
                break;
            case 0x11: // shutdown notification
                if(!SL_Shutdown(srv, pack)) return false;
                break;
            case 0x12: // map info update, server info update
                if(!SL_UpdateInfo(srv, pack)) return false;
                break;
            case 0x13:
                if(!SL_Broadcast(srv, pack)) return false;
                break;
            default: break;
        }
    }

    return true;
}

void SV_Disconnect(Server* srv)
{
    if(srv->Connection)
    {
        if(srv->ShuttingDown)
            Printf(LOG_Trivial, "[SV] Server ID %u disconnected.\n", srv->Number);
        else Printf(LOG_Error, "[SV] Server ID %u unexpectedly closed connection.\n", srv->Number);
        SOCK_Destroy(srv->Connection->Socket);
        delete srv->Connection;
        srv->Connection = NULL;
    }
}

void SV_DisconnectLayer(Server* srv)
{
    if(srv->Layer)
    {
        if(srv->ShuttingDown)
            Printf(LOG_Trivial, "[SV] Server ID %u closed control connection.\n", srv->Number);
        else Printf(LOG_Error, "[SV] Server ID %u unexpectedly closed control connection.\n", srv->Number);
        SOCK_Destroy(srv->Layer->Socket);
        delete srv->Layer;
        srv->Layer = NULL;
    }
}

void Net_ProcessServers()
{
    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if(srv)
        {
            if(srv->Connection)
            {
                if(!(srv->Connection->Flags & SERVER_CONNECTED) || !SV_Process(srv))
                    SV_Disconnect(srv);
            }
            if(srv->Layer)
            {
                if(!srv->Layer->Flags.Connected || !SV_ProcessLayer(srv))
                    SV_DisconnectLayer(srv);
            }
        }
    }
}

bool SL_Initialized(Server* srv, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0x10) return false;
    uint32_t caps;
    pack >> caps;
    srv->Info.ServerCaps = caps;

    return true;
}

bool SL_Shutdown(Server* srv, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;
    if(packet_id != 0x11) return false;
    srv->ShuttingDown = true;

    return true;
}

bool SL_UpdateInfo(Server* srv, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0x12) return false;
    uint32_t p_gamemode;
    std::string p_mapname;
    uint8_t p_maplevel;
    uint32_t p_mapwidth;
    uint32_t p_mapheight;
    uint32_t p_maptime;
    uint32_t p_servermode;

    pack >> p_gamemode >> p_mapname >> p_maplevel >> p_mapwidth >> p_mapheight >> p_maptime >> p_servermode;

    if(p_gamemode > 3 || p_maplevel > 4)
    {
        srv->ShuttingDown = true;
        return true;
    }

    uint32_t p_playercount;
    pack >> p_playercount;

    srv->Info.ServerCaps |= SERVER_CAP_DETAILED_INFO;
    srv->Info.PlayerCount = p_playercount;
    srv->Info.MapName = p_mapname;
    srv->Info.MapWidth = p_mapwidth;
    srv->Info.MapHeight = p_mapheight;
    srv->Info.MapLevel = p_maplevel;
    srv->Info.Time = p_maptime;
    srv->Info.GameMode = p_gamemode;

    // magic here
    if (p_gamemode != GAMEMODE_Arena)
    {
        if (p_servermode & SVF_SOFTCORE)
            srv->Info.GameMode = GAMEMODE_Softcore;
        else if (p_servermode & SVF_SANDBOX)
            srv->Info.GameMode = GAMEMODE_Sandbox;
    }
    // magic ends here

    srv->Info.ServerMode = p_servermode;

    std::vector<ServerPlayer> players_old = srv->Info.Players;
    srv->Info.Players.clear();

    for(size_t i = 0; i < p_playercount; i++)
    {
        uint32_t pp_id1;
        uint32_t pp_id2;
        std::string pp_nickname;
        std::string pp_login;
        bool pp_connected;
        std::string pp_ip;
        pack >> pp_nickname >> pp_login >> pp_id1 >> pp_id2;
        pack >> pp_connected;
        pack >> pp_ip;
        ServerPlayer player;
        player.Id1 = pp_id1;
        player.Id2 = pp_id2;
        player.Nickname = pp_nickname;
        player.Login = pp_login;
        pp_ip = Trim(pp_ip);
        if(pp_ip.length())
        {
            player.IPAddress = pp_ip;
            player.Connected = true;
        }
        else
        {
            player.IPAddress = "";
            for(std::vector<ServerPlayer>::iterator it = players_old.begin(); it != players_old.end(); ++it)
            {
                ServerPlayer& plr = (*it);
                if(plr.Id1 == player.Id1 && plr.Id2 == player.Id2)
                    player.IPAddress = plr.IPAddress;
            }
            player.Connected = false;
        }
        srv->Info.Players.push_back(player);
    }

    uint32_t p_logincount;
    pack >> p_logincount;

    srv->Info.Locked.clear();
    for(size_t i = 0; i < p_logincount; i++)
    {
        std::string p_login;
        pack >> p_login;
        srv->Info.Locked.push_back(p_login);
    }
/*
    if(srv->Number == 22)
    {
        Printf(LOG_Info, "[SV] Server ID %u logins count: %d\n", srv->Number, p_logincount);
        for(size_t i = 0; i < srv->Info.Locked.size(); i++)
            Printf(LOG_Info, "[SV] Server ID %u login: %s\n", srv->Number, srv->Info.Locked[i].c_str());
    }
*/
    ST_ScheduleGeneration();

    return true;
}

bool SL_Broadcast(Server* srv, Packet& pack)
{
    uint8_t packet_id;
    pack >> packet_id;

    if(packet_id != 0x13) return false;
    std::string message;
    pack >> message;

    Printf(LOG_Info, "[SL] %s\n", message.c_str());

    Packet msgP;
    msgP << (uint8_t)0x63;
    msgP << message;

    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if(srv && srv->Layer)
        {
            if(SOCK_SendPacket(srv->Layer->Socket, msgP, 20) != 0)
                srv->Layer->Flags.Connected = false;
        }
    }

    return true;
}

bool SLCMD_Screenshot(Server* srv, std::string login, uint32_t uid, bool done, std::string url)
{
    Packet msgP;
    msgP << (uint8_t)0x64;
    msgP << login;
    msgP << uid;
    msgP << done;
    msgP << url;
    return (SOCK_SendPacket(srv->Layer->Socket, msgP, 20) == 0);
}

bool SLCMD_MutePlayer(Server* srv, std::string login, uint32_t unmutedate)
{
    Packet msgP;
    msgP << (uint8_t)0x65;
    msgP << login;
    msgP << unmutedate;
    return (SOCK_SendPacket(srv->Layer->Socket, msgP, 20) == 0);
}
