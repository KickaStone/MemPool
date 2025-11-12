#pragma once
#include <thread>
#include "ThreadCache.h"
#include "PageCache.h"

// 仿tcmalloc的接口
void *ConcurrentAlloc(size_t size){
    // 获取线程id
    // cout << std::this_thread::get_id() << " " << pTLSThreadCache << endl;

    if(pTLSThreadCache == nullptr){ // 不存在线程安全问题，每个线程相互独立
        // pTLSThreadCache = new ThreadCache; // 每个线程独立， 所以需要new
        static lockfree::ObjectPool<ThreadCache> threadCachePool;
        pTLSThreadCache = threadCachePool.New();
    }
    return pTLSThreadCache->Allocate(size);
}

void ConcurrentFree(void *ptr){ // 第二个参数以后会去掉
    assert(ptr);
    size_t size = PageCache::GetInstance()->MapObjectToSpan(ptr)->_objSize;
    assert(size <= MAX_BYTES);

    pTLSThreadCache->Deallocate(ptr, size);
}