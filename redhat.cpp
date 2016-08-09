#include "config.hpp"
#include "socket.hpp"
#include "sql.hpp"
#include "utils.hpp"
#include "listener.hpp"
#include "status.hpp"
#include "login.hpp"

void H_Quit()
{
    Printf(LOG_Info, "[HC] Hat is shutting down.\n");
    Net_Quit();
    SQL_Close();
}

void LGN_DBConvert(std::string directory);

bool H_Init(int argc, char* argv[])
{
    atexit(H_Quit);

    SetConsoleTitle("Red Hat");

    HANDLE wHnd = GetStdHandle(STD_OUTPUT_HANDLE);
    SMALL_RECT windowSize = {0, 0, 119, 49};
    COORD bufferSize = {120, 300};

    // Change the console window size:
    SetConsoleScreenBufferSize(wHnd, bufferSize);
    SetConsoleWindowInfo(wHnd, TRUE, &windowSize);

    if(!ReadConfig("redhat.cfg")) return false;
    if(!SQL_Init()) return false;

    if(!Login_UnlockAll())
        Printf(LOG_Warning, "[HC] Unable to hat-unlock all logins.\n");

    bool exit_ = false;
    for(int i = 0; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "-droptables")
        {
            SQL_DropTables();
            exit_ = true;
        }
        else if(arg == "-createtables")
        {
            SQL_CreateTables();
            exit_ = true;
        }
        else if(arg == "-convertlgn" && i != argc-1)
        {
            std::string arg2 = argv[i+1];
            LGN_DBConvert(arg2);
            exit_ = true;
        }
        else if(arg == "-updatetables")
        {
            SQL_UpdateTables();
            exit_ = true;
        }
    }
    if(exit_) return false;

    Printf(LOG_Info, "[HC] Red Hat (v1.3) started.\n");

    Net_Init();

    return true;
}

void H_Process()
{
    while(true)
    {
        Net_Listen();
        ST_Generate();
        Sleep(1);
    }
}

int main(int argc, char* argv[])
{
    if(!H_Init(argc, argv)) return 1;
    H_Process();
    return 0;
}
