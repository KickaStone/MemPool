#include "PageCache.h"

PageCache PageCache::_sInst;

Span *PageCache::NewSpan(size_t k)
{
    assert(k > 0 && k < PAGE_NUM); // 申请页数有范围

    // (1) k号桶中有span
    if (!_spanLists[k].Empty())
    {
        Span *span = _spanLists[k].PopFront();

        // 记录pageId和span的映射关系
        for (PageId i = 0; i < span->_n; ++i)
        {
            // _idSpanMap[span->_pageId + i] = span;
            _pageMap.set(span->_pageId + i, span);
        }
        return span;
    }

    // (2) k号桶没有span，但后面的桶有span
    for (size_t i = k + 1; i < PAGE_NUM; ++i)
    {
        // i 桶有span， 对该span进行切分
        if (!_spanLists[i].Empty())
        {
            // 获取桶中span， 起名nSpan
            Span *nSpan = _spanLists[i].PopFront();

            /* 将nSpan切分成一个k页的span和一个n-k页的span */

            // Span的空间需要新建， 而不是用当前内存池中的空间
            // _spanPool._poolMtx.lock();
            Span *kSpan = _spanPool.New();
            // _spanPool._poolMtx.unlock();
            // Span *kSpan = new Span;  

            // 分一个k页的span叫kSpan
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;

            // nSpan调整大小
            nSpan->_pageId += k;
            nSpan->_n -= k;

            // 将n-k页的span挂到n-k号桶中
            _spanLists[nSpan->_n].PushFront(nSpan);

            // 边缘页映射，方便合并
            // _idSpanMap[nSpan->_pageId] = nSpan;                 // 第一页
            _pageMap.set(nSpan->_pageId, nSpan);
            // _idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan; // 最后一页
            _pageMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

            // 返回kSpan
            // _idSpanMap[kSpan->_pageId] = kSpan; // 记录pageId和span的映射关系
            _pageMap.set(kSpan->_pageId, kSpan);

            for (PageId i = 0; i < kSpan->_n; ++i)
            {
                // _idSpanMap[kSpan->_pageId + i] = kSpan;
                _pageMap.set(kSpan->_pageId + i, kSpan);
            }
            return kSpan;
        }
    }

    // (3) k号桶没有span，后面的桶也没有span

    /* 走到这里说明没有128页的span， 需要向系统申请128页的span */
    void *ptr = SystemAlloc(PAGE_NUM - 1); // PAGE_NUM为129

    Span *bigSpan = _spanPool.New();

    // 只需要修改bigSpan的_pageId和_n
    bigSpan->_pageId = (PageId)ptr >> PAGE_SHIFT;
    bigSpan->_n = PAGE_NUM - 1;

    // 将bigSpan挂到128号桶中
    _spanLists[PAGE_NUM - 1].PushFront(bigSpan);
    return NewSpan(k);
}

Span *PageCache::MapObjectToSpan(void *obj)
{
    PageId id = ((PageId)obj) >> PAGE_SHIFT;

    // std::unique_lock<std::mutex> lock(_pageMtx);
    // auto it = _idSpanMap.find(id);
    auto it = _pageMap.get(id);
    if(it != nullptr)
    {
        return (Span*)it;
    }
    cout << "PageCache::MapObjectToSpan: " << id << " not found" << endl;
    return nullptr;
}

void PageCache::ReleaseSpanToPageCache(Span *span)
{
    // 向左合并
    while (true)
    {
        PageId leftId = span->_pageId - 1;  // 左边相邻页id
        auto ret = _pageMap.get(leftId); // 相邻页对应span在划分时会加入到_idSpanMap中

        // 没有相邻页停止合并
        if (ret == nullptr)
        {
            break;
        }

        Span *leftSpan = (Span*)ret; // 相邻span指针
        // leftSpan在cc中停止合并
        if (leftSpan->isUse)
            break;

        // 合并后超过128页停止合并
        if (leftSpan->_n + span->_n > PAGE_NUM - 1)
        {
            break;
        }

        span->_pageId = leftSpan->_pageId;
        span->_n += leftSpan->_n;

        _spanLists[leftSpan->_n].Erase(leftSpan);

        // 删除相邻页的映射 原文没有的一点
        // _idSpanMap.erase(leftSpan->_pageId);
        // _idSpanMap.erase(leftSpan->_pageId + leftSpan->_n - 1);

        // delete leftSpan; // 删除相邻span对象 leftSpan是NewSpan中创建出来的数据，需要删除，并不会删掉其pageId和n代表的页空间
        _spanPool.Delete(leftSpan);
    }

    // 向右合并
    while (true)
    {
        PageId rightId = span->_pageId + span->_n; // 右边相邻页，注意span可能有多个page
        auto it = _pageMap.get(rightId);

        if (it == nullptr)
            break;

        Span *rightSpan = (Span*)it;
        if (rightSpan->isUse)
            break;

        if (rightSpan->_n + span->_n > PAGE_NUM - 1)
            break;

        span->_n += rightSpan->_n;

        _spanLists[rightSpan->_n].Erase(rightSpan);

        // _idSpanMap.erase(rightSpan->_pageId);
        // _idSpanMap.erase(rightSpan->_pageId + rightSpan->_n - 1);

        // delete rightSpan;
        _spanPool.Delete(rightSpan);
    }

    // 把合并后的span挂到对应桶中
    _spanLists[span->_n].PushFront(span);

    // _idSpanMap[span->_pageId] = span;
    // _idSpanMap[span->_pageId + span->_n - 1] = span;
    _pageMap.set(span->_pageId, span);
    _pageMap.set(span->_pageId + span->_n - 1, span);
}
