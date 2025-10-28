#pragma once

#include "Common.h"

class CentralCache{

public:
    static CentralCache* GetInstance(){
        return &_sInst;
    }

    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t alignSize); // cc从自己的_spanListss中为tc提供所需块

    Span* GetOneSpan(SpanList& spanList, size_t size); // 从spanList中获取一个非空的span

    void ReleaseListToSpans(void* start, size_t size);

private:
    // 单例去掉构造析构和拷贝构造
    CentralCache(){}
    CentralCache(const CentralCache&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;

    SpanList _spanLists[FREE_LIST_NUM];
    static CentralCache _sInst; // 饿汉单例模式
};
