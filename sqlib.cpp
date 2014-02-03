#include "sqlib.hpp"
#include "utils.hpp"

namespace sql
{
    connection_t::connection_t()
    {
        myConnection = NULL;
        myResult = NULL;
    }

    connection_t::~connection_t()
    {
        if(myConnection)
        {
            mysql_close(myConnection);
            myConnection = NULL;
        }
    }

    bool connection_t::connect(std::string hostname, uint16_t port, std::string username, std::string password, std::string database, uint32_t flags)
    {
        myConnection = mysql_init(NULL);
        if(!myConnection) return false;
        my_bool b_reconnect = true;
        mysql_options(myConnection, MYSQL_OPT_RECONNECT, &b_reconnect);
        if(!mysql_real_connect(myConnection, (hostname.length() ? hostname.c_str() : NULL),
                                             (username.length() ? username.c_str() : NULL),
                                             (password.length() ? password.c_str() : NULL),
                                             (database.length() ? database.c_str() : NULL),
                                             port,
                                             NULL,
                                             flags))
        {
            mysql_close(myConnection);
            myConnection = NULL;
            return false;
        }

        return true;
    }

    void connection_t::close()
    {
        if(myResult)
        {
            mysql_free_result(myResult);
            myResult = NULL;
        }

        if(myConnection)
        {
            mysql_close(myConnection);
            myConnection = NULL;
        }
    }

    error_t connection_t::get_last_error()
    {
        return error_t(myConnection);
    }

    error_t::error_t(MYSQL* connection)
    {
        if(!connection)
        {
            myNumber = 0;
            myString = "Not connected.";
        }

        myNumber = mysql_errno(connection);
        myString = mysql_error(connection);
    }

    std::string error_t::as_string()
    {
        return myString;
    }

    uint32_t error_t::as_uint()
    {
        return myNumber;
    }

    std::string connection_t::escape(std::string string)
    {
        std::string output = "";
        for(std::string::iterator it = string.begin(); it != string.end(); ++it)
        {
            char ch = (*it);
            if(ch == '\\') output += "\\\\";
            else if(ch == '\'') output += "\\'";
            else output += ch;
        }

        return output;
    }

    bool connection_t::query(std::string what)
    {
        if(!myConnection) return false;
        if(myResult) mysql_free_result(myResult);
        if(mysql_real_query(myConnection, what.data(), what.length()) != 0) return false;
        myResult = mysql_store_result(myConnection);

        if(!myResult && mysql_field_count(myConnection))
            return false;
        return true;
    }

    uint32_t connection_t::affected_rows()
    {
        if(!myConnection) return 0;
        return mysql_affected_rows(myConnection);
    }

    uint32_t connection_t::num_rows()
    {
        if(!myConnection) return 0;
        if(!myResult) return 0;
        return mysql_num_rows(myResult);
    }

    row_t connection_t::fetch_row()
    {
        return row_t(myResult);
    }

    row_t::row_t(MYSQL_RES* result)
    {
        if(!result)
        {
            myValid = false;
            return;
        }

        MYSQL_ROW row = NULL;
        row = mysql_fetch_row(result);

        if(!row)
        {
            myValid = false;
            return;
        }

        uint32_t num_fields = mysql_num_fields(result);
        uint32_t* lengths = (uint32_t*)mysql_fetch_lengths(result);
        MYSQL_FIELD* fields = mysql_fetch_fields(result);

        for(uint32_t i = 0; i < num_fields; i++)
        {
            std::string fld_name = fields[i].name;
            char* fld_str = new char[lengths[i] + 1];
            memset(fld_str, 0, lengths[i] + 1);
            memcpy(fld_str, row[i], lengths[i]);

            std::string fld_content;
            fld_content.resize(lengths[i]);
            for(uint32_t j = 0; j < fld_content.size(); j++)
                fld_content[j] = fld_str[j];

            delete[] fld_str;

            myItems[fld_name] = fld_content;
        }

        myValid = true;
    }

    row_item_t row_t::get_item(std::string field)
    {
        for(std::map<std::string, std::string>::iterator it = myItems.begin(); it != myItems.end(); ++it)
        {
            if(it->first == field)
            {
                return row_item_t(it->second);
            }
        }
        return row_item_t();
    }

    row_item_t::row_item_t()
    {
        myValid = false;
    }

    row_item_t::row_item_t(std::string value)
    {
        myValid = true;
        myString = value;
    }

    uint32_t row_item_t::as_uint()
    {
        if(CheckInt(myString))
            return StrToInt(myString);
        return 0;
    }

    int32_t row_item_t::as_int()
    {
        return (int32_t)as_uint();
    }

    std::string row_item_t::as_string()
    {
        return myString;
    }

    double row_item_t::as_float()
    {
        if(CheckFloat(myString))
            return StrToFloat(myString);
        return 0.0;
    }
}
