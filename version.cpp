#include "version.hpp"
#include <map>

namespace Version
{
    uint32_t Current = 0;
    std::map<uint32_t, uint32_t> Keys;
}

uint32_t V_AddSession(uint32_t key)
{
    Version::Keys[Version::Current] = key;
    uint32_t k = Version::Current;
    Version::Current++;
    return k;
}

void V_ChangeSession(uint32_t session, uint32_t key)
{
    Version::Keys[session] = key;
}

uint32_t V_GetSession(uint32_t session)
{
    uint32_t k = 0;
    for(std::map<uint32_t, uint32_t>::iterator it = Version::Keys.begin(); it != Version::Keys.end(); ++it)
    {
        if(it->first == session)
        {
            k = it->second;
            break;
        }
    }
    return k;
}

void V_DelSession(uint32_t session)
{
    for(std::map<uint32_t, uint32_t>::iterator it = Version::Keys.begin(); it != Version::Keys.end(); ++it)
    {
        if(it->first == session)
        {
            Version::Keys.erase(it);
            break;
        }
    }
}
