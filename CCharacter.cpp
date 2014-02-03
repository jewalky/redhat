#include "CCharacter.hpp"
#include <ctime>
#include <cstdlib>
#include "utils.hpp"

CCharacter::CCharacter()
{
    Retarded = false;
    Dress.UnknownValue0 = 0;
    Dress.UnknownValue1 = 0;
    Dress.UnknownValue2 = 0;
    Dress.Items.resize(12);
    for(std::vector<CItem>::iterator it = Dress.Items.begin(); it != Dress.Items.end(); ++it)
    {
        CItem& item = (*it);
        item.Id = 0;
        item.Count = 1;
        item.IsMagic = false;
        item.Price = 0;
        item.Effects.clear();
    }
}

CCharacter::~CCharacter()
{
    Dress.Items.clear();
}

bool CCharacter::LoadFromStream(BinaryStream& fil)
{
    fil.Seek(0);
    if(fil.ReadUInt32() != 0x04507989) return false; // invalid signature

    BinaryStream csec;

    while(!fil.EndOfStream())
    {
        uint32_t ssig = fil.ReadUInt32(),
                 ssiz = fil.ReadUInt32(),
                 skey = fil.ReadUInt32(),
                 scrc = fil.ReadUInt32();
        skey = (skey & 0xFFFF0000) >> 16;

        switch(ssig)
        {
            case 0xAAAAAAAA: // base info
            {
                csec.LoadFromStream(fil, ssiz);
                CryptSection(csec, skey);
                if(StreamCRC(csec) != scrc) return false;
                csec.Seek(0);
                Id1 = csec.ReadUInt32();
                Id2 = csec.ReadUInt32();
                HatId = csec.ReadUInt32();
                std::string rawnick = csec.ReadFixedString(32);
                size_t splw = rawnick.find('|');
                if(splw != std::string::npos)
                {
                    Nick = rawnick;
                    Clan = rawnick;
                    Nick.erase(splw);
                    Clan.erase(0, splw+1);
                }
                else
                {
                    Nick = rawnick;
                    Clan = "";
                }
                Sex = csec.ReadUInt8();
                Picture = csec.ReadUInt8();
                MainSkill = csec.ReadUInt8();
                Flags = csec.ReadUInt8();
                Color = csec.ReadUInt8();
                UnknownValue1 = csec.ReadUInt8();
                UnknownValue2 = csec.ReadUInt8();
                UnknownValue3 = csec.ReadUInt8();
                csec.Reset();
                break;
            }
            case 0x41392521: // stats info
            {
                if((skey & 0x0001) == 0x0001) fil.ReadUInt8();
                MonstersKills = fil.ReadUInt32() ^ 0x01529251;
                if((skey & 0x0002) == 0x0002) fil.ReadUInt8();
                PlayersKills = fil.ReadUInt32() + MonstersKills * 5 + 0x13141516;
                if((skey & 0x0004) == 0x0004) fil.ReadUInt8();
                Frags = fil.ReadUInt32() + PlayersKills * 7 + 0x00ABCDEF;
                if((skey & 0x0008) == 0x0008) fil.ReadUInt8();
                Deaths = fil.ReadUInt32() ^ 0x17FF12AA;
                if((skey & 0x0010) == 0x0010) fil.ReadUInt8();
                Money = fil.ReadUInt32() + MonstersKills * 3 - 0x21524542;
                if((skey & 0x0020) == 0x0020) fil.ReadUInt8();
                Body = (uint8_t)((uint32_t)fil.ReadUInt8() + Money * 0x11 + MonstersKills * 0x13);
                if((skey & 0x0040) == 0x0040) fil.ReadUInt8();
                Reaction = (uint8_t)((uint32_t)fil.ReadUInt8() + Body * 3);
                if((skey & 0x0080) == 0x0080) fil.ReadUInt8();
                Mind = (uint8_t)((uint32_t)fil.ReadUInt8() + Body + Reaction * 5);
                if((skey & 0x0100) == 0x0100) fil.ReadUInt8();
                Spirit = (uint8_t)((uint32_t)fil.ReadUInt8() + Body * 7 + Mind * 9);
                if((skey & 0x4000) == 0x4000) fil.ReadUInt8();
                Spells = fil.ReadUInt32() - 0x10121974;
                if((skey & 0x2000) == 0x2000) fil.ReadUInt8();
                ActiveSpell = fil.ReadUInt32();
                if((skey & 0x0200) == 0x0200) fil.ReadUInt8();
                ExpFireBlade = fil.ReadUInt32() ^ 0xDADEDADE;
                if((skey & 0x0400) == 0x0400) fil.ReadUInt8();
                ExpWaterAxe = fil.ReadUInt32() - ExpFireBlade * 0x771;
                if((skey & 0x0800) == 0x0800) fil.ReadUInt8();
                ExpAirBludgeon = fil.ReadUInt32() - ExpWaterAxe * 0x771;
                if((skey & 0x1000) == 0x1000) fil.ReadUInt8();
                ExpEarthPike = fil.ReadUInt32() - ExpAirBludgeon * 0x771;
                if((skey & 0x2000) == 0x2000) fil.ReadUInt8();
                ExpAstralShooting = fil.ReadUInt32() - ExpEarthPike * 0x771;
                if(StreamCRC2() != scrc) return false;
                break;
            }
            case 0x3A5A3A5A: // bag
                csec.LoadFromStream(fil, ssiz);
                CryptSection(csec, skey);
                if(StreamCRC(csec) != scrc) return false;
                if(!Bag.LoadFromStream(csec)) return false;
                csec.Reset();
                break;
            case 0xDE0DE0DE: // dress
                csec.LoadFromStream(fil, ssiz);
                CryptSection(csec, skey);
                if(StreamCRC(csec) != scrc) return false;
                if(!Dress.LoadFromStream(csec)) return false;
                csec.Reset();
                break;
            case 0x40A40A40: // unknown 1
                Section40A40A40.LoadFromStream(fil, ssiz);
                CryptSection(Section40A40A40, skey);
                if(StreamCRC(Section40A40A40) != scrc) return false;
                break;
            case 0x55555555: // unknown 2
                Section55555555.LoadFromStream(fil, ssiz);
                CryptSection(Section55555555, skey);
                if(StreamCRC(Section55555555) != scrc) return false;
                break;
            default:
                csec.LoadFromStream(fil, ssiz);
                CryptSection(csec, skey);
                if(StreamCRC(csec) != scrc) return false;
                csec.Reset();
                break;
        }
    }

    return true;
}

bool CCharacter::SaveToStream(BinaryStream& fil)
{
    fil.Reset();
    fil.WriteUInt32(0x04507989);

    BinaryStream csec;
    // first, write 0xAAAAAAAA section (base info)
    csec.WriteUInt32(Id1);
    csec.WriteUInt32(Id2);
    csec.WriteUInt32(HatId);
    csec.WriteFixedString(GetFullName(), 32);
    csec.WriteUInt8(Sex);
    csec.WriteUInt8(Picture);
    csec.WriteUInt8(MainSkill);
    csec.WriteUInt8(Flags);
    csec.WriteUInt8(Color);
    csec.WriteUInt8(UnknownValue1);
    csec.WriteUInt8(UnknownValue2);
    csec.WriteUInt8(UnknownValue3);
    uint16_t key = GenerateKey();
    uint32_t crc = StreamCRC(csec);
    CryptSection(csec, key);
    SaveSection(fil, 0xAAAAAAAA, key, crc, csec);
    csec.Reset();

    // write 0x55555555 section (extended monster kills)
    csec.LoadFromStream(Section55555555, Section55555555.GetLength());
    key = GenerateKey(key);
    crc = StreamCRC(csec);
    CryptSection(csec, key);
    SaveSection(fil, 0x55555555, key, crc, csec);
    csec.Reset();

    // write 0x40A40A40 section (unknown 1)
    // ONLY IF PRESENT
    // create zero section
    //csec.LoadFromStream(Section40A40A40, Section40A40A40.GetLength());
    for(uint32_t i = 0; i < 9; i++)
    {
        csec.WriteUInt16(0);
        csec.WriteUInt16(0);
        csec.WriteUInt32(0);
    }
    key = GenerateKey(key);
    crc = StreamCRC(csec);
    CryptSection(csec, key);
    SaveSection(fil, 0x40A40A40, key, crc, csec);
    csec.Reset();

    // write 0xDE0DE0DE section (dress)
    if(!Dress.SaveToStream(csec, true)) return false;
    key = GenerateKey(key);
    crc = StreamCRC(csec);
    CryptSection(csec, key);
    SaveSection(fil, 0xDE0DE0DE, key, crc, csec);
    csec.Reset();

    // write 0x41392521 section (stats info)
    key = GenerateKey(key);
    if(key & 0x0001) csec.WriteUInt8(0);
    csec.WriteUInt32(MonstersKills ^ 0x01529251);
    if(key & 0x0002) csec.WriteUInt8(0);
    csec.WriteUInt32(PlayersKills - MonstersKills * 5 - 0x13141516);
    if(key & 0x0004) csec.WriteUInt8(0);
    csec.WriteUInt32(Frags - PlayersKills * 7 - 0x00ABCDEF);
    if(key & 0x0008) csec.WriteUInt8(0);
    csec.WriteUInt32(Deaths ^ 0x17FF12AA);
    if(key & 0x0010) csec.WriteUInt8(0);
    csec.WriteUInt32(Money - MonstersKills * 3 + 0x21524542);
    if(key & 0x0020) csec.WriteUInt8(0);
    csec.WriteUInt8(Body - Money * 0x11 - MonstersKills * 0x13);
    if(key & 0x0040) csec.WriteUInt8(0);
    csec.WriteUInt8(Reaction - Body * 3);
    if(key & 0x0080) csec.WriteUInt8(0);
    csec.WriteUInt8(Mind - Body - Reaction * 5);
    if(key & 0x0100) csec.WriteUInt8(0);
    csec.WriteUInt8(Spirit - Body * 7 - Mind * 9);
    if(key & 0x4000) csec.WriteUInt8(0);
    csec.WriteUInt32(Spells + 0x10121974);
    if(key & 0x2000) csec.WriteUInt8(0);
    csec.WriteUInt32(ActiveSpell);
    if(key & 0x0200) csec.WriteUInt8(0);
    csec.WriteUInt32(ExpFireBlade ^ 0xDADEDADE);
    if(key & 0x0400) csec.WriteUInt8(0);
    csec.WriteUInt32(ExpWaterAxe + ExpFireBlade * 0x771);
    if(key & 0x0800) csec.WriteUInt8(0);
    csec.WriteUInt32(ExpAirBludgeon + ExpWaterAxe * 0x771);
    if(key & 0x1000) csec.WriteUInt8(0);
    csec.WriteUInt32(ExpEarthPike + ExpAirBludgeon * 0x771);
    if(key & 0x2000) csec.WriteUInt8(0);
    csec.WriteUInt32(ExpAstralShooting + ExpEarthPike * 0x771);
    SaveSection(fil, 0x41392521, key, StreamCRC2(), csec);
    csec.Reset();

    // write 0x3A5A3A5A section (bag)
    if(!Bag.SaveToStream(csec, false)) return false;
    key = GenerateKey(key);
    crc = StreamCRC(csec);
    CryptSection(csec, key);
    SaveSection(fil, 0x3A5A3A5A, key, crc, csec);
    csec.Reset();

    return true;
}

bool CCharacter::LoadFromFile(std::string filename)
{
    BinaryStream stream;
    if(!stream.LoadFromFile(filename)) return false;
    return LoadFromStream(stream);
}

bool CCharacter::SaveToFile(std::string filename)
{
    BinaryStream stream;
    if(!SaveToStream(stream)) return false;
    return stream.SaveToFile(filename);
}

uint16_t CCharacter::GenerateKey(uint16_t prev)
{
    if(!prev) srand(time(NULL));
    else srand(prev);

    return rand();
}

uint32_t CCharacter::StreamCRC2()
{
    BinaryStream cs;
    cs.WriteUInt32(MonstersKills);
    cs.WriteUInt32(PlayersKills);
    cs.WriteUInt32(Frags);
    cs.WriteUInt32(Deaths);
    cs.WriteUInt32(Money);
    cs.WriteUInt8(Body);
    cs.WriteUInt8(Reaction);
    cs.WriteUInt8(Mind);
    cs.WriteUInt8(Spirit);
    cs.WriteUInt32(Spells);
    cs.WriteUInt32(ActiveSpell);
    cs.WriteUInt32(ExpFireBlade);
    cs.WriteUInt32(ExpWaterAxe);
    cs.WriteUInt32(ExpAirBludgeon);
    cs.WriteUInt32(ExpEarthPike);
    cs.WriteUInt32(ExpAstralShooting);

    return StreamCRC(cs);
}

uint32_t CCharacter::StreamCRC(BinaryStream& stream)
{
    std::vector<uint8_t>& buf = stream.GetBuffer();

    uint32_t scrc = 0;
    for(size_t i = 0; i < buf.size(); i++)
        scrc = (scrc << 1) + buf[i];
    return scrc;
}

void CCharacter::SaveSection(BinaryStream& file, uint32_t magic, uint16_t key, uint32_t crc, BinaryStream& section)
{
    uint32_t ssig = magic;
    uint32_t ssiz = section.GetLength();
    uint32_t skey = (uint32_t)(key << 16) & 0xFFFF0000;
    uint32_t scrc = crc;

    file.WriteUInt32(ssig);
    file.WriteUInt32(ssiz);
    file.WriteUInt32(skey);
    file.WriteUInt32(scrc);
    section.SaveToStream(file);
}

void CCharacter::CryptSection(BinaryStream& section, uint16_t key)
{
    uint32_t k = key | ((uint32_t)key << 0x10);
    std::vector<uint8_t>& data = section.GetBuffer();
    for(size_t i = 0; i < data.size(); i++)
    {
        data[i] = ((uint8_t)(k >> 0x10) ^ data[i]) & 0xFF;
        k = (k << 1) & 0xFFFFFFFF;
        if((i & 0x0F) == 0x0F) k |= key;
    }
}

bool CItemList::LoadFromStream(BinaryStream& stream)
{
    Items.clear();
    stream.Seek(0);
    UnknownValue0 = stream.ReadUInt8();
    UnknownValue1 = stream.ReadUInt8();
    UnknownValue2 = stream.ReadUInt8();

    stream.Seek(9);
    while(!stream.EndOfStream())
    {
        CItem item;
        item.Id = stream.ReadUInt16();
        uint8_t b000 = stream.ReadUInt8();
        if((b000 & 0x80) == 0x80)
        {
            item.Count = b000 - 0x80;
            item.IsMagic = false;
            item.Price = 0;
            item.Effects.clear();
            Items.push_back(item);
        }
        else if((b000 & 0x20) == 0x20)
        {
            uint8_t effsz = b000 & 0x0F;
            item.Price = stream.ReadUInt32();
            item.IsMagic = true;
            item.Count = 1;
            item.Effects.clear();
            for(uint8_t i = 0; i < effsz; i++)
            {
                CEffect effect;
                effect.Id1 = stream.ReadUInt8();
                effect.Value1 = stream.ReadUInt8();
                effect.Id2 = 0;
                effect.Value2 = 0;
                if(effect.Id1 == 0x29 || (effect.Id1 >= 0x2C && effect.Id1 <= 0x30))
                {
                    effect.Id2 = stream.ReadUInt8();
                    effect.Value2 = stream.ReadUInt8();
                    effsz -= 1;
                }
                item.Effects.push_back(effect);
            }
            Items.push_back(item);
        }
        else if(!b000)
        {
            item.Count = (uint32_t)stream.ReadUInt16();
            item.IsMagic = false;
            item.Price = 0;
            item.Effects.clear();
            Items.push_back(item);
        }
        else return false;
    }

    return true;
}

bool CItemList::SaveToStream(BinaryStream& stream, bool min_format)
{
    stream.Seek(0);
    for(std::vector<CItem>::iterator it = Items.begin(); it != Items.end(); ++it)
    {
        CItem& item = (*it);
        stream.WriteUInt16(item.Id);

        if(item.IsMagic)
        {
            uint8_t ec = 0;
            for(std::vector<CEffect>::iterator jt = item.Effects.begin(); jt != item.Effects.end(); ++jt)
            {
                CEffect& effect = (*jt);
                ec++;
                if(effect.Id1 == 0x29 || (effect.Id1 >= 0x2C && effect.Id1 <= 0x30))
                    ec++;
            }

            if(ec > 0x0F) return false;
            stream.WriteUInt8(ec | 0x20);
            stream.WriteUInt32(item.Price);

            for(std::vector<CEffect>::iterator jt = item.Effects.begin(); jt != item.Effects.end(); ++jt)
            {
                CEffect& effect = (*jt);
                stream.WriteUInt8(effect.Id1);
                stream.WriteUInt8(effect.Value1);
                if(effect.Id1 == 0x29 || (effect.Id1 >= 0x2C && effect.Id1 <= 0x30))
                {
                    stream.WriteUInt8(effect.Id2);
                    stream.WriteUInt8(effect.Value2);
                }
            }
        }
        else if(item.Count <= 0x3F)
        {
            stream.WriteUInt8(item.Count | 0x80);
        }
        else
        {
            stream.WriteUInt8(0);
            stream.WriteUInt16(item.Count);
        }
    }

    stream.Seek(0);
    uint32_t siz = stream.GetLength();
    stream.WriteUInt8(0);
    stream.WriteUInt8(0);
    if(!min_format) stream.WriteUInt8(0);
    else stream.WriteUInt8(Items.size() * 3 + 4);
    if(!min_format) stream.WriteUInt16(Items.size());
    else stream.WriteUInt16(0);
    stream.WriteUInt16(0);
    stream.WriteUInt16(siz);

    return true;
}

std::string CCharacter::GetFullName()
{
    if(Clan.length()) return Nick + "|" + Clan;
    else return Nick;
}
