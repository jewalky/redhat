#ifndef SQLIB_HPP_INCLUDED
#define SQLIB_HPP_INCLUDED

#include <winsock2.h>
#include <string>
#include <mysql.h>
#include <map>

namespace sql
{
    typedef class error_t
    {
        public:
            error_t(MYSQL* connection);

            std::string as_string();
            uint32_t as_uint();

        private:
            uint32_t myNumber;
            std::string myString;
    } error_t;

    typedef class row_item_t
    {
        public:
            row_item_t();
            row_item_t(std::string content);

            std::string as_string();
            uint32_t as_uint();
            int32_t as_int();
            double as_float();

            operator bool() { return myValid; }

        private:
            bool myValid;
            std::string myString;
    } row_item_t;

    typedef class row_t
    {
        public:
            row_t(MYSQL_RES* result);
            row_item_t get_item(std::string name);

            operator bool() { return myValid; }

        private:
            bool myValid;
            std::map<std::string, std::string> myItems;
    } row_t;

    typedef class connection_t
    {
        public:
            connection_t();
            ~connection_t();

            bool connect(std::string hostname, uint16_t port, std::string username, std::string password, std::string database, uint32_t flags);
            void close();

            error_t get_last_error();

            std::string escape(std::string string);
            bool query(std::string what);

            uint32_t affected_rows();
            uint32_t num_rows();

            row_t fetch_row();

        private:
            MYSQL* myConnection;
            MYSQL_RES* myResult;
    } connection_t;
};

#endif // SQLIB_HPP_INCLUDED
