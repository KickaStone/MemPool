#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst; // 避免链接错误，在cpp中初始化

/**
 * @brief 从中心缓存获取一定数量的对象
 * @param start 提供空间的开始，输出型参数
 * @param end 提供空间的结尾，输出型参数
 * @param batchNum tc需要多少块size大小的空间
 * @param size 单块空间大小
 * @return cc实际提供的空间大小
 */
size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t alignSize)
{
    size_t index = SizeClass::Index(alignSize);
    // 考虑3种情况

    _spanLists[index]._mtx.lock(); // 对cc中spanlist操作需要加锁，保证线程安全 // 这个锁会在GetOneSpan里提前解锁

    Span *span = GetOneSpan(_spanLists[index], alignSize);

    assert(span);
    assert(span->_freelist);

    // 起初都指向_freelist, end向后移动
    start = end = span->_freelist;

    size_t actualNum = 1; // 想想为什么从1开始
    size_t i = 0;

    while (i < batchNum - 1 && ObjNext(end) != nullptr)
    {
        end = ObjNext(end);
        ++actualNum; // 记录步数
        ++i;
    }

    span->_freelist = ObjNext(end); // 更新span的_freelist
    span->use_count += actualNum;   // 把分出去的块添加到use_count上去，方便之后回收

    ObjNext(end) = nullptr; // 将end的next置空, 因为ObjNext返回引用，可以直接操作

    // 如果end == nullptr, 说明span中没有足够空间

    _spanLists[index]._mtx.unlock();
    return actualNum;
}

Span *CentralCache::GetOneSpan(SpanList &spanList, size_t alignSize)
{
    Span *it = spanList.Begin();
    while (it != spanList.End())
    {
        if (it->_freelist != nullptr)
        {
            return it;
        }
        it = it->_next;
    }

    // 【重要】将cc的桶锁解掉，为的是其他线程能把内存归还到桶里
    spanList._mtx.unlock();

    // 如果遍历完所有span都没有找到非空的span，则需要从PC中获取span
    size_t pages = SizeClass::NumMovePage(alignSize); // size 转换成匹配的页数，以提供pc一个合适的span

    // 解决死锁方法，在调用NewSpan的位置加锁
    PageCache::GetInstance()->_pageMtx.lock();
    Span *span = PageCache::GetInstance()->NewSpan(pages); // 调用NewSpan获取新span
    span->isUse = true; // 设置span正在被使用
    span->_objSize = alignSize; // 设置span管理的块大小
    PageCache::GetInstance()->_pageMtx.unlock();

    // 处理拿到的新span, 新span只设置了_pageId和_n， 自由链表为空

    // 使用span的_pageId和_n 结合单页大小， 计算出span的起始地址和结束地址

    // 起始地址 = 通过页号获取。
    char* start = (char*)(span->_pageId << PAGE_SHIFT);    // start = 页号*单页大小
    char* end = (char*)(start + (span->_n << PAGE_SHIFT)); // end = start + 页数*单页大小

    // 将span的_freelist指向start
    span->_freelist = start;

    // 切分span 切好做成自由链表放到span->_freelist
    
    void *tail = start;
    start += alignSize; // start向后移动alignSize个字节
    // start 先动
    // 链接各个块
    while (start < end)
    {
        ObjNext(tail) = start; // 将start的地址赋值给tail的next
        start += alignSize;    // start向后移动alignSize个字节
        tail = ObjNext(tail);
    }

    ObjNext(tail) = nullptr; // 最后一位置空, 不然指向的是end，会出现未定义行为

    // 【重要】将cc的桶锁加回来，因为下面要操作桶
    spanList._mtx.lock();
    spanList.PushFront(span); // 将span放入spanList

    return span;
}

void CentralCache::ReleaseListToSpans(void *start, size_t alignSize)
{
    // 找到spanList的位置
    size_t index = SizeClass::Index(alignSize);

    // 对spanlist操作要加锁
    _spanLists[index]._mtx.lock();

    while (start)
    {
        void *next = ObjNext(start);

        // 找到start对应的span
        Span *span = PageCache::GetInstance()->MapObjectToSpan(start);

        // 将start插入到span的freelist中 头插法
        ObjNext(start) = span->_freelist;
        span->_freelist = start;

        span->use_count--; // 使用计数减1

        if (span->use_count == 0)
        {
            // 先从spanList中删除
            _spanLists[index].Erase(span);
            span->_freelist = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            // 把当前cc中的桶解锁，以便其他线程可以获取桶中的span
            _spanLists[index]._mtx.unlock(); 

            // 对pc加锁，因为要操作pc的spanList
            PageCache::GetInstance()->_pageMtx.lock();
            // 如果span的use_count为0，则将span还给PC
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->_pageMtx.unlock();

            _spanLists[index]._mtx.lock(); // 归还完毕，加锁
        }

        start = next; // 下一块
    }

    _spanLists[index]._mtx.unlock();
}
