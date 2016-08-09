#ifndef UTILS_HPP_INCLUDED
#define UTILS_HPP_INCLUDED

#include <string>
#include <vector>

#include <dirent.h>
#include <cstdlib>

#define LOG_Silent          0
#define LOG_FatalError      1
#define LOG_Error           2
#define LOG_Hacking         3
#define LOG_Warning         4
#define LOG_Info            5
#define LOG_Trivial         6

// string-related
std::string Format(const std::string& format, ...);
std::vector<std::string> Explode(const std::string& what, const std::string& separator);
bool IsWhitespace(char what);
std::string TrimLeft(const std::string& what, bool (callback)(char) = IsWhitespace);
std::string TrimRight(const std::string& what, bool (callback)(char) = IsWhitespace);
std::string Trim(const std::string& what, bool (callback)(char) = IsWhitespace);
std::string ToLower(const std::string& what);
std::string ToUpper(const std::string& what);
unsigned long StrToInt(const std::string& what);
unsigned long HexToInt(const std::string& what);
double StrToFloat(const std::string& what);
bool StrToBool(const std::string& what);
bool CheckInt(const std::string& what);
bool CheckBool(const std::string& what);
bool CheckHex(const std::string& what);
bool CheckFloat(const std::string& what);

// file related
bool FileExists(const std::string& file);
std::string Basename(const std::string& file);
std::string FixSlashes(const std::string& filename);
std::string TruncateSlashes(const std::string& filename);

// directory search
struct DirectoryEntry
{
    std::string name;
};

class Directory
{
public:
    Directory();
    ~Directory();

    bool Open(const std::string& what);
    bool Read(DirectoryEntry& where);
    void Close();

protected:
    DIR* directory;
};

void Printf(unsigned long level, const std::string& format, ...);
void PrintfT(unsigned long level, const std::string& format, ...);

#endif // UTILS_HPP_INCLUDED
