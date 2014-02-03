#ifndef LOGIN_HPP_INCLUDED
#define LOGIN_HPP_INCLUDED

#include <string>
#include <fstream>
#include <vector>

#include "character.hpp"

struct RawCharacter
{
    unsigned long Id1;
    unsigned long Id2;

    unsigned long Size;
    char* Data;

    bool IsCreated;
    std::string CreatedName;
    unsigned char CreatedBody, CreatedReaction, CreatedMind, CreatedSpirit;
    unsigned char CreatedPicture, CreatedSex;
};

class Login
{
    public:
        ~Login();

        unsigned long Signature;
        std::string Password;
        bool Banned;
        bool Locked;
        bool GM;

        unsigned long HatID;
        unsigned long ServerID;
        unsigned long LockedID1;
        unsigned long LockedID2;

        unsigned long LockedSID1; // ???
        unsigned long LockedSID2; // ???

        std::vector<Character> Characters;
        std::vector<RawCharacter> RawCharacters;
        bool LoadFromFile(std::string name);
        void SaveToFile(std::string name);
};

#endif // LOGIN_HPP_INCLUDED
