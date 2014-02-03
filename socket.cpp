#include "config.hpp"
#include "socket.hpp"
#include <winsock2.h>

#include <fstream>
#include "utils.hpp"
#include "hat2.hpp"

SOCKET SOCK_Connect(std::string addr, unsigned short port, unsigned short localport)
{
    sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = 0;
    local.sin_port = htons(localport);
    SOCKET out = socket(AF_INET, SOCK_STREAM, 0);
    if(out == INVALID_SOCKET) return SERR_NOTCREATED;
    if(bind(out, (sockaddr*)&local, sizeof(local)) != 0)
    {
        closesocket(out);
        return SERR_NOTCREATED;
    }

    sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = inet_addr(addr.c_str());
    remote.sin_port = htons(port);
    if(connect(out, (sockaddr*)&remote, sizeof(remote)) != 0)
    {
        closesocket(out);
        return SERR_NOTCREATED;
    }

    return out;
}

SOCKET SOCK_Listen(std::string addr, unsigned short port)
{
    sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = inet_addr(addr.c_str());
    local.sin_port = htons(port);
    SOCKET out = socket(AF_INET, SOCK_STREAM, 0);
    if(out == INVALID_SOCKET) return SERR_NOTCREATED;
    if(bind(out, (sockaddr*)&local, sizeof(local)) != 0)
    {
        closesocket(out);
        return SERR_NOTCREATED;
    }
    if(listen(out, Config::AcceptBacklog) != 0)
    {
        closesocket(out);
        return SERR_NOTCREATED;
    }

    return out;
}

SOCKET SOCK_Accept(SOCKET listener, sockaddr_in& addr)
{
    int fromlen = sizeof(addr);
    SOCKET client = accept(listener, (sockaddr*)&addr, &fromlen);
    if(client == INVALID_SOCKET) return SERR_NOTCREATED;
    return client;
}

int SOCK_SendPacket(SOCKET socket, Packet& packet, unsigned long protover)
{
    uint8_t* data = NULL;
    uint32_t size;
    packet.GetAllData(data, size);
    int ret_code = send_msg(socket, protover, data, size, Config::SendTimeout);
    delete[] data;
    if(ret_code == -2) return SERR_TIMEOUT;
    if(ret_code == -1) return SERR_CONNECTION_LOST;
    return 0;
}

int SOCK_ReceivePacket(SOCKET socket, Packet& packet, unsigned long protover)
{
    packet.Reset();
    unsigned long origin = 0xFFFFFFFF;

    while(!(origin & 0xFFFF0000) || origin == 0xFFFFFFFF)
    {
        unsigned long _siz;
        unsigned long _org;
        if(!SOCK_WaitEvent(socket, 0) && origin == 0xFFFFFFFF) return SERR_TIMEOUT;
        else if(origin != 0xFFFFFFFF) return SERR_INCOMPLETE_DATA;
        int headersize = recv(socket, (char*)&_siz, 4, 0); // receive packet length
        if(headersize != 4) return SERR_CONNECTION_LOST;
        if(!SOCK_WaitEvent(socket, 0)) return SERR_CONNECTION_LOST;
        headersize = recv(socket, (char*)&_org, 4, 0); // receive packet origin
        if(headersize != 4) return SERR_CONNECTION_LOST;
        if(_siz > 0xFF || !_siz) return SERR_CONNECTION_LOST;
        unsigned char* data = new unsigned char[_siz];
        memset(data, 0, _siz);
        if(!SOCK_WaitEvent(socket, 0)) return SERR_CONNECTION_LOST;
        int packetsize = recv(socket, (char*)data, _siz, 0);
        if(packetsize != (int)_siz)
        {
            packet.Reset();
            delete[] data;
            return SERR_CONNECTION_LOST;
        }
        origin = _org;
        PACKET_XorByKey(data, _siz, protover);
        packet.AppendData((uint8_t*)data, _siz);
        delete[] data;
    }
    packet.ResetPosition();
    return 0;
}

void SOCK_Destroy(SOCKET socket)
{
    closesocket(socket);
}

namespace IPFilter
{
    void IPAddress::FromString(std::string str)
    {
        unsigned long mask;

        if(str.length() == 40) // uuid
        {
            Address = ToLower(str);
            Addr = 0;
            Masked = 0;
            Mask = 0;
            return;
        }

        std::vector<std::string> addr = Explode(str, "/");
        if(addr.size() == 2) mask = StrToInt(addr[1]);
        else if(addr.size() == 1) mask = 32;
        else
        {
            Address = "";
            Addr = 0;
            Masked = 0;
            return;
        }
        std::vector<std::string> at = Explode(addr[0], ".");
        if(at.size() != 4)
        {
            Address = "";
            Addr = 0;
            Masked = 0;
            return;
        }
        Addr = 0;
        Masked = 0;
        uint8_t oc_1 = StrToInt(at[0]);
        uint8_t oc_2 = StrToInt(at[1]);
        uint8_t oc_3 = StrToInt(at[2]);
        uint8_t oc_4 = StrToInt(at[3]);
        Addr |= oc_1;
        Addr <<= 8;
        Addr |= oc_2;
        Addr <<= 8;
        Addr |= oc_3;
        Addr <<= 8;
        Addr |= oc_4;
        Address = addr[0];
        unsigned long lmask = 0;
        for(unsigned long i = 0; i < mask; i++)
            lmask |= (1 << (31-i));
        Masked = Addr & lmask;
        Mask = lmask;
    }

    void IPFFile::ReadIPF(std::string string, bool filename)
    {
        std::vector<std::string> vs;
        if(filename)
        {
            std::ifstream f_temp;
            f_temp.open(string.c_str(), std::ios::in);
            if(!f_temp.is_open()) return;
            std::string str;
            while(f_temp >> str) vs.push_back(Trim(str));
            f_temp.close();
        }
        else vs = Explode(string, "\n");
        std::vector<IPFEntry> ret;

        for(size_t i = 0; i < vs.size(); i++)
        {
            std::string str = vs[i];
            size_t cid = str.find(";");
            if(cid != std::string::npos)
                str.erase(cid);
            cid = str.find("//");
            if(cid != std::string::npos)
                str.erase(cid);
            cid = str.find("#");
            if(cid != std::string::npos)
                str.erase(cid);

            str = Trim(str);

            if(str.length() < 8) continue;
            char action = str[0];
            str.erase(0, 1);
            IPFEntry ent;
            if(action == '+') ent.Action = 1;
            else if(action == '-') ent.Action = -1;
            else if(action == '@') ent.Action = 3;
            else if(action == '%') ent.Action = 2;
            else if(action == '~') ent.Action = -10;
            else if(action == ':') ent.Action = -100;
            else continue;

            if(ent.Action > 1)
            {
                size_t in_adm = str.find('!');
                if(in_adm == std::string::npos)
                    continue;
                std::string adm = str;
                str.erase(0, in_adm + 1);
                adm.erase(in_adm);

                ent.Access = adm;
            }

            ent.Address.FromString(str);
            ret.push_back(ent);
        }
        Entries = ret;
    }

    int IPFFile::CheckAddress(std::string addr, std::string& access)
    {
        IPAddress caddr;
        caddr.FromString(addr);
        for(std::vector<IPFEntry>::iterator it = Entries.begin(); it != Entries.end(); ++it)
        {
            IPFEntry& ent = (*it);
            if(ent.Address.Masked == (caddr.Addr & ent.Address.Mask))
            {
                if(ent.Action > 1) access.assign(Trim(ent.Access));
                return ent.Action;
            }
        }
        return 0;
    }

    int IPFFile::CheckUUID(std::string uuid)
    {
        uuid = Trim(ToLower(uuid));
        for(std::vector<IPFEntry>::iterator it = Entries.begin(); it != Entries.end(); ++it)
        {
            IPFEntry& ent = (*it);
            if(ent.Address.Masked == 0 &&
               ent.Address.Mask == 0 &&
               ent.Address.Addr == 0 &&
               ent.Address.Address.length() == 40)
            {
                if(uuid == ent.Address.Address)
                    return ent.Action;
            }
        }
        return 0;
    }
}

void SOCK_SetBlocking(SOCKET socket, bool blocking)
{
    u_long iMode = 1;
    ioctlsocket(socket, FIONBIO, &iMode);
}

bool SOCK_WaitEvent(SOCKET socket, unsigned long timeout)
{
    fd_set fd;
    timeval tv;
    FD_ZERO(&fd);
    FD_SET(socket, &fd);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    return (select(0, &fd, NULL, NULL, &tv));
}
