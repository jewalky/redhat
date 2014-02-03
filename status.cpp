#include "status.hpp"
#include <fstream>
#include <string>
#include "server.hpp"
#include "utils.hpp"
#include <ctime>
#include "BinaryStream.hpp"

bool st_gen = false;

void ST_ScheduleGeneration()
{
    st_gen = true;
}

void ST_Generate()
{
    if(st_gen)
    {
        ST_GenStatus();
        ST_GenPlayernum();
        st_gen = false;
    }
}

std::string ST_EscapeXML(std::string what)
{
    std::string newstr = "";
    for(std::string::iterator it = what.begin(); it != what.end(); ++it)
    {
        char wh = (*it);
        if(wh == '<') newstr += "&lt;";
        else if(wh == '>') newstr += "&gt;";
        else if(wh == '&') newstr += "&amp;";
        else if((unsigned char)wh < 32) newstr += "&#x00B2";
        else newstr += wh;
    }
    return newstr;
}

void ST_GenStatus()
{
    std::string status = "<?xml version=\"1.0\" encoding=\"cp866\"?>\n";
    status += "<status>\n";

    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if(!srv) continue;
        std::string str_online = "active";
        if(!srv->Connection) str_online = "disconnected";
        else if(!srv->Connection->Active) str_online = "inactive";

        status += " <server>\n";
        status += Format("  <id>%u</id>\n", srv->Number);
        status += Format("  <name>%s</name>\n", ST_EscapeXML(srv->Name).c_str());
        status += Format("  <online>%s</online>\n", str_online.c_str());

        if(!srv->Connection || !srv->Connection->Active)
        {
            status += " </server>\n";
            continue;
        }

        ServerInfo& inf = srv->Info;
        status += Format("  <detailed>%s</detailed>\n", ((inf.ServerCaps & SERVER_CAP_DETAILED_INFO) ? "true" : "false"));

        if(inf.ServerCaps & SERVER_CAP_DETAILED_INFO)
        {
            unsigned long tme = inf.Time;
            unsigned long tm_h = tme / 3600;
            unsigned long tm_m = tme / 60 - tm_h * 60;
            status += Format("  <map_time>%u:%02u</map_time>\n", tm_h, tm_m);
            status += Format("  <servermode>%u</servermode>\n", inf.ServerMode);
        }


        status += Format("  <map_name>%s</map_name>\n", ST_EscapeXML(inf.MapName).c_str());
        status += Format("  <map_size>%ux%u</map_size>\n", inf.MapWidth, inf.MapHeight);
        status += Format("  <map_level>%u</map_level>\n", inf.MapLevel);
        status += Format("  <gamemode>%u</gamemode>\n", inf.GameMode);
        status += Format("  <player_count>%u</player_count>\n", inf.PlayerCount);

        if(inf.ServerCaps & SERVER_CAP_DETAILED_INFO)
        {
            status += "  <players>\n";
            for(std::vector<ServerPlayer>::iterator jt = inf.Players.begin(); jt != inf.Players.end(); ++jt)
            {
                ServerPlayer& player = (*jt);
                status += "   <player>\n";
                status += Format("    <name>%s</name>\n", ST_EscapeXML(player.Nickname).c_str());
                status += Format("    <id1>%u</id1>\n", player.Id1);
                status += Format("    <id2>%u</id2>\n", player.Id2);
                status += Format("    <login>%s</login>\n", ST_EscapeXML(player.Login).c_str());
                status += Format("    <connected>%s</connected>\n", (player.Connected ? "true" : "false"));
                status += Format("    <ip>%s</ip>\n", player.IPAddress.c_str());
                status += "   </player>\n";
            }
            status += "  </players>\n";
        }

        status += " </server>\n";
    }

    status += "</status>";

    std::ofstream f_stat;
    f_stat.open(Config::PathStatus.c_str(), std::ios::out | std::ios::trunc);
    if(!f_stat.is_open()) return;
    f_stat << status << std::endl;
    f_stat.close();
}

void ST_GenPlayernum()
{
    uint32_t playernum = 0;

    for(std::vector<Server*>::iterator it = Servers.begin(); it != Servers.end(); ++it)
    {
        Server* srv = (*it);
        if(!srv) continue;
        if(!srv->Connection) continue;
        else if(!srv->Connection->Active) continue;

        playernum += srv->Info.PlayerCount;
    }

    std::ofstream f_pnum;
    f_pnum.open(Config::PathPlayernum.c_str(), std::ios::out | std::ios::trunc);
    if(!f_pnum.is_open()) return;
    f_pnum << playernum << std::endl;
    f_pnum.close();
}
