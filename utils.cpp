#include "utils.hpp"
#include "config.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>

using namespace std;

/*
    utils::Format: функция, аналогичная sprintf(), но для STL строк.

    Возвращает отформатированную STL строку.

    format: формат
    ...: аргументы для формата
*/
std::string Format(const string& format, ...)
{
    char* line = NULL;
    try
    {
        va_list list;
        va_start(list, format);
        size_t linesize = (vsnprintf(NULL, 0, format.c_str(), list) + 1);
        line = new char[linesize + 1];
        if(!line) return "";
        line[linesize] = 0;
        vsnprintf(line, linesize, format.c_str(), list);
        va_end(list);

        std::string sline(line);
        delete[] line;

        return sline;
    }
    catch(...)
    {
        if(line) delete[] line;
        return "";
    }
}

/*
    utils::Explode: функция для разбиения строки на подстроки.

    Возвращает массив из найденных подстрок.

    what: разбиваемая строка
    separator: разделитель
*/
vector<string> Explode(const string& what, const string& separator)
{
    string curstr;
    vector<string> retval;
    for(size_t i = 0; i < what.length(); i++)
    {
        if(what.find(separator, i) == i)
        {
            retval.push_back(curstr);
            curstr.assign("");
            i += separator.length()-1;
            continue;
        }

        curstr += what[i];
    }
    retval.push_back(curstr);
    return retval;
}

/*
    utils::TrimLeft, utils::TrimRight, utils::Trim: обрезать незначимые символы в начале строки (TrimLeft), в конце (TrimRight)
        или и там, и там (Trim).

    Возвращает обрезанную строку.

    what: обрезаемая строка
    callback: функция, определяющая значимость символа. См. IsWhitespace
*/
string TrimLeft(const string& what, bool (callback)(char))
{
    string ret = what;
    for(string::iterator i = ret.begin(); i != ret.end(); ++i)
    {
        if(callback((*i)))
            continue;
        ret.erase(ret.begin(), i);
        return ret;
    }
    return "";
}

string TrimRight(const string& what, bool (callback)(char))
{
    string ret = what;
    for(string::reverse_iterator i = ret.rbegin(); i != ret.rend(); ++i)
    {
        if(callback((*i)))
            continue;
        ret.erase(i.base(), ret.end());
        return ret;
    }
    return "";
}

string Trim(const string& what, bool (callback)(char))
{
    return TrimRight(TrimLeft(what, callback));
}

/*
    utils::IsWhitespace: вспомогательная функция для Trim*

    Возвращает true, если символ является пробелом и его нужно обрезать.

    what: проверяемый символ.
*/
bool IsWhitespace(char what)
{
    switch((unsigned char)what)
    {
        case ' ':
        case '\r':
        case '\n':
        case '\t':
        case 0xFF:
            return true;
        default:
            return false;
    }
}

/*
    utils::ToLower, utils::ToUpper: преобразование строки в верхний или нижний регистр.

    Возвращает преобразованную строку.

    what: Преобразовываемая строка.
*/
string ToLower(const string& what)
{
    string ret = what;
    for(string::iterator i = ret.begin(); i != ret.end(); ++i)
        (*i) = tolower((*i));
    return ret;
}

string ToUpper(const string& what)
{
    string ret = what;
    for(string::iterator i = ret.begin(); i != ret.end(); ++i)
        (*i) = toupper((*i));
    return ret;
}

#include <fstream>

/*
    utils::FileExists: проверка на существование файла.

    Возвращает true, если файл существует.

    filename: название проверяемого файла.
*/
bool FileExists(const string& filename)
{
    ifstream f_temp;
    f_temp.open(filename.c_str(), ios::in | ios::binary);
    if(!f_temp.is_open()) return false;
    f_temp.close();
    return true;
}

/*
    utils::Basename: срезать путь к файлу, оставив только его название.

    Возвращаемое значение: обрезанная строка.

    filename: старое название файла.
*/
string Basename(const string& filename)
{
    string ret = FixSlashes(filename);
    uint32_t where = ret.find_last_of('/');
    if(where == string::npos) return ret;
    ret.erase(0, where+1);
    return ret;
}

/*
    utils::FixSlashes: сделать путь к файлу POSIX-совместимым (т.е. с человеческими слешами вместо backslash, используемого в DOS/Windows)

    Возвращаемое значение: исправленная строка.

    filename: старое название файла.
*/
string FixSlashes(const string& filename)
{
    string ret = filename;
    for(string::iterator i = ret.begin(); i != ret.end(); ++i)
        if((*i) == '\\') (*i) = '/';
    return ret;
}

/*
    utils::TruncateSlashes: удалить повторяющиеся слеши (напр. main//graphics/mainmenu//menu_.bmp).

    Возвращает исправленную строку.

    filename: старое название файла.
*/
string TruncateSlashes(const string& filename)
{
    string ret = filename;
    char lastchar = 0;
    char thischar = 0;
    for(string::iterator i = ret.begin(); i != ret.end(); ++i)
    {
        thischar = (*i);
        if((thischar == '/' || thischar == '\\') &&
           (lastchar == '/' || lastchar == '\\'))
        {
            ret.erase(i);
            i--;
        }
        lastchar = thischar;
    }
    return ret;
}

/*
    utils::Directory::Open: назначить папку, в которой будет производиться поиск.

    Возвращает true при успешном открытии папки.

    what: название папки.
*/
bool Directory::Open(const string& what)
{
    this->Close();
    this->directory = opendir(what.c_str());
    if(!this->directory) return false;
    return true;
}

/*
    utils::Directory::Read: прочитать следующий файл из папки.

    Возвращаемое значение: true если прочитан, false в противном случае.

    where: структура, куда будут помещены данные о файле.
*/
bool Directory::Read(DirectoryEntry& where)
{
    if(!this->directory) return false;
    struct dirent* dp = readdir(this->directory);
    if(!dp) return false;
    if(dp->d_name[0] == '.') return this->Read(where);
    where.name.assign(dp->d_name);
    return true;
}

/*
    utils::Directory::Close: закрыть папку.
*/
void Directory::Close()
{
    if(this->directory) closedir(this->directory);
    this->directory = NULL;
}

// На всякий, собственно, пожарный
Directory::~Directory()
{
    this->Close();
}

Directory::Directory()
{
    this->directory = NULL;
}

unsigned long StrToInt(const string& what)
{
	unsigned int retval;
	sscanf(what.c_str(), "%u", &retval);
	return retval;
}

unsigned long HexToInt(const string& what)
{
	unsigned int retval;
	sscanf(what.c_str(), "%X", &retval);
	return retval;
}

double StrToFloat(const string& what)
{
	float retval;
	sscanf(what.c_str(), "%f", &retval);
	return (double)retval;
}

bool CheckInt(const string& what)
{
	for(size_t i = 0; i < what.length(); i++)
		if(what[i] < 0x30 || 0x39 < what[i]) return false;
	return true;
}

bool CheckFloat(const string& what)
{
	for(size_t i = 0; i < what.length(); i++)
		if((what[i] < 0x30 || 0x39 < what[i]) && what[i] != '.') return false;
	return true;
}

bool CheckHex(const string& what)
{
	for(size_t i = 0; i < what.length(); i++)
		if((what[i] < 0x30 || 0x39 < what[i]) && !(what[i] >= 'A' && what[i] <= 'F') && !(what[i] >= 'a' && what[i] <= 'f')) return false;
	return true;
}

bool CheckBool(const string& what)
{
    string wh2 = ToLower(Trim(what));
    if(wh2 == "true" || wh2 == "false" || wh2 == "yes" || wh2 == "no" || wh2 == "y" || wh2 == "n" || wh2 == "0" || wh2 == "1")
        return true;
    return false;
}

bool StrToBool(const string& what)
{
	string cr = Trim(ToLower(what));
	if(cr == "yes" || cr == "true" || cr == "1" || cr == "y")
		return true;
	return false;
}

#include <windows.h>

HANDLE PrintfMutex = NULL;

void Printf(unsigned long level, const string& format, ...)
{
    if(Config::LogLevel < level) return;
    if(!PrintfMutex) PrintfMutex = CreateMutex(NULL, TRUE, NULL);
    else WaitForSingleObject(PrintfMutex, INFINITE);

    char* line = NULL;
    va_list list;
    va_start(list, format);
    size_t linesize = (vsnprintf(NULL, 0, format.c_str(), list) + 1);
    line = new char[linesize + 1];
    if(!line) return;
    line[linesize] = 0;
    vsnprintf(line, linesize, format.c_str(), list);
    va_end(list);

    time_t t;
    struct tm *tm;
    time(&t);
    tm = localtime(&t);

    printf("[%2u.%02u.%04u %2u:%02u:%02u] %s", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, line);

    FILE* f_log = fopen(Config::LogFile.c_str(), "a");
    if(f_log)
    {
        fprintf(f_log, "[%2u.%02u.%04u %2u:%02u:%02u] %s", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, line);
        fclose(f_log);
    }

    delete[] line;

    ReleaseMutex(PrintfMutex);
}

void PrintfT(unsigned long level, const string& format, ...)
{
    if(Config::LogLevel < level) return;
    if(!PrintfMutex) PrintfMutex = CreateMutex(NULL, TRUE, NULL);
    else WaitForSingleObject(PrintfMutex, INFINITE);

    char* line = NULL;
    va_list list;
    va_start(list, format);
    size_t linesize = (vsnprintf(NULL, 0, format.c_str(), list) + 1);
    line = new char[linesize + 1];
    if(!line) return;
    line[linesize] = 0;
    vsnprintf(line, linesize, format.c_str(), list);
    va_end(list);

    time_t t;
    struct tm *tm;
    time(&t);
    tm = localtime(&t);

    printf(line);

    FILE* f_log = fopen(Config::LogFile.c_str(), "a");
    if(f_log)
    {
        fprintf(f_log, line);
        fclose(f_log);
    }

    delete[] line;

    ReleaseMutex(PrintfMutex);
}
