#ifndef CCHARACTER_HPP_INCLUDED
#define CCHARACTER_HPP_INCLUDED

#include <string>
#include <vector>
#include "BinaryStream.hpp"

struct CEffect
{
    uint8_t Id1, Id2;
    uint8_t Value1, Value2;
};

struct CItem
{
    uint32_t Id;
    bool IsMagic;
    uint32_t Price;
    uint16_t Count;
    std::vector<CEffect> Effects;
};

class CItemList
{
    public:
        bool LoadFromStream(BinaryStream& stream);
        bool SaveToStream(BinaryStream& stream, bool min_format);

        uint8_t UnknownValue0,
                UnknownValue1,
                UnknownValue2;

        std::vector<CItem> Items;
};

class CCharacter
{
    public:
        CCharacter();
        ~CCharacter();

        bool LoadFromStream(BinaryStream& stream);
        bool SaveToStream(BinaryStream& stream);

        bool LoadFromFile(std::string filename);
        bool SaveToFile(std::string filename);
        uint16_t GenerateKey(uint16_t prev = 0);

        uint32_t StreamCRC2();
        uint32_t StreamCRC(BinaryStream& stream);

        void SaveSection(BinaryStream& file, uint32_t magic, uint16_t key, uint32_t crc, BinaryStream& section);

        uint32_t MonstersKills,
                 PlayersKills,
                 Frags,
                 Deaths,
                 Money,
                 Spells,
                 ActiveSpell,
                 ExpFireBlade,
                 ExpWaterAxe,
                 ExpAirBludgeon,
                 ExpEarthPike,
                 ExpAstralShooting,
                 Id1, Id2, HatId;

        uint8_t UnknownValue1,
                UnknownValue2,
                UnknownValue3,
                Picture,
                Body,
                Reaction,
                Mind,
                Spirit,
                Sex,
                MainSkill,
                Flags,
                Color;

        std::string Nick;
        std::string Clan;

        CItemList Bag;
        CItemList Dress;

        void CryptSection(BinaryStream& section, uint16_t key);

        std::string GetFullName();

        BinaryStream Section55555555;
        BinaryStream Section40A40A40;

        bool Retarded;

        std::string ClanTag;
};

#endif // CCHARACTER_HPP_INCLUDED
