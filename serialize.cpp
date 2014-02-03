#include "serialize.hpp"
#include "utils.hpp"

#include <cstring>
#include <string>
#include <fstream>
#include <iostream>

using namespace std;

Archive::Archive()
{
    myPosRead = 0;
    myPosWrite = 0;
    myFail = false;
}

void Archive::AppendData(uint8_t* data, uint32_t count)
{
    for(uint32_t i = 0; i < count; i++)
    {
        myData.push_back(data[i]);
        myPosWrite++;
    }
}

void Archive::GetData(uint8_t* data, uint32_t count)
{
    uint32_t oldPosRead = myPosRead;
    for(uint32_t i = 0; i < count; i++)
    {
        if(myPosRead >= myData.size())
        {
            for(uint32_t j = 0; j < count; j++)
                data[j] = 0;
            myFail = true;
            break;
        }
        data[i] = myData[oldPosRead+i];
        myPosRead++;
    }
}

Archive& Archive::operator << (uint8_t data)
{
    AppendData(&data, 1);
    return *this;
}

Archive& Archive::operator << (uint16_t data)
{
    AppendData((uint8_t*)&data, 2);
    return *this;
}

Archive& Archive::operator << (uint32_t data)
{
    AppendData((uint8_t*)&data, 4);
    return *this;
}

Archive& Archive::operator << (uint64_t data)
{
    AppendData((uint8_t*)&data, 8);
    return *this;
}

Archive& Archive::operator << (int8_t data)
{
    AppendData((uint8_t*)&data, 1);
    return *this;
}

Archive& Archive::operator << (int16_t data)
{
    AppendData((uint8_t*)&data, 2);
    return *this;
}

Archive& Archive::operator << (int32_t data)
{
    AppendData((uint8_t*)&data, 4);
    return *this;
}

Archive& Archive::operator << (int64_t data)
{
    AppendData((uint8_t*)&data, 8);
    return *this;
}

Archive& Archive::operator << (const std::string& data)
{
    uint16_t strsiz = data.length();
    AppendData((uint8_t*)&strsiz, 2);
    AppendData((uint8_t*)data.c_str(), data.length()+1);
    return *this;
}

Archive& Archive::operator << (const char* data)
{
    uint32_t strsiz = strlen(data);
    AppendData((uint8_t*)data, strsiz);
    return *this;
}

Archive& Archive::operator << (bool data)
{
    uint8_t what = (uint8_t)(data != 0);
    AppendData(&what, 1);
    return *this;
}

Archive& Archive::operator << (double data)
{
    double what = data;
    AppendData((uint8_t*)&what, 8);
    return *this;
}

Archive& Archive::operator >> (uint8_t &data)
{
    GetData(&data, 1);
    return *this;
}

Archive& Archive::operator >> (uint16_t &data)
{
    GetData((uint8_t*)&data, 2);
    return *this;
}

Archive& Archive::operator >> (uint32_t &data)
{
    GetData((uint8_t*)&data, 4);
    return *this;
}

Archive& Archive::operator >> (uint64_t &data)
{
    GetData((uint8_t*)&data, 8);
    return *this;
}

Archive& Archive::operator >> (int8_t &data)
{
    GetData((uint8_t*)&data, 1);
    return *this;
}

Archive& Archive::operator >> (int16_t &data)
{
    GetData((uint8_t*)&data, 2);
    return *this;
}

Archive& Archive::operator >> (int32_t &data)
{
    GetData((uint8_t*)&data, 4);
    return *this;
}

Archive& Archive::operator >> (int64_t &data)
{
    GetData((uint8_t*)&data, 8);
    return *this;
}

Archive& Archive::operator >> (std::string& data)
{
    uint16_t strsiz;
    GetData((uint8_t*)&strsiz, 2);
    data.resize(strsiz);
    for(uint32_t i = 0; i < strsiz; i++)
        GetData((uint8_t*)&data[i], 1);
    myPosRead++;
    return *this;
}

Archive& Archive::operator >> (char*& data)
{
    uint32_t rp = myPosRead;
    for(uint32_t i = 0; i+rp < myData.size(); i++)
    {
        GetData((uint8_t*)&data[i], 1);
        if(!data[i]) break;
        if(i+rp == myData.size()-1) data[i] = 0;
    }
    return *this;
}

Archive& Archive::operator >> (bool& data)
{
    uint8_t wh;
    GetData(&wh, 1);
    data = wh;
    return *this;
}

Archive& Archive::operator >> (double& data)
{
    double wh;
    GetData((uint8_t*)&wh, 8);
    data = wh;
    return *this;
}

Archive::operator bool ()
{
    return !myFail;
}

void Archive::GetAllData(uint8_t*& buf, uint32_t& count)
{
    count = myData.size();
    if(!buf) buf = new uint8_t[count];
    for(uint32_t i = 0; i < count; i++)
        buf[i] = myData[i];
}

void Archive::SetAllData(uint8_t* buf, uint32_t count)
{
    myData.clear();

    for(uint32_t i = 0; i < count; i++)
        myData.push_back(buf[i]);
}

void Archive::SaveToFile(string filename)
{
    ofstream f_temp;
    f_temp.open(filename.c_str(), ios::out | ios::binary);
    if(!f_temp.is_open())
    {
        myFail = true;
        return;
    }
    SaveToStream(f_temp);
    f_temp.close();
}

void Archive::LoadFromFile(string filename)
{
    ifstream f_temp;
    f_temp.open(filename.c_str(), ios::in | ios::binary);
    if(!f_temp.is_open())
    {
        myFail = true;
        return;
    }
    LoadFromStream(f_temp);
    f_temp.close();
}

void Archive::SaveToStream(ostream& stream)
{
    uint8_t* buf = NULL;
    uint32_t siz = 0;
    GetAllData(buf, siz);
    stream.write((char*)buf, siz);
    delete buf;
}

void Archive::LoadFromStream(istream& stream)
{
    uint8_t* buf = NULL;
    stream.seekg(0, ios::end);
    uint32_t siz = stream.tellg();
    stream.seekg(0, ios::beg);
    buf = new uint8_t[siz];
    stream.read((char*)buf, siz);

    SetAllData(buf, siz);

    delete buf;
}

void Archive::Reset()
{
    SetAllData(NULL, 0);
    myPosRead = 0;
    myPosWrite = 0;
}

void Archive::ResetPosition()
{
	myPosRead = 0;
	myPosWrite = 0;
}

uint32_t Archive::GetLength()
{
    return myData.size();
}
