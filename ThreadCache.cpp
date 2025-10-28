#include "ThreadCache.h"
#include "CentralCache.h"

void *ThreadCache::Allocate(size_t size)
{
    assert(size <= MAX_BYTES);

    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(alignSize);

    if(!_freeLists[index].Empty()){
        return _freeLists[index].Pop(); // 直接从自由链表获取空间
    }else{
        return FetchFromCentralCache(index, alignSize); // 从中心缓存获取空间
    }

}

void ThreadCache::Deallocate(void *ptr, size_t alignSize)
{
    assert(ptr);    // 回收空间不能为空
    assert(alignSize <= MAX_BYTES); // 回收空间不能超过256KB, alignSize 已经对齐过

    size_t index = SizeClass::Index(alignSize); // 找到对应的桶
    _freeLists[index].Push(ptr); // 将空间返回给自由链表

    if(_freeLists[index].Size() >= _freeLists[index].MaxSize()){
        ListTooLong(_freeLists[index], alignSize);
    }
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
    // 实现SizeClass::NumMoveSize(size)后，再实现
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));

    if(batchNum == _freeLists[index].MaxSize()){
        _freeLists[index].MaxSize() += 1; // 下次多给一块
    }

    /* 上面就是慢开始反馈调节算法 */

    // 从cc获取batchNum个size大小的空间
    void* start = nullptr;
    void* end = nullptr;

    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);
    // 根据actualNum决定后续操作
    assert(actualNum >= 1);

    if(actualNum == 1){ // 如果只取到一块，则直接返回
        assert(start == end);
        return start;
    }

    // 留下一块，把剩下[OjbNext(start), end]的加入freeList
    _freeLists[index].PushRange(ObjNext(start), end, actualNum - 1); // 这里actualNum - 1是因为已经分给tc一块

    return start;
}

void ThreadCache::ListTooLong(FreeList& list, size_t alignSize)
{
    void* start = nullptr;
    void* end = nullptr;

    list.PopRange(start, end, list.MaxSize());

    CentralCache::GetInstance()->ReleaseListToSpans(start, alignSize); // 不需要传end， 因为popRange保证后面是空，所以只需要判断nex是不是k |

}
