#ifndef PACKET_HPP_INCLUDED
#define PACKET_HPP_INCLUDED

#include "serialize.hpp"

typedef Archive Packet;

extern const unsigned char key_08[0x50], key_10[0x50], key_11[0x50], key_15[0x50], key_20[0x50];

void PACKET_XorByKey(unsigned char* data, unsigned long size, unsigned long protover);
void PACKET_Crypt(Packet& packet, unsigned long protover);

#endif // PACKET_HPP_INCLUDED
