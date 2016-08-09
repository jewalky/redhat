#ifndef LOGIN_HPP_INCLUDED
#define LOGIN_HPP_INCLUDED

#include <string>
#include <vector>

#include "sql.hpp"
#include "CCharacter.hpp"

struct CharacterInfo
{
    unsigned long ID1;
    unsigned long ID2;
};

bool Login_Initialize();
void Login_Shutdown();

std::string Login_MakePassword(std::string password);
bool Login_UnlockAll();
bool Login_UnlockOne(std::string login);
bool Login_Create(std::string login, std::string password);
bool Login_Delete(std::string login);
bool Login_GetPassword(std::string login, std::string& password);
bool Login_SetPassword(std::string login, std::string password);
bool Login_SetLocked(std::string login, bool locked_hat, bool locked, unsigned long id1, unsigned long id2, unsigned long srvid);
bool Login_GetLocked(std::string login, bool& locked_hat, bool& locked, unsigned long& id1, unsigned long& id2, unsigned long& srvid);
bool Login_SetBanned(std::string login, bool banned, unsigned long date_ban, unsigned long date_unban, std::string reason);
bool Login_GetBanned(std::string login, bool& banned, unsigned long& date_ban, unsigned long& date_unban, std::string& reason);
bool Login_SetMuted(std::string login, bool muted, unsigned long date_mute, unsigned long date_unmute, std::string reason);
bool Login_GetMuted(std::string login, bool& muted, unsigned long& date_mute, unsigned long& date_unmute, std::string& reason);
bool Login_SetCharacter(std::string login, unsigned long id1, unsigned long id2, unsigned long size, char* data, std::string nickname);
bool Login_GetCharacter(std::string login, unsigned long id1, unsigned long id2, CCharacter& character);
bool Login_GetCharacter(std::string login, unsigned long id1, unsigned long id2, unsigned long& size, char*& data, std::string& nickname, bool genericId = false);
bool Login_DelCharacter(std::string login, unsigned long id1, unsigned long id2);
bool Login_Exists(std::string login);
bool Login_NickExists(std::string nickname, int hatId);
bool Login_CharExists(std::string login, unsigned long id1, unsigned long id2, bool onlyNormal = false);
bool Login_GetCharacterList(std::string login, std::vector<CharacterInfo>& info, int hatId);
bool Login_GetIPF(std::string login, std::string& ipf);
bool Login_SetIPF(std::string login, std::string ipf);
std::string Login_SerializeItems(CItemList& list);
CItemList Login_UnserializeItems(std::string data);
bool Login_LogAuthentication(std::string login, std::string ip, std::string uuid);

#endif // LOGIN_HPP_INCLUDED
