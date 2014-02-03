#include "session.hpp"
#include <windows.h>
#include <vector>

struct Session
{
    unsigned long ID1;
    unsigned long ID2;
    unsigned long Value;
};

std::vector<Session> Sessions;
HANDLE SessionsMutex = NULL;

void SESSION_Lock()
{
    if(!SessionsMutex) SessionsMutex = CreateMutex(NULL, TRUE, NULL);
    else WaitForSingleObject(SessionsMutex, INFINITE);
}

void SESSION_Unlock()
{
    ReleaseMutex(SessionsMutex);
}

void SESSION_AddLogin(unsigned long id1, unsigned long id2)
{
    SESSION_DelLogin(id1, id2);

    Session s;
    s.ID1 = id1;
    s.ID2 = id2;
    s.Value = 0xFFFFFFFF;
    Sessions.push_back(s);
}

void SESSION_SetLogin(unsigned long id1, unsigned long id2, unsigned long value)
{
    for(std::vector<Session>::iterator it = Sessions.begin(); it != Sessions.end(); ++it)
    {
        Session& s = (*it);
        if(s.ID1 == id1 && s.ID2 == id2) s.Value = value;
    }
}

unsigned long SESSION_GetLogin(unsigned long id1, unsigned long id2)
{
    unsigned long value = 0xFFFFFFFF;
    for(std::vector<Session>::iterator it = Sessions.begin(); it != Sessions.end(); ++it)
    {
        Session& s = (*it);
        if(s.ID1 == id1 && s.ID2 == id2)
        {
            value = s.Value;
            break;
        }
    }
    return value;
}

void SESSION_DelLogin(unsigned long id1, unsigned long id2)
{
    for(std::vector<Session>::iterator it = Sessions.begin(); it != Sessions.end(); ++it)
    {
        Session& s = (*it);
        if(s.ID1 == id1 && s.ID2 == id2)
        {
            Sessions.erase(it);
            it--;
        }
    }
}
