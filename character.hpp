#ifndef CHARACTER_HPP_INCLUDED
#define CHARACTER_HPP_INCLUDED

#include <cstdlib>
#include <string>
#include <vector>
#include <map>

struct sEffect
{
    unsigned long Id, Id2;
    unsigned long Value, Value2;
};

struct sItem
{
    unsigned long Id;
    unsigned long Price;
    bool IsMagic;
    unsigned long Count;
    std::vector<sEffect> Effects;
};

class Character
{
    public:
        unsigned long
            MonstersKills,
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
        unsigned char
            MainSkill,
            Flags,
            Color,
            Picture,
            Body,
            Reaction,
            Mind,
            Spirit,
            Sex;
        std::string
            Nick, Clan;
        std::vector<sItem>
            Bag, Dress;

        bool Bad;
        std::string BadString;

        void DecryptSection(void* buf, unsigned long len, unsigned long key);
        void LoadFromFile(std::string filename);
        void LoadFromBuffer(void* buf, unsigned long len);
        void Error(std::string);
        // сохранение не используется

        char* Section55555555;
        uint32_t Size55555555;
};

#endif // CHARACTER_HPP_INCLUDED
