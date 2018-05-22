#ifndef ID_ALLOCTOR_H
#define ID_ALLOCTOR_H

#include <sstream>

#include "TypeDefine.h"


const uint16 alloc_id_max_limit = 0xffff;

struct id_alloctor;

id_alloctor* id_alloctor_new(uint16 nCount);
void id_alloctor_del(id_alloctor* pAlloc);

uint16 id_alloctor_alloc(id_alloctor* pAlloc);
void id_alloctor_free(id_alloctor* pAlloc, uint16 id);

void id_alloctor_dump(id_alloctor* pAlloc, std::ostream* pStream);

#endif // ID_ALLOCTOR_H