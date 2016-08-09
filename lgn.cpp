#include <cstring>
#include "lgn.hpp"

bool Login::LoadFromFile(std::string filename)
{
    std::ifstream f_temp;
    f_temp.open(filename.c_str(), std::ios::in | std::ios::binary);
    if(!f_temp.is_open()) return false;
    f_temp.seekg(0);
    f_temp.read((char*)&this->Signature, 4);
    if(this->Signature != 0x12ED34FB) return false;
    this->Password = "";
    f_temp >> this->Password;
    for(int i = 0; i < this->Password.length(); i++)
        if(this->Password[i] == 0)
            this->Password.erase(i);
    f_temp.seekg(0x104);
    unsigned long cnt = 0;
    f_temp.read((char*)&cnt, 4);
    for(unsigned long i = 0; i < cnt; i++)
    {
        RawCharacter rchr;
        rchr.IsCreated = false;
        f_temp.read((char*)&rchr.Id1, 4);
        f_temp.read((char*)&rchr.Id2, 4);
        this->RawCharacters.push_back(rchr);
    }
    f_temp.seekg(0x188);
    unsigned long sizes[16];
    memset(sizes, 0, sizeof(sizes));
    for(unsigned long i = 0; i < cnt; i++)
    {
        f_temp.read((char*)&this->RawCharacters[i].Size, 4);
    }
    f_temp.seekg(0x234);
    for(unsigned long i = 0; i < cnt; i++)
    {
        unsigned long siz = this->RawCharacters[i].Size;
        if(!siz) continue;
        if(siz > 0x0FFFFF)
        {
            cnt--;
            this->RawCharacters.erase(this->RawCharacters.begin()+i);
            i--;
            continue;
        }
        char* sz = new char[siz];
        char* sz2 = new char[siz];
        f_temp.read(sz, siz);
        memcpy(sz2, sz, siz);
        this->RawCharacters[i].Data = sz2;
        Character chr;
        chr.LoadFromBuffer(sz, siz);
        delete sz;
        this->Characters.push_back(chr);
    }
    f_temp.seekg(0x1E0);
    f_temp.read((char*)&this->ServerID, 4);
    f_temp.read((char*)&this->LockedID1, 4);
    f_temp.read((char*)&this->LockedID2, 4);
    f_temp.read((char*)&this->LockedSID1, 4);
    f_temp.read((char*)&this->LockedSID2, 4);
    f_temp.seekg(0x1C8);
    unsigned long lock = 0, ban = 0;
    f_temp.read((char*)&lock, 4);
    f_temp.read((char*)&ban, 4);
    this->Locked = (lock);
    this->Banned = (ban);
    f_temp.close();
    return true;
}

void Login::SaveToFile(std::string filename)
{
    for(std::vector<RawCharacter>::iterator i = this->RawCharacters.begin(); i != this->RawCharacters.end(); ++i)
    {
        RawCharacter& chr = (*i);
        if(chr.IsCreated)
        {
            delete[] chr.Data;
            chr.Size = 0;
            this->RawCharacters.erase(i);
            i--;
        }
    }
    std::ofstream f_temp;
    f_temp.open(filename.c_str(), std::ios::out | std::ios::binary);
    if(!f_temp.is_open()) return;
    unsigned long sig = 0x12ED34FB;
    char* u = new char[0x234];
    memset(u, 0, 0x234);
    f_temp.write(u, 0x234);
    delete u;
    f_temp.seekp(0);
    f_temp.write((char*)&sig, 4);
    f_temp.write(this->Password.c_str(), this->Password.length()+1); // with last \0
    f_temp.seekp(0x104);
    unsigned long cnt = this->RawCharacters.size();
    f_temp.write((char*)&cnt, 4);
    for(std::vector<RawCharacter>::iterator i = this->RawCharacters.begin(); i != this->RawCharacters.end(); ++i)
    {
        RawCharacter& chr = (*i);
        f_temp.write((char*)&chr.Id1, 4);
        f_temp.write((char*)&chr.Id2, 4);
    }
    f_temp.seekp(0x188);
    for(std::vector<RawCharacter>::iterator i = this->RawCharacters.begin(); i != this->RawCharacters.end(); ++i)
    {
        RawCharacter& chr = (*i);
        f_temp.write((char*)&chr.Size, 4);
    }
    f_temp.seekp(0x234);
    for(std::vector<RawCharacter>::iterator i = this->RawCharacters.begin(); i != this->RawCharacters.end(); ++i)
    {
        RawCharacter& chr = (*i);
        f_temp.write(chr.Data, chr.Size);
    }
    f_temp.seekp(0x1E0);
    f_temp.write((char*)&this->ServerID, 4);
    f_temp.write((char*)&this->LockedID1, 4);
    f_temp.write((char*)&this->LockedID2, 4);
    f_temp.write((char*)&this->LockedSID1, 4);
    f_temp.write((char*)&this->LockedSID2, 4);
    f_temp.seekp(0x1C8);
    unsigned long lock = this->Locked, ban = this->Banned;
    f_temp.write((char*)&lock, 4);
    f_temp.write((char*)&ban, 4);
    f_temp.close();
}

Login::~Login()
{
    for(std::vector<RawCharacter>::iterator i = this->RawCharacters.begin(); i != this->RawCharacters.end(); ++i)
    {
        delete (*i).Data;
    }
}

bool Login_Create(std::string login, std::string password);
bool Login_SetCharacter(std::string login, unsigned long id1, unsigned long id2, unsigned long size, char* data, std::string nickname);
bool Login_SetIPF(std::string login, std::string ipf);
bool Login_SetLocked(std::string login, bool locked_hat, bool locked, unsigned long id1, unsigned long id2, unsigned long srvid);
#include "utils.hpp"

void LGN_DBConvert(std::string directory)
{
    std::string LoginL = "abcdefghijklmnopqrstuvwxyz0123456789_";
    for(size_t i = 0; i < LoginL.length(); i++)
    {
        std::string dir = directory + "\\" + LoginL[i];
        Directory cdir;
        if(cdir.Open(dir))
        {
            DirectoryEntry ent;
            while(cdir.Read(ent))
            {
                unsigned long chars = 0;
                if(ent.name[0] != LoginL[i]) continue;
                std::string ext = ent.name;
                size_t wh = ext.find_last_of('.');
                if(wh == std::string::npos) continue;
                std::string login = ent.name;
                ext.erase(0, wh+1);
                login.erase(wh);
                ext = ToLower(ext);
                if(ext != "lgn") continue;
                std::string ipf_path = dir + "\\" + login + ".ipf";
                std::ifstream f_ipf;
                f_ipf.open(ipf_path.c_str(), std::ios::in);
                std::string ipf = "";
                if(f_ipf.is_open())
                {
                    std::string ln;
                    while(getline(f_ipf, ln))
                    {
                        ln = Trim(ln);
                        ipf += ln + "\n";
                    }
                    f_ipf.close();
                }

                std::string path = dir + "\\" + ent.name;
                Login lgn;
                if(!lgn.LoadFromFile(path))
                {
                    Printf(LOG_Warning, "[CB] Warning: couldn't convert login %s.\n", login.c_str());
                    continue;
                }

                if(!Login_Create(login, lgn.Password))
                {
                    Printf(LOG_Warning, "[CB] Warning: couldn't convert login %s.\n", login.c_str());
                    continue;
                }

                if(!Login_SetLocked(login, false, lgn.Locked, lgn.LockedID1, lgn.LockedID2, lgn.ServerID))
                    Printf(LOG_Warning, "[CB] Warning: couldn't lock login %s.\n", login.c_str());

                if(ipf.length())
                {
                    if(!Login_SetIPF(login, ipf))
                        Printf(LOG_Warning, "[CB] Warning: couldn't set up IP filter for login %s.\n", login.c_str());
                    else Printf(LOG_Info, "[CB] Added IP filter for login %s.\n", login.c_str());
                }

                for(unsigned long j = 0; j < lgn.Characters.size(); j++)
                {
                    char* data = NULL;
                    unsigned long size = 0;

                    for(unsigned long k = 0; k < lgn.RawCharacters.size(); k++)
                    {
                        if(lgn.RawCharacters[k].Id1 == lgn.Characters[j].Id1 &&
                           lgn.RawCharacters[k].Id2 == lgn.Characters[j].Id2)
                        {
                            data = lgn.RawCharacters[k].Data;
                            size = lgn.RawCharacters[k].Size;
                        }
                    }

                    if(!size || !data)
                    {
                        Printf(LOG_Warning, "[CB] Warning: couldn't convert character %s (login %s), raw character not found!\n", lgn.Characters[j].Nick.c_str(), login.c_str());
                        continue;
                    }

                    if(!Login_SetCharacter(login, lgn.Characters[j].Id1, lgn.Characters[j].Id2, size, data, lgn.Characters[j].Nick.c_str()))
                    {
                        Printf(LOG_Warning, "[CB] Warning: couldn't convert character %s (login %s), database error!\n", lgn.Characters[j].Nick.c_str(), login.c_str());
                        delete[] data;
                        continue;
                    }

                    chars++;
                }
                Printf(LOG_Info, "[CB] Login %s converted (%u characters).\n", login.c_str(), chars);
            }
            cdir.Close();
        }
    }
}
