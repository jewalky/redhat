#ifndef SERIALIZE_H_INCLUDED
#define SERIALIZE_H_INCLUDED

#include <vector>
#include <string>
#include <stdint.h>

class Archive
{
    public:
        Archive();

        Archive& operator << (uint8_t second);
        Archive& operator << (uint16_t second);
        Archive& operator << (uint32_t second);
        Archive& operator << (uint64_t second);
        Archive& operator << (int8_t second);
        Archive& operator << (int16_t second);
        Archive& operator << (int32_t second);
        Archive& operator << (int64_t second);
        Archive& operator << (const std::string& second);
        Archive& operator << (const char* second);
        Archive& operator << (bool second);
        Archive& operator << (double second);

        Archive& operator >> (uint8_t& second);
        Archive& operator >> (uint16_t& second);
        Archive& operator >> (uint32_t& second);
        Archive& operator >> (uint64_t& second);
        Archive& operator >> (int8_t& second);
        Archive& operator >> (int16_t& second);
        Archive& operator >> (int32_t& second);
        Archive& operator >> (int64_t& second);
        Archive& operator >> (std::string& second);
        Archive& operator >> (char*& second);
        Archive& operator >> (bool& second);
        Archive& operator >> (double& second);

        operator bool ();

        void AppendData(uint8_t* buf, uint32_t count);
        void GetData(uint8_t* buf, uint32_t count);

        void GetAllData(uint8_t*& buf, uint32_t& count);
        void SetAllData(uint8_t* buf, uint32_t count);

        void SaveToFile(std::string filename);
        void LoadFromFile(std::string filename);

        void SaveToStream(std::ostream& stream);
        void LoadFromStream(std::istream& stream);

        void Reset();
		void ResetPosition();

		uint32_t GetLength();

    protected:
        uint32_t myPosRead;
        uint32_t myPosWrite;

        bool myFail;

        std::vector<uint8_t> myData;
};

#endif // SERIALIZE_H_INCLUDED
