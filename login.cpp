#include "login.hpp"
#include "sql.hpp"
#include "utils.hpp"
#include "config.hpp"

#include "sha1.h"

#include <windows.h>

std::string Login_MakePassword(std::string password)
{
    //Printf("Login_MakePassword()\n");
    if(!Config::UseSHA1) return password;
    static unsigned char hash[20];
    static char hexstring[41];
    hexstring[40] = 0;
    sha1::calc(password.c_str(), password.length(), hash);
    sha1::toHexString(hash, hexstring);
    return std::string(hexstring);
}

bool Login_UnlockOne(std::string login)
{
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        std::string query_unlockone = Format("UPDATE `logins` SET `locked_hat`='0' WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_unlockone.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_UnlockAll()
{
    if(!SQL_CheckConnected()) return false;

    try
    {
        std::string query_unlockall = "UPDATE `logins` SET `locked_hat`='0'";
        if(SQL_Query(query_unlockall.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_Exists(std::string login)
{
    //Printf("Login_Exists()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `name` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        int rows = SQL_NumRows(result);
        SQL_FreeResult(result);

        SQL_Unlock();

        return (rows);
    }
    catch(...)
    {
        SQL_Unlock();
        return true;
    }
}

bool Login_Create(std::string login, std::string password)
{
    //Printf("Login_Create()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(Login_Exists(login)) return false;
        login = SQL_Escape(login);

        std::string pass_new = SQL_Escape(Login_MakePassword(password));

        std::string query_createlgn = Format("INSERT INTO `logins` (`ip_filter`, `name`, `banned`, `banned_date`, `banned_unbandate`, `banned_reason`, `locked`, `locked_id1`, `locked_id2`, `locked_srvid`, `password`) VALUES \
                                            ('', '%s', '0', '0', '0', '', '0', '0', '0', '0', '%s')", login.c_str(), pass_new.c_str());

        if(SQL_Query(query_createlgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        int rows = SQL_AffectedRows();
        SQL_Unlock();

        if(rows != 1) return false; // sql error

        return true; // login created
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

bool Login_Delete(std::string login)
{
    //Printf("Login_Delete()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);

        std::string query_deletelgn = Format("DELETE FROM `logins` WHERE `name`='%s' AND `id`='%u'", login.c_str(), login_id);
        if(SQL_Query(query_deletelgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }

        if(login_id == -1)
        {
            SQL_Unlock();
            return true; // тут спорно, возвращать false или true. с одной стороны, логин таки удалЄн,
                         // с другой - запись в базе €вно неправильна€.
        }

        std::string query_deletecharacters = Format("DELETE FROM `characters` WHERE `login_id`='%d'", login_id);
        if(SQL_Query(query_deletecharacters.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
    return true;
}

bool Login_GetPassword(std::string login, std::string& password)
{
    //Printf("Login_GetPassword()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(!Login_Exists(login)) return false;
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `password` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        password = SQL_FetchString(row, result, "password");
        SQL_FreeResult(result);

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
    return true;
}

bool Login_SetPassword(std::string login, std::string password)
{
    //Printf("Login_SetPassword()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(!Login_Exists(login)) return false;
        login = SQL_Escape(login);

        SQL_Lock();
        std::string pass_new = Login_MakePassword(password);
        std::string query_setpasswd = Format("UPDATE `logins` SET `password`='%s' WHERE `name`='%s'", pass_new.c_str(), login.c_str());
        if(SQL_Query(query_setpasswd.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_SetLocked(std::string login, bool locked_hat, bool locked, unsigned long id1, unsigned long id2, unsigned long srvid)
{
    //Printf("Login_SetLocked()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(!Login_Exists(login)) return false;
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_setlockd = Format("UPDATE `logins` SET `locked_hat`='%u', `locked`='%u', `locked_id1`='%u', `locked_id2`='%u', `locked_srvid`='%u' WHERE `name`='%s'",
                                                (unsigned int)locked_hat, (unsigned int)locked, id1, id2, srvid, login.c_str());
        if(SQL_Query(query_setlockd.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        /*
        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }
        */

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
    return true;
}

bool Login_GetLocked(std::string login, bool& locked_hat, bool& locked, unsigned long& id1, unsigned long& id2, unsigned long& srvid)
{
    //Printf("Login_GetLocked()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `locked_hat`, `locked`, `locked_id1`, `locked_id2`, `locked_srvid` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        locked_hat = (SQL_FetchInt(row, result, "locked_hat"));
        locked = (SQL_FetchInt(row, result, "locked"));
        id1 = SQL_FetchInt(row, result, "locked_id1");
        id2 = SQL_FetchInt(row, result, "locked_id2");
        srvid = SQL_FetchInt(row, result, "locked_srvid");
        SQL_FreeResult(result);
        SQL_Unlock();

        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

bool Login_SetBanned(std::string login, bool banned, unsigned long date_ban, unsigned long date_unban, std::string reason)
{
    //Printf("Login_SetBanned()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(!Login_Exists(login)) return false;
        login = SQL_Escape(login);
        reason = SQL_Escape(reason);

        SQL_Lock();

        std::string query_setlockd = Format("UPDATE `logins` SET `banned`='%u', `banned_date`='%u', `banned_unbandate`='%u', `banned_reason`='%s' WHERE `name`='%s'",
                                                (unsigned int)banned, date_ban, date_unban, reason.c_str(), login.c_str());
        if(SQL_Query(query_setlockd.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
    return true;
}

bool Login_GetBanned(std::string login, bool& banned, unsigned long& date_ban, unsigned long& date_unban, std::string& reason)
{
    //Printf("Login_GetBanned()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id`, `banned`, `banned_date`, `banned_unbandate`, `banned_reason` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        banned = (SQL_FetchInt(row, result, "banned"));
        date_ban = SQL_FetchInt(row, result, "banned_date");
        date_unban = SQL_FetchInt(row, result, "banned_unbandate");
        reason = SQL_FetchString(row, result, "banned_reason");
        SQL_FreeResult(result);
        SQL_Unlock();

        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

bool Login_SetMuted(std::string login, bool muted, unsigned long date_mute, unsigned long date_unmute, std::string reason)
{
    //Printf("Login_SetMuted()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        if(!Login_Exists(login)) return false;
        login = SQL_Escape(login);
        reason = SQL_Escape(reason);

        SQL_Lock();

        std::string query_setlockd = Format("UPDATE `logins` SET `muted`='%u', `muted_date`='%u', `muted_unmutedate`='%u', `muted_reason`='%s' WHERE `name`='%s'",
                                                (unsigned int)muted, date_mute, date_unmute, reason.c_str(), login.c_str());
        if(SQL_Query(query_setlockd.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
    return true;
}

bool Login_GetMuted(std::string login, bool& muted, unsigned long& date_mute, unsigned long& date_unmute, std::string& reason)
{
    //Printf("Login_GetMuted()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id`, `muted`, `muted_date`, `muted_unmutedate`, `muted_reason` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        muted = (SQL_FetchInt(row, result, "muted"));
        date_mute = SQL_FetchInt(row, result, "muted_date");
        date_unmute = SQL_FetchInt(row, result, "muted_unmutedate");
        reason = SQL_FetchString(row, result, "muted_reason");
        SQL_FreeResult(result);
        SQL_Unlock();

        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

#include "CCharacter.hpp"

std::string Login_SerializeItems(CItemList& list)
{
    std::string out;
    out = Format("[%u,%u,%u,%u]", list.UnknownValue0, list.UnknownValue1, list.UnknownValue2, list.Items.size());
    for(std::vector<CItem>::iterator it = list.Items.begin(); it != list.Items.end(); ++it)
    {
        CItem& item = (*it);
        out += Format(";[%u,%u,%u,%u", item.Id, (uint32_t)item.IsMagic, item.Price, item.Count);
        if(item.IsMagic)
        {
            for(std::vector<CEffect>::iterator jt = item.Effects.begin(); jt != item.Effects.end(); ++jt)
            {
                CEffect& effect = (*jt);
                out += Format(",{%u:%u:%u:%u}", effect.Id1, effect.Value1, effect.Id2, effect.Value2);
            }
        }
        out += "]";
    }
    return SQL_Escape(out);
}

CItemList Login_UnserializeItems(std::string list)
{
    CItemList items;
    items.UnknownValue0 = 0;
    items.UnknownValue1 = 0;
    items.UnknownValue2 = 0;

    std::vector<std::string> f_str = Explode(list, ";");
    std::string of_listdata = Trim(f_str[0]);
    if(of_listdata[0] != '[' || of_listdata[of_listdata.length()-1] != ']') return items;
    of_listdata.erase(0, 1);
    of_listdata.erase(of_listdata.length()-1, 1);
    std::vector<std::string> f_listdata = Explode(of_listdata, ",");
    if(f_listdata.size() != 4) return items;

    items.UnknownValue0 = StrToInt(Trim(f_listdata[0]));
    items.UnknownValue1 = StrToInt(Trim(f_listdata[1]));
    items.UnknownValue2 = StrToInt(Trim(f_listdata[2]));
    items.Items.resize(StrToInt(Trim(f_listdata[3])));

    f_str.erase(f_str.begin());

    for(std::vector<CItem>::iterator it = items.Items.begin(); it != items.Items.end(); ++it)
    {
        CItem& item = (*it);
        item.Id = 0;
        item.Count = 1;
        item.IsMagic = false;
        item.Price = 0;
        item.Effects.clear();

        if(!f_str.size()) return items;
        std::string of_itemdata = Trim(f_str[0]);
        if(of_itemdata[0] != '[' || of_itemdata[of_itemdata.length()-1] != ']') return items;
        of_itemdata.erase(0, 1);
        of_itemdata.erase(of_itemdata.length()-1, 1);
        std::vector<std::string> f_itemdata = Explode(of_itemdata, ",");
        if(f_itemdata.size() < 4) return items;
        item.Id = StrToInt(Trim(f_itemdata[0]));
        item.IsMagic = (StrToInt(Trim(f_itemdata[1])));
        item.Price = StrToInt(Trim(f_itemdata[2]));
        item.Count = StrToInt(Trim(f_itemdata[3]));
        if(item.IsMagic)
        {
            for(size_t i = 4; i < f_itemdata.size(); i++)
            {
                std::string of_efdata = f_itemdata[i];
                if(of_efdata[0] != '{' || of_efdata[of_efdata.length()-1] != '}') return items;
                of_efdata.erase(0, 1);
                of_efdata.erase(of_efdata.length()-1, 1);
                std::vector<std::string> f_efdata = Explode(of_efdata, ":");
                if(f_efdata.size() != 4) return items;
                CEffect effect;
                effect.Id1 = StrToInt(Trim(f_efdata[0]));
                effect.Value1 = StrToInt(Trim(f_efdata[1]));
                effect.Id2 = StrToInt(Trim(f_efdata[2]));
                effect.Value2 = StrToInt(Trim(f_efdata[3]));
                item.Effects.push_back(effect);
            }
        }
        f_str.erase(f_str.begin());
    }
    return items;
}

bool Login_SetCharacter(std::string login, unsigned long id1, unsigned long id2, unsigned long size, char* data, std::string nickname)
{
    //Printf("Login_SetCharacter()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);
        nickname = SQL_Escape(nickname);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        std::string query_checkchr = Format("SELECT `login_id` FROM `characters` WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u'", login_id, id1, id2);
        if(SQL_Query(query_checkchr.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        bool create = true;

        result = SQL_StoreResult();
        if(result)
        {
            if(SQL_NumRows(result) == 1) create = false;
            SQL_FreeResult(result);
        }

        if(size == 0x30 && *(unsigned long*)(data) == 0xFFDDAA11)
        {
            uint8_t p_nickname_length = *(uint8_t*)(data + 4);
            uint8_t p_body = *(uint8_t*)(data + 5);
            uint8_t p_reaction = *(uint8_t*)(data + 6);
            uint8_t p_mind = *(uint8_t*)(data + 7);
            uint8_t p_spirit = *(uint8_t*)(data + 8);
            uint8_t p_base = *(uint8_t*)(data + 9);
            uint8_t p_picture = *(uint8_t*)(data + 10);
            uint8_t p_sex = *(uint8_t*)(data + 11);
            uint32_t p_id1 = *(uint32_t*)(data + 12);
            uint32_t p_id2 = *(uint32_t*)(data + 16);
            std::string p_nickname(data+20, p_nickname_length);
            std::string p_nick, p_clan;

            size_t splw = p_nickname.find('|');
            if(splw != std::string::npos)
            {
                p_nick = p_nickname;
                p_clan = p_nickname;
                p_nick.erase(splw);
                p_clan.erase(0, splw+1);
            }
            else
            {
                p_nick = p_nickname;
                p_clan = "";
            }

            std::string chr_query_create1;

            if(create)
            {
                chr_query_create1 = Format("INSERT INTO `characters` (`login_id`, `retarded`, `body`, `reaction`, `mind`, `spirit`, \
                                           `mainskill`, `picture`, `class`, `id1`, `id2`, `nick`, `clan`, `clantag`, `deleted`) VALUES \
                                           ('%u', '1', '%u', '%u', '%u', '%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '', '0')", login_id,
                                                p_body, p_reaction, p_mind, p_spirit, p_base, p_picture, p_sex, p_id1, p_id2,
                                                p_nick.c_str(), p_clan.c_str());
            }
            else
            {
                chr_query_create1 = Format("UPDATE `characters` SET `login_id`='%u' `retarded`='1', `body`='%u', `reaction`='%u', `mind`='%u', `spirit`='%u', \
                                           `mainskill`='%u', `picture`='%u', `class`='%u', `id1`='%u', `id2`='%u', `nick`='%s', `clan`='%s', `clantag`='', `deleted`='0'", login_id,
                                                p_body, p_reaction, p_mind, p_spirit, p_base, p_picture, p_sex, p_id1, p_id2,
                                                p_nick.c_str(), p_clan.c_str());
            }

            if(SQL_Query(chr_query_create1) != 0)
            {
                SQL_Unlock();
                return false;
            }
        }
        else
        {
            BinaryStream strm;
            std::vector<uint8_t>& buf = strm.GetBuffer();
            for(size_t i = 0; i < size; i++)
                buf.push_back(data[i]);

            strm.Seek(0);
            CCharacter chr;
            if(!chr.LoadFromStream(strm))
            {
                SQL_Unlock();
                return false;
            }

            if(chr.Clan.size() > 0)
            {
                int cpos = chr.Clan.find("[");
                if(cpos >= 0)
                {
                    if(cpos > 0 && chr.Clan[cpos-1] == '|')
                        cpos--;
                    chr.Clan = std::string(chr.Clan);
                    chr.Clan.erase(cpos);
                }
            }

            std::string chr_query_update;

            std::vector<uint8_t>& data_40A40A40 = chr.Section40A40A40.GetBuffer();
            std::vector<uint8_t>& data_55555555 = chr.Section55555555.GetBuffer();

            if(create)
            {
                chr_query_update = Format("INSERT INTO `characters` ( \
                                                `login_id`, `id1`, `id2`, `hat_id`, \
                                                `unknown_value_1`, `unknown_value_2`, `unknown_value_3`, \
                                                `nick`, `clan`, \
                                                `picture`, `body`, `reaction`, `mind`, `spirit`, \
                                                `class`, `mainskill`, `flags`, `color`, \
                                                `monsters_kills`, `players_kills`, `frags`, `deaths`, \
                                                `spells`, `active_spell`, `money`, \
                                                `exp_fire_blade`, `exp_water_axe`, \
                                                `exp_air_bludgeon`, `exp_earth_pike`, \
                                                `exp_astral_shooting`, `bag`, `dress`, `clantag`, \
                                                `sec_55555555`, `sec_40A40A40`, `retarded`, `deleted`) VALUES ( \
                                                    '%u', '%u', '%u', '%u', \
                                                    '%u', '%u', '%u', \
                                                    '%s', '%s', \
                                                    '%u', '%u', '%u', '%u', '%u', \
                                                    '%u', '%u', '%u', '%u', \
                                                    '%u', '%u', '%u', '%u', \
                                                    '%u', '%u', '%u', \
                                                    '%u', '%u', '%u', '%u', '%u', \
                                                    '%s', '%s', '%s', '",
                                                        login_id, chr.Id1, chr.Id2, chr.HatId,
                                                        chr.UnknownValue1, chr.UnknownValue2, chr.UnknownValue3,
                                                        SQL_Escape(chr.Nick).c_str(), SQL_Escape(chr.Clan).c_str(),
                                                        chr.Picture, chr.Body, chr.Reaction, chr.Mind, chr.Spirit,
                                                        chr.Sex, chr.MainSkill, chr.Flags, chr.Color,
                                                        chr.MonstersKills, chr.PlayersKills, chr.Frags, chr.Deaths,
                                                        chr.Spells, chr.ActiveSpell, chr.Money,
                                                        chr.ExpFireBlade, chr.ExpWaterAxe,
                                                        chr.ExpAirBludgeon, chr.ExpEarthPike,
                                                        chr.ExpAstralShooting,
                                                        Login_SerializeItems(chr.Bag).c_str(), Login_SerializeItems(chr.Dress).c_str(), SQL_Escape(chr.ClanTag).c_str());

                for(size_t i = 0; i < data_55555555.size(); i++)
                {
                    char ch = (char)data_55555555[i];
                    if(ch == '\\' || ch == '\'') chr_query_update += '\\';
                    chr_query_update += ch;
                }

                chr_query_update += "', '";

                for(size_t i = 0; i < data_40A40A40.size(); i++)
                {
                    char ch = (char)data_40A40A40[i];
                    if(ch == '\\' || ch == '\'') chr_query_update += '\\';
                    chr_query_update += ch;
                }

                chr_query_update += "', '0', '0')";
            }
            else
            {
                chr_query_update = Format("UPDATE `characters` SET \
                                                `id1`='%u', `id2`='%u', `hat_id`='%u', \
                                                `unknown_value_1`='%u', `unknown_value_2`='%u', `unknown_value_3`='%u', \
                                                `nick`='%s', `clan`='%s', \
                                                `picture`='%u', `body`='%u', `reaction`='%u', `mind`='%u', `spirit`='%u', \
                                                `class`='%u', `mainskill`='%u', `flags`='%u', `color`='%u', \
                                                `monsters_kills`='%u', `players_kills`='%u', `frags`='%u', `deaths`='%u', \
                                                `spells`='%u', `active_spell`='%u', `money`='%u', \
                                                `exp_fire_blade`='%u', `exp_water_axe`='%u', \
                                                `exp_air_bludgeon`='%u', `exp_earth_pike`='%u', \
                                                `exp_astral_shooting`='%u', `bag`='%s', `dress`='%s', `deleted`='0'",
                                                    chr.Id1, chr.Id2, chr.HatId,
                                                    chr.UnknownValue1, chr.UnknownValue2, chr.UnknownValue3,
                                                    SQL_Escape(chr.Nick).c_str(), SQL_Escape(chr.Clan).c_str(),
                                                    chr.Picture, chr.Body, chr.Reaction, chr.Mind, chr.Spirit,
                                                    chr.Sex, chr.MainSkill, chr.Flags, chr.Color,
                                                    chr.MonstersKills, chr.PlayersKills, chr.Frags, chr.Deaths,
                                                    chr.Spells, chr.ActiveSpell, chr.Money,
                                                    chr.ExpFireBlade, chr.ExpWaterAxe,
                                                    chr.ExpAirBludgeon, chr.ExpEarthPike,
                                                    chr.ExpAstralShooting,
                                                    Login_SerializeItems(chr.Bag).c_str(), Login_SerializeItems(chr.Dress).c_str());

                chr_query_update += ", `sec_55555555`='";
                for(size_t i = 0; i < data_55555555.size(); i++)
                {
                    char ch = (char)data_55555555[i];
                    if(ch == '\\' || ch == '\'') chr_query_update += '\\';
                    chr_query_update += ch;
                }

                chr_query_update += "', `sec_40A40A40`='";
                for(size_t i = 0; i < data_40A40A40.size(); i++)
                {
                    char ch = (char)data_40A40A40[i];
                    if(ch == '\\' || ch == '\'') chr_query_update += '\\';
                    chr_query_update += ch;
                }

                chr_query_update += Format("', `retarded`='0' WHERE `login_id`='%u' AND `id1`='%u' AND `id2`='%u'", login_id, id1, id2);
            }

            if(SQL_Query(chr_query_update) != 0)
            {
                Printf(LOG_Error, "[SQL] %s\n", SQL_Error().c_str());

                SQL_Unlock();
                return false;
            }
        }

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        Printf(LOG_Error, "[SQL] Caught\n");

        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_GetCharacter(std::string login, unsigned long id1, unsigned long id2, unsigned long& size, char*& data, std::string& nickname, bool genericId)
{
    //Printf("Login_GetCharacter()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        std::string query_character = Format("SELECT * FROM `characters` WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u' AND `deleted`='0'", login_id, id1, id2);
        if(SQL_Query(query_character.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_NumRows(result) != 1)
        {
            SQL_Unlock();
            return false;
        }

        row = SQL_FetchRow(result);

        bool p_retarded = (bool)SQL_FetchInt(row, result, "retarded");
        if(p_retarded) // server creation msg
        {
            size = 0x30;
            data = new char[size];
            std::string p_nick = SQL_FetchString(row, result, "nick");
            std::string p_clan = SQL_FetchString(row, result, "clan");
            std::string p_nickname = p_nick;
            if(p_clan.length()) p_nickname += "|" + p_clan;
            *(uint32_t*)(data) = 0xFFDDAA11;
            *(uint8_t*)(data + 4) = (uint8_t)p_nickname.size();
            *(uint8_t*)(data + 5) = (uint8_t)SQL_FetchInt(row, result, "body");
            *(uint8_t*)(data + 6) = (uint8_t)SQL_FetchInt(row, result, "reaction");
            *(uint8_t*)(data + 7) = (uint8_t)SQL_FetchInt(row, result, "mind");
            *(uint8_t*)(data + 8) = (uint8_t)SQL_FetchInt(row, result, "spirit");
            *(uint8_t*)(data + 9) = (uint8_t)SQL_FetchInt(row, result, "mainskill");
            *(uint8_t*)(data + 10) = (uint8_t)SQL_FetchInt(row, result, "picture");
            *(uint8_t*)(data + 11) = (uint8_t)SQL_FetchInt(row, result, "class");
            *(uint32_t*)(data + 12) = (uint32_t)SQL_FetchInt(row, result, "id1");
            *(uint32_t*)(data + 16) = (uint32_t)SQL_FetchInt(row, result, "id2");
            memcpy(data + 20, p_nickname.data(), p_nickname.size());
            nickname = p_nick;
        }
        else
        {
            CCharacter chr;
            chr.Id1 = SQL_FetchInt(row, result, "id1");
            chr.Id2 = SQL_FetchInt(row, result, "id2");
            chr.HatId = (genericId ? Config::HatID : SQL_FetchInt(row, result, "hat_id"));
            chr.UnknownValue1 = SQL_FetchInt(row, result, "unknown_value_1");
            chr.UnknownValue2 = SQL_FetchInt(row, result, "unknown_value_2");
            chr.UnknownValue3 = SQL_FetchInt(row, result, "unknown_value_3");
            chr.Nick = SQL_FetchString(row, result, "nick");
            chr.Clan = SQL_FetchString(row, result, "clan");
            chr.ClanTag = SQL_FetchString(row, result, "clantag");
            chr.Picture = SQL_FetchInt(row, result, "picture");
            chr.Body = SQL_FetchInt(row, result, "body");
            chr.Reaction = SQL_FetchInt(row, result, "reaction");
            chr.Mind = SQL_FetchInt(row, result, "mind");
            chr.Spirit = SQL_FetchInt(row, result, "spirit");
            chr.Sex = SQL_FetchInt(row, result, "class");
            chr.MainSkill = SQL_FetchInt(row, result, "mainskill");
            chr.Flags = SQL_FetchInt(row, result, "flags");
            chr.Color = SQL_FetchInt(row, result, "color");
            chr.MonstersKills = SQL_FetchInt(row, result, "monsters_kills");
            chr.PlayersKills = SQL_FetchInt(row, result, "players_kills");
            chr.Frags = SQL_FetchInt(row, result, "frags");
            chr.Deaths = SQL_FetchInt(row, result, "deaths");
            chr.Money = SQL_FetchInt(row, result, "money");
            chr.Spells = SQL_FetchInt(row, result, "spells");
            chr.ActiveSpell = SQL_FetchInt(row, result, "active_spell");
            chr.ExpFireBlade = SQL_FetchInt(row, result, "exp_fire_blade");
            chr.ExpWaterAxe = SQL_FetchInt(row, result, "exp_water_axe");
            chr.ExpAirBludgeon = SQL_FetchInt(row, result, "exp_air_bludgeon");
            chr.ExpEarthPike = SQL_FetchInt(row, result, "exp_earth_pike");
            chr.ExpAstralShooting = SQL_FetchInt(row, result, "exp_astral_shooting");

            std::string data_55555555 = SQL_FetchString(row, result, "sec_55555555");
            std::string data_40A40A40 = SQL_FetchString(row, result, "sec_40A40A40");
            chr.Section55555555.Reset();
            chr.Section55555555.WriteFixedString(data_55555555, data_55555555.size());
            chr.Section40A40A40.Reset();
            chr.Section40A40A40.WriteFixedString(data_40A40A40, data_55555555.size());

            chr.Bag = Login_UnserializeItems(SQL_FetchString(row, result, "bag"));
            chr.Dress = Login_UnserializeItems(SQL_FetchString(row, result, "dress"));

            BinaryStream strm;
            if(!chr.SaveToStream(strm))
            {
                SQL_FreeResult(result);
                SQL_Unlock();
                return false;
            }

            std::vector<uint8_t>& buf = strm.GetBuffer();
            size = buf.size();
            data = new char[size];
            for(uint32_t i = 0; i < size; i++)
                data[i] = buf[i];

            nickname = chr.Nick;
        }

        SQL_FreeResult(result);
        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_GetCharacter(std::string login, unsigned long id1, unsigned long id2, CCharacter& character)
{
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        std::string query_character = Format("SELECT * FROM `characters` WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u' AND `deleted`='0'", login_id, id1, id2);
        if(SQL_Query(query_character.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_NumRows(result) != 1)
        {
            SQL_Unlock();
            return false;
        }

        row = SQL_FetchRow(result);

        bool p_retarded = (bool)SQL_FetchInt(row, result, "retarded");
        CCharacter chr;
        chr.Retarded = p_retarded;
        chr.Id1 = SQL_FetchInt(row, result, "id1");
        chr.Id2 = SQL_FetchInt(row, result, "id2");
        chr.HatId = SQL_FetchInt(row, result, "hat_id");
        chr.UnknownValue1 = SQL_FetchInt(row, result, "unknown_value_1");
        chr.UnknownValue2 = SQL_FetchInt(row, result, "unknown_value_2");
        chr.UnknownValue3 = SQL_FetchInt(row, result, "unknown_value_3");
        chr.Nick = SQL_FetchString(row, result, "nick");
        chr.Clan = SQL_FetchString(row, result, "clan");
        chr.ClanTag = SQL_FetchString(row, result, "clantag");
        chr.Picture = SQL_FetchInt(row, result, "picture");
        chr.Body = SQL_FetchInt(row, result, "body");
        chr.Reaction = SQL_FetchInt(row, result, "reaction");
        chr.Mind = SQL_FetchInt(row, result, "mind");
        chr.Spirit = SQL_FetchInt(row, result, "spirit");
        chr.Sex = SQL_FetchInt(row, result, "class");
        chr.MainSkill = SQL_FetchInt(row, result, "mainskill");
        chr.Flags = SQL_FetchInt(row, result, "flags");
        chr.Color = SQL_FetchInt(row, result, "color");
        chr.MonstersKills = SQL_FetchInt(row, result, "monsters_kills");
        chr.PlayersKills = SQL_FetchInt(row, result, "players_kills");
        chr.Frags = SQL_FetchInt(row, result, "frags");
        chr.Deaths = SQL_FetchInt(row, result, "deaths");
        chr.Money = SQL_FetchInt(row, result, "money");
        chr.Spells = SQL_FetchInt(row, result, "spells");
        chr.ActiveSpell = SQL_FetchInt(row, result, "active_spell");
        chr.ExpFireBlade = SQL_FetchInt(row, result, "exp_fire_blade");
        chr.ExpWaterAxe = SQL_FetchInt(row, result, "exp_water_axe");
        chr.ExpAirBludgeon = SQL_FetchInt(row, result, "exp_air_bludgeon");
        chr.ExpEarthPike = SQL_FetchInt(row, result, "exp_earth_pike");
        chr.ExpAstralShooting = SQL_FetchInt(row, result, "exp_astral_shooting");

        std::string data_55555555 = SQL_FetchString(row, result, "sec_55555555");
        std::string data_40A40A40 = SQL_FetchString(row, result, "sec_40A40A40");
        chr.Section55555555.Reset();
        chr.Section55555555.WriteFixedString(data_55555555, data_55555555.size());
        chr.Section40A40A40.Reset();
        chr.Section40A40A40.WriteFixedString(data_40A40A40, data_55555555.size());

        chr.Bag = Login_UnserializeItems(SQL_FetchString(row, result, "bag"));
        chr.Dress = Login_UnserializeItems(SQL_FetchString(row, result, "dress"));

        character = chr;

        SQL_FreeResult(result);
        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }

    return true;
}

bool Login_GetCharacterList(std::string login, std::vector<CharacterInfo>& info, int hatId)
{
    //Printf("Login_GetCharacterList()\n");
    info.clear();

    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        std::string query_character = Format("SELECT `id1`, `id2` FROM `characters` WHERE `login_id`='%d' AND `deleted`='0' AND `retarded`='0'", login_id);
        if (hatId > 0) query_character += Format(" AND (`hat_id`='%d' or (`id2`&0x3F000000)=0x3F000000)", hatId);
        if(SQL_Query(query_character.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        int rows = SQL_NumRows(result);
        for(int i = 0; i < rows; i++)
        {
            row = SQL_FetchRow(result);
            CharacterInfo inf;
            inf.ID1 = SQL_FetchInt(row, result, "id1");
            inf.ID2 = SQL_FetchInt(row, result, "id2");
            info.push_back(inf);
        }

        SQL_FreeResult(result);

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return true;
    }
}

const char enru_charmap_en[] =
    {
        'a',
        'A',
        'c',
        'C',
        'e',
        'E',
        'K',
        'o',
        'О',
        'p',
        'P',
        'u',
        'x',
        'X',
        'y',
        'Y',
        'm',
    };

const char enru_charmap_ru[] =
    {
        '†',
        'А',
        'б',
        'С',
        '•',
        'Е',
        'К',
        'Ѓ',
        'О',
        'а',
        'Р',
        '®',
        'е',
        'Х',
        'г',
        'У',
        'в',
    };

std::string RegexEscape(char what)
{
    std::string retval = "";
    switch(what)
    {
        case '*':
        case '.':
        case '?':
        case '\\':
        case '|':
        case '(':
        case ')':
        case '[':
        case ']':
        case '+':
        case '-':
        case '{':
        case '}':
        case '"':
        case '\'':
        case '^':
        case '$':
            retval += "\\";
        default:
            retval += what;
            break;
    }

    return retval;
}

bool Login_NickExists(std::string nickname, int hatId)
{
    //Printf("Login_NickExists()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        std::string nickname2 = nickname;
        nickname = "";
        for(size_t i = 0; i < nickname2.length(); i++)
        {
            bool char_dest = false;
            for(size_t j = 0; j < sizeof(enru_charmap_en); j++)
            {
                if(nickname2[i] == enru_charmap_en[j] ||
                   nickname2[i] == enru_charmap_ru[j])
                {
                    if(!char_dest) nickname += "[";
                    char_dest = true;
                    nickname += RegexEscape((enru_charmap_en[j] == nickname2[i]) ? enru_charmap_ru[j] : enru_charmap_en[j]);
                }
            }

            nickname += RegexEscape(nickname2[i]);
            if(char_dest)
                nickname += "]";
        }

        nickname = SQL_Escape(nickname);
        //Printf(LOG_Info, "Resulting nickname: %s\n", nickname.c_str());

        SQL_Lock();
        std::string query_nickheck = Format("SELECT `nick` FROM `characters` WHERE `nick` REGEXP '^%s$' AND `deleted`='0'", nickname.c_str());
        if (hatId > 0) query_nickheck += Format(" AND `hat_id`='%d'", hatId);
        if(SQL_Query(query_nickheck.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_FreeResult(result);
            SQL_Unlock();
            return false; // nick does not exist
        }

        SQL_FreeResult(result);
        SQL_Unlock();
        return true; // nick exists
    }
    catch(...)
    {
        SQL_Unlock();
        return true;
    }
}

bool Login_DelCharacter(std::string login, unsigned long id1, unsigned long id2)
{
    //Printf("Login_DelCharacter()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        //std::string query_delchar = Format("DELETE FROM `characters` WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u'", login_id, id1, id2);
        std::string query_delchar = Format("UPDATE `characters` SET `deleted`='1' WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u'", login_id, id1, id2);
        if(SQL_Query(query_delchar.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(SQL_AffectedRows() != 1)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

bool Login_CharExists(std::string login, unsigned long id1, unsigned long id2, bool onlyNormal)
{
    //Printf("Login_CharExists()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_checklgn = Format("SELECT `id` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_checklgn.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        int login_id = SQL_FetchInt(row, result, "id");
        SQL_FreeResult(result);
        if(login_id == -1)
        {
            SQL_Unlock();
            return false;
        }

        std::string query_char = Format("SELECT `login_id` FROM `characters` WHERE `login_id`='%d' AND `id1`='%u' AND `id2`='%u' AND `deleted`='0'", login_id, id1, id2);
        if(onlyNormal) query_char += " AND `retarded`='0'";
        if(SQL_Query(query_char.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false;
        }

        SQL_FreeResult(result);

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return true;
    }
}

bool Login_GetIPF(std::string login, std::string& ipf)
{
    //Printf("Login_GetIPF()\n");
    if(!SQL_CheckConnected()) return false;

    try
    {
        login = SQL_Escape(login);

        SQL_Lock();
        std::string query_getipf = Format("SELECT `ip_filter` FROM `logins` WHERE `name`='%s'", login.c_str());
        if(SQL_Query(query_getipf.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }
        MYSQL_RES* result = SQL_StoreResult();
        if(!result)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_NumRows(result))
        {
            SQL_Unlock();
            SQL_FreeResult(result);
            return false; // login does not exist
        }

        MYSQL_ROW row = SQL_FetchRow(result);
        ipf = SQL_FetchString(row, result, "ip_filter");
        SQL_FreeResult(result);
        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return true;
    }
}

bool Login_SetIPF(std::string login, std::string ipf)
{
    //Printf("Login_SetIPF()\n");
    try
    {
        login = SQL_Escape(login);
        ipf = SQL_Escape(ipf);

        if(!Login_Exists(login)) return false;

        SQL_Lock();
        std::string query_setipf = Format("UPDATE `logins` SET `ip_filter`='%s' WHERE `name`='%s'", ipf.c_str(), login.c_str());
        if(SQL_Query(query_setipf.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        if(!SQL_AffectedRows())
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}

bool Login_LogAuthentication(std::string login, std::string ip, std::string uuid)
{
    try
    {
        login = SQL_Escape(login);
        ip = SQL_Escape(ip);
        uuid = SQL_Escape(uuid);

        SQL_Lock();
        std::string query_logauth = Format("INSERT INTO `authlog` (`login_name`, `ip`, `uuid`, `date`) VALUES ('%s', '%s', '%s', NOW())", login.c_str(), ip.c_str(), uuid.c_str());
        if (SQL_Query(query_logauth.c_str()) != 0)
        {
            SQL_Unlock();
            return false;
        }

        SQL_Unlock();
        return true;
    }
    catch(...)
    {
        SQL_Unlock();
        return false;
    }
}
