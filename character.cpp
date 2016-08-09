#include <cstdio>
#include <cstring>
#include <fstream>
#include "character.hpp"

void Character::LoadFromBuffer(void* buf, unsigned long len)
{
    unsigned long sig = *(unsigned long*)(buf);
    if(sig != 0x04507989)
    {
        this->Error("INVALID_CHARACTER");
        return;
    }

    unsigned long p = 4;
    while(p+16 < len)
    {
        unsigned long ssig = *(unsigned long*)(p+buf); p+=4;
        unsigned long ssiz = *(unsigned long*)(p+buf); p+=4;
        unsigned long skey = *(unsigned long*)(p+buf) >> 0x10; p+=4; // особенности использования
        unsigned long scrc = *(unsigned long*)(p+buf); p+=4;

        switch(ssig)
        {
            case 0xAAAAAAAA: // main
            {
                this->DecryptSection(p+buf, ssiz, skey);
                this->Id1 = *(unsigned long*)(p+buf); p+=4;
                this->Id2 = *(unsigned long*)(p+buf); p+=4;
                this->HatId = *(unsigned long*)(p+buf); p+=4;
                char nickname[33]; // всегда ровно 32 символа
                memcpy(nickname, p+buf, 32); p+=32;
                nickname[32] = 0;
                std::string tmpn = nickname;
                unsigned long cpos = tmpn.find_first_of('|');
                if(cpos != std::string::npos)
                {
                    this->Nick = tmpn;
                    this->Nick.erase(cpos);
                    this->Clan = tmpn;
                    this->Clan.erase(0, cpos+1);
                }
                else
                {
                    this->Nick = tmpn;
                    this->Clan = "";
                }
                this->Sex = *(unsigned char*)(p+buf); p++;
                this->Picture = *(unsigned char*)(p+buf); p++;
                this->MainSkill = *(unsigned char*)(p+buf); p++;
                this->Flags = *(unsigned char*)(p+buf); p++;
                this->Color = *(unsigned char*)(p+buf); p++;

                p += 3; // align?
                break;
            }
            case 0x41392521: // info
            {
                if(skey & 0x0001) p++;
                this->MonstersKills = *(unsigned long*)(p+buf) ^ 0x01529251; p+=4;
                if(skey & 0x0002) p++;
                this->PlayersKills = *(unsigned long*)(p+buf) + this->MonstersKills * 5 + 0x13141516; p+=4;
                if(skey & 0x0004) p++;
                this->Frags = *(unsigned long*)(p+buf) + this->PlayersKills * 7 + 0x00ABCDEF; p+=4;
                if(skey & 0x0008) p++;
                this->Deaths = *(unsigned long*)(p+buf) ^ 0x17FF12AA; p+=4;
                if(skey & 0x0010) p++;
                this->Money = *(unsigned long*)(p+buf) + this->MonstersKills * 3 - 0x21524542; p+=4;
                if(skey & 0x0020) p++;
                this->Body = (*(unsigned char*)(p+buf) + this->Money * 0x11 + this->MonstersKills * 0x13) & 0xFF; p++;
                if(skey & 0x0040) p++;
                this->Reaction = (*(unsigned char*)(p+buf) + this->Body * 3) & 0xFF; p++;
                if(skey & 0x0080) p++;
                this->Mind = (*(unsigned char*)(p+buf) + this->Body + this->Reaction * 5) & 0xFF; p++;
                if(skey & 0x0100) p++;
                this->Spirit = (*(unsigned char*)(p+buf) + this->Body * 7 + this->Mind * 9) & 0xFF; p++;
                if(skey & 0x4000) p++;
                this->Spells = *(unsigned long*)(p+buf) ^ 0x10121974; p+=4;
                if(skey & 0x2000) p++;
                this->ActiveSpell = *(unsigned long*)(p+buf); p+=4; // !
                if(skey & 0x0200) p++;
                this->ExpFireBlade = *(unsigned long*)(p+buf) ^ 0xDADEDADE; p+=4;
                if(skey & 0x0400) p++;
                this->ExpWaterAxe = *(unsigned long*)(p+buf) - this->ExpFireBlade * 0x771; p+=4;
                if(skey & 0x0800) p++;
                this->ExpAirBludgeon = *(unsigned long*)(p+buf) - this->ExpWaterAxe * 0x771; p+=4;
                if(skey & 0x1000) p++;
                this->ExpEarthPike = *(unsigned long*)(p+buf) - this->ExpAirBludgeon * 0x771; p+=4;
                if(skey & 0x2000) p++;
                this->ExpAstralShooting = *(unsigned long*)(p+buf) - this->ExpEarthPike * 0x771; p+=4;

                // end?
                // end.
                break;
            }
            case 0x55555555: // unknown; client info?
            {
                p += ssiz;
                break;
            }
            case 0x40A40A40: // unknown; client info?
            {
                p += ssiz;
                break;
            }
            case 0x3A5A3A5A: // bag
            {
                p += ssiz;
                break;
            }
            case 0xDE0DE0DE: // dress
            {
                p += ssiz;
                break;
            }
            case NULL:
            {
                return;
            }
            default: // wtf?
            {
                this->Error("STRANGE_SECTION");
                return;
            }
        }
    }
}

void Character::LoadFromFile(std::string filename)
{
    unsigned long filelen;
    std::fstream f_temp(filename.c_str(), std::ios::binary);

    if(!f_temp.is_open())
    {
        this->Error("FILE_NOT_FOUND");
        return;
    }

    f_temp.seekg(0, std::ios::end);
    filelen = f_temp.tellg();
    f_temp.seekg(0, std::ios::beg);
    unsigned char* buf = (unsigned char*)malloc(filelen);
    f_temp.read((char*)buf, filelen);
    f_temp.close();
    this->LoadFromBuffer(buf, filelen);
    free(buf);
}

void Character::Error(std::string err)
{
    this->BadString = err;
    this->Bad = true;
}

void Character::DecryptSection(void* buf, unsigned long len, unsigned long key)
{
    unsigned long k = key | (key << 0x10);
    unsigned char* nbuf = reinterpret_cast<unsigned char*>(buf);
    for(int i = 0; i < len; i++)
    {
        nbuf[i] = ((unsigned char)(k >> 0x10) ^ nbuf[i]) & 0xFF;
        k = (k << 1) & 0xFFFFFFFF;
		if((i & 0x0F) == 0x0F) k |= key;
    }
}
