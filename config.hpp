#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED

#include <string>
#include <vector>

#define GAMEMODE_Cooperative 0
#define GAMEMODE_Arena 3
#define GAMEMODE_Softcore 2

namespace Config
{
    extern std::string ControlDirectory;
    extern unsigned long ControlRescanDelay;
    extern std::string LogFile;
    extern unsigned long LogLevel;
    extern bool UseSHA1;
    extern bool AutoRegister;
    extern unsigned long HatID;
    extern unsigned long HatIDSoftcore;

    extern std::string HatAddress;
    extern unsigned short HatPort;
    extern std::string IHatAddress;
    extern unsigned short IHatPort;
    extern unsigned long ProtocolVersion;
    extern unsigned long AcceptBacklog;
    extern unsigned long SendTimeout;
    extern unsigned long RecvTimeout;

    extern std::string PathPlayernum;
    extern std::string PathStatus;

    extern std::string SqlAddress;
    extern unsigned short SqlPort;
    extern std::string SqlLogin;
    extern std::string SqlPassword;
    extern std::string SqlDatabase;

    extern bool UseFirewall;
    extern std::string AccessLog;
    extern std::string ErrorLog;

    extern std::vector<uint32_t> ExecutableCRC;
    extern std::vector<uint32_t> LibraryCRC;
    extern std::vector<uint32_t> PatchCRC;
    extern std::vector<uint32_t> WorldCRC;
    //extern std::vector<uint32_t> GraphicsCRC;

    extern bool ReportDatabaseErrors;

    extern std::string OnlineLog;
}

bool ReadConfig(std::string filename);

#endif // CONFIG_HPP_INCLUDED
