#ifndef VERSION_HPP_INCLUDED
#define VERSION_HPP_INCLUDED

#include <stdint.h>
uint32_t V_AddSession(uint32_t key);
uint32_t V_GetSession(uint32_t session);
void V_DelSession(uint32_t session);

void V_ChangeSession(uint32_t session, uint32_t key);

#endif // VERSION_HPP_INCLUDED
