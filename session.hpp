#ifndef SESSION_HPP_INCLUDED
#define SESSION_HPP_INCLUDED

#include <string>
void SESSION_AddLogin(unsigned long id1, unsigned long id2);
void SESSION_SetLogin(unsigned long id1, unsigned long id2, unsigned long value);
unsigned long SESSION_GetLogin(unsigned long id1, unsigned long id2);
void SESSION_DelLogin(unsigned long id1, unsigned long id2);

#endif // SESSION_HPP_INCLUDED
