#include "IdAlloctor.h"

#include <assert.h>

struct id_alloctor
{
    uint16* m_pOIds;
    uint16 m_nAll;
    uint16 m_nAlloc;
    bool* m_pFlags;
};

id_alloctor* id_alloctor_new(uint16 nCount)
{
    if (nCount == 0 || nCount >= alloc_id_max_limit) {
        return NULL;
    }
    id_alloctor* pAlloc = new id_alloctor;
    pAlloc->m_nAll = nCount;
    pAlloc->m_nAlloc = 0;
    pAlloc->m_pOIds = new uint16[nCount];
    for (uint16 i = 0; i < nCount; ++i) {
        pAlloc->m_pOIds[i] = i;
    }
    pAlloc->m_pFlags = new bool[nCount];
    memset(pAlloc->m_pFlags, 0, sizeof(pAlloc->m_pFlags[0])*nCount);

    return pAlloc;
}

void id_alloctor_del(id_alloctor* pAlloc)
{
    delete[] pAlloc->m_pOIds;
    pAlloc->m_pOIds = NULL;
    
    pAlloc->m_nAll = 0;
    pAlloc->m_nAlloc = 0;
    
    delete[] pAlloc->m_pFlags;
    pAlloc->m_pFlags = NULL;

    delete pAlloc;
}

uint16 id_alloctor_alloc(id_alloctor* pAlloc)
{
    uint16 id = alloc_id_max_limit;
    if (pAlloc->m_nAlloc < pAlloc->m_nAll) {
        id = pAlloc->m_pOIds[pAlloc->m_nAlloc++];
        pAlloc->m_pFlags[id] = true;
    }
    return id;
}

void id_alloctor_free(id_alloctor* pAlloc, uint16 id)
{
    if (id >= pAlloc->m_nAll) {
        return ;
    }
    if (pAlloc->m_nAlloc > 0) {
        if (pAlloc->m_pFlags[id]) {
            pAlloc->m_pOIds[--pAlloc->m_nAlloc] = id;     
            pAlloc->m_pFlags[id] = false;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void id_alloctor_dump( id_alloctor* pAlloc, std::ostream* pStream )
{
    std::ostream& stream = *pStream;

    stream << "all:" << pAlloc->m_nAll << ", "
           << "alloc:" << pAlloc->m_nAlloc << ", "
           << "free:" << pAlloc->m_nAll-pAlloc->m_nAlloc << std::endl;


    stream << "[alloc_list]: ";
    for (uint16 i = 0; i < pAlloc->m_nAll; ++i) {
        if (pAlloc->m_pFlags[i]) {
            stream << i << " ";
        }
    }
    stream << std::endl;


    stream << "[free_list]: ";
    for (uint16 i = pAlloc->m_nAlloc; i < pAlloc->m_nAll; ++i) {
        stream << pAlloc->m_pOIds[i] << " ";
    }
    stream << std::endl;


    for (uint16 i = pAlloc->m_nAlloc; i < pAlloc->m_nAll; ++i) {
        uint16 id = pAlloc->m_pOIds[i];
        assert(!pAlloc->m_pFlags[id]);
    }
    uint16 nCalcFreeCount = 0;
    for (uint16 i = 0; i < pAlloc->m_nAll; ++i) {
        if (!pAlloc->m_pFlags[i]) {
            ++nCalcFreeCount;
        }
    }
    assert(nCalcFreeCount == (pAlloc->m_nAll-pAlloc->m_nAlloc));
    
}
