#include "config.hpp"
#include <fstream>
#include <algorithm>
#include "utils.hpp"
#include "server.hpp"

namespace Config
{
    std::string ControlDirectory = "ctl";
    unsigned long ControlRescanDelay = 2500;
    std::string LogFile = "redhat.log";
    unsigned long LogLevel = LOG_Trivial; // all messages
    bool UseSHA1 = true;
    bool AutoRegister = false;
    unsigned long HatID = 1000;
    unsigned long HatIDSoftcore = 2000;
    unsigned long HatIDSandbox = 3000;

    std::string HatAddress = "0.0.0.0";
    unsigned short HatPort = 8000;
    std::string IHatAddress = "0.0.0.0";
    unsigned short IHatPort = 7999;
    unsigned long ProtocolVersion = 15;
    unsigned long AcceptBacklog = 64;
    unsigned long SendTimeout = 15;
    unsigned long RecvTimeout = 15;
    unsigned long ClientTimeout = 5;
    unsigned long ClientActiveTimeout = 60;

    std::string PathPlayernum = "playernum.txt";
    std::string PathStatus = "playerstat.xml";

    std::string SqlAddress = "localhost";
    unsigned short SqlPort = 3306;
    std::string SqlLogin = "root";
    std::string SqlPassword = "";
    std::string SqlDatabase = "logins";

    std::vector<Server> Servers;

    bool UseFirewall = false;
    std::string AccessLog = "";
    std::string ErrorLog = "";

    std::vector<std::string> Includes;

    std::vector<uint32_t> ExecutableCRC;
    std::vector<uint32_t> LibraryCRC;
    std::vector<uint32_t> PatchCRC;
    std::vector<uint32_t> WorldCRC;
    //std::vector<uint32_t> GraphicsCRC;

    std::string OnlineLog = "redhat.ohd";

    bool ReportDatabaseErrors = false;
}

bool ReadConfig(std::string filename)
{
    using namespace std;
    ifstream f_cfg;
    f_cfg.open(filename.c_str(), ios::in);
    if(!f_cfg.is_open()) return false;

    string line;
    unsigned long lnid = 0;
    std::string section = "root";

    Server* srv = NULL;

    while(getline(f_cfg, line))
    {
        bool enc = false;
        for(size_t i = 0; i < line.length(); i++)
        {
            if(line[i] == '"') enc = !enc;
            if(enc) continue;
            if(line[i] == '#' || line[i] == ';' ||
                (line[i] == '/' && i != line.length()-1 && line[i+1] == '/'))
            {
                line.erase(i);
                break;
            }
        }

        line = Trim(line);
        if(!line.length()) continue;

        if(line[0] == '!')
        {
            if(section == "server")
            {
                if(srv) Servers.push_back(srv);
                srv = new Server();
                srv->Connection = NULL;
                srv->Layer = NULL;
                section = "";
            }

            if(line.find("!include ") == 0)
            {
                line.erase(0, 9);
                line = Trim(line);
                if(line[0] == '"')
                {
                    line.erase(0, 1);
                    line.erase(line.length()-1, 1);
                }

                if(std::find(Config::Includes.begin(), Config::Includes.end(), line) == Config::Includes.end())
                {
                    Config::Includes.push_back(line);
                    ReadConfig(line);
                }
                else
                {
                    Printf(LOG_Error, "[HC] ReadConfig: recursive inclusion detected in \"%s\":%u.\n", filename.c_str(), lnid);
                }
            }
            continue;
        }

        if(line[0] == '[')
        {
            if(section == "server")
            {
                if(srv) Servers.push_back(srv);
                srv = NULL;
            }

            line.erase(0, 1);
            size_t whs = line.find_first_of(']');
            if(whs == string::npos)
            {
                return false;
            }
            line.erase(whs);
            section = ToLower(Trim(line));

            if(section.find("server.") == 0)
            {
                section.erase(0, 7);
                if(CheckInt(section))
                {
                    if(!srv) srv = new Server();
                    srv->Number = StrToInt(section);
                    srv->Address = "";
                    srv->IAddress = "";
                    srv->Port = 0;
                    srv->IPort = 0;
                    srv->Name = "";
                    srv->Connection = NULL;
                    srv->Layer = NULL;
                    srv->HatId = -1;
                    section = "server";
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            std::string parameter, value;
            size_t whd = line.find_first_of('=');
            if(whd != string::npos)
            {
                parameter = line;
                parameter.erase(whd);
                value = line;
                value.erase(0, whd+1);
            }
            else
            {
                value = "";
                parameter = line;
            }

            parameter = ToLower(Trim(parameter));
            value = Trim(value);
            if(value[0] == '"')
            {
                if(value[value.length()-1] != '"')
                {
                    return false;
                }

                value.erase(0, 1);
                value.erase(value.length()-1, 1);
            }

            if(section == "settings.base")
            {
                if(parameter == "controldirectory")
                    Config::ControlDirectory = value;
                else if(parameter == "controlrescandelay")
                {
                    if(CheckInt(value))
                        Config::ControlRescanDelay = StrToInt(value);
                }
                else if(parameter == "logfile")
                    Config::LogFile = value;
/*                else if(parameter == "silent")
                {
                    if(CheckBool(value))
                        Config::Silent = StrToBool(value);
                }*/
                else if(parameter == "loglevel")
                {
                    if(CheckInt(value))
                        Config::LogLevel = StrToInt(value);
                }
                else if(parameter == "usesha1")
                {
                    if(CheckBool(value))
                        Config::UseSHA1 = StrToBool(value);
                }
                else if(parameter == "autoregister")
                {
                    if(CheckBool(value))
                        Config::AutoRegister = StrToBool(value);
                }
                else if(parameter == "hatid")
                {
                    if(CheckInt(value))
                        Config::HatID = StrToInt(value);
                }
                else if(parameter == "hatidsoftcore")
                {
                    if(CheckInt(value))
                        Config::HatIDSoftcore = StrToInt(value);
                }
                else if(parameter == "hatidsandbox")
                {
                    if(CheckInt(value))
                        Config::HatIDSandbox = StrToInt(value);
                }
                else if(parameter == "usefirewall")
                {
                    if(CheckBool(value))
                        Config::UseFirewall = StrToBool(value);
                }
                else if(parameter == "accesslog")
                    Config::AccessLog = value;
                else if(parameter == "errorlog")
                    Config::ErrorLog = value;
                else if(parameter == "onlinelog")
                    Config::OnlineLog = value;
            }
            else if(section == "settings.network")
            {
                if(parameter == "hataddress")
                {
                    vector<string> ipd = Explode(value, ":");
                    Config::HatAddress = ipd[0];
                    if(ipd.size() == 2)
                        Config::HatPort = StrToInt(ipd[1]);
                }
                else if(parameter == "inthataddress")
                {
                    vector<string> ipd = Explode(value, ":");
                    Config::IHatAddress = ipd[0];
                    if(ipd.size() == 2)
                        Config::IHatPort = StrToInt(ipd[1]);
                }
                else if(parameter == "protocolversion")
                {
                    if(CheckInt(value))
                        Config::ProtocolVersion = StrToInt(value);
                }
                else if(parameter == "acceptbacklog")
                {
                    if(CheckInt(value))
                        Config::AcceptBacklog = StrToInt(value);
                }
                else if(parameter == "sendtimeout")
                {
                    if(CheckInt(value))
                        Config::SendTimeout = StrToInt(value);
                }
                else if(parameter == "recvtimeout")
                {
                    if(CheckInt(value))
                        Config::RecvTimeout = StrToInt(value);
                }
                else if(parameter == "clienttimeout")
                {
                    if(CheckInt(value))
                        Config::ClientTimeout = StrToInt(value);
                }
                else if(parameter == "clientactivetimeout")
                {
                    if(CheckInt(value))
                        Config::ClientActiveTimeout = StrToInt(value);
                }
            }
            else if(section == "settings.status")
            {
                if(parameter == "pathplayernum")
                    Config::PathPlayernum = value;
                else if(parameter == "pathstatus")
                    Config::PathStatus = value;
            }
            else if(section == "settings.sql")
            {
                if(parameter == "server")
                {
                    vector<string> ipd = Explode(value, ":");
                    Config::SqlAddress = ipd[0];
                    if(ipd.size() == 2)
                        Config::SqlPort = StrToInt(ipd[1]);
                }
                else if(parameter == "login")
                    Config::SqlLogin = value;
                else if(parameter == "password")
                    Config::SqlPassword = value;
                else if(parameter == "database")
                    Config::SqlDatabase = value;
                else if(parameter == "reportdatabaseerrors")
                {
                    if(CheckBool(value))
                        Config::ReportDatabaseErrors = StrToBool(value);
                }
            }
            else if(section == "settings.version")
            {
                if(parameter == "executablecrc")
                {
                    std::vector<std::string> str = Explode(value, ",");
                    for(std::vector<std::string>::iterator kt = str.begin(); kt != str.end(); ++kt)
                    {
                        std::string st = (*kt);
                        st = Trim(st);
                        if(!st.length()) continue;
                        if(!CheckHex(st)) continue;
                        Config::ExecutableCRC.push_back(HexToInt(st));
                    }
                }
                else if(parameter == "librarycrc")
                {
                    std::vector<std::string> str = Explode(value, ",");
                    for(std::vector<std::string>::iterator kt = str.begin(); kt != str.end(); ++kt)
                    {
                        std::string st = (*kt);
                        st = Trim(st);
                        if(!st.length()) continue;
                        if(!CheckHex(st)) continue;
                        Config::LibraryCRC.push_back(HexToInt(st));
                    }
                }
                else if(parameter == "patchcrc")
                {
                    std::vector<std::string> str = Explode(value, ",");
                    for(std::vector<std::string>::iterator kt = str.begin(); kt != str.end(); ++kt)
                    {
                        std::string st = (*kt);
                        st = Trim(st);
                        if(!st.length()) continue;
                        if(!CheckHex(st)) continue;
                        Config::PatchCRC.push_back(HexToInt(st));
                    }
                }
                else if(parameter == "worldcrc")
                {
                    std::vector<std::string> str = Explode(value, ",");
                    for(std::vector<std::string>::iterator kt = str.begin(); kt != str.end(); ++kt)
                    {
                        std::string st = (*kt);
                        st = Trim(st);
                        if(!st.length()) continue;
                        if(!CheckHex(st)) continue;
                        Config::WorldCRC.push_back(HexToInt(st));
                    }
                }
                /*else if(parameter == "graphicscrc")
                {
                    std::vector<std::string> str = Explode(value, ",");
                    for(std::vector<std::string>::iterator kt = str.begin(); kt != str.end(); ++kt)
                    {
                        std::string st = (*kt);
                        st = Trim(st);
                        if(!st.length()) continue;
                        if(!CheckHex(st)) continue;
                        Config::GraphicsCRC.push_back(HexToInt(st));
                    }
                }*/
            }
            else if(section == "server")
            {
                if(parameter == "name")
                    srv->Name = value;
                else if(parameter == "serveraddr")
                {
                    vector<string> ipd = Explode(value, ":");
                    srv->Address = ipd[0];
                    if(ipd.size() == 2)
                        srv->Port = StrToInt(ipd[1]);
                }
                else if(parameter == "intserveraddr")
                {
                    vector<string> ipd = Explode(value, ":");
                    srv->IAddress = ipd[0];
                    if(ipd.size() == 2)
                        srv->IPort = StrToInt(ipd[1]);
                }
                else if(parameter == "hatid")
                {
                    if(CheckInt(value))
                        srv->HatId = StrToInt(value);
                }
            }
        }
        lnid++;
    }

    if(section == "server")
    {
        if(srv)
        {
            Servers.push_back(srv);
            srv = NULL;
        }
    }

    f_cfg.close();

    return true;
}
