#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <sys/mman.h>
#include <mutex>
#include <sys/mman.h>
#include <unordered_map>

static const size_t FREE_LIST_NUM = 208;   // 哈希桶中自由链表个数
static const size_t MAX_BYTES = 256 * 1024; // TC单次申请最大字节数
static const size_t PAGE_NUM = 129; // 页数 129避免-1
static const size_t PAGE_SHIFT = 12; // 一页4KB, 右移12位得到页号 
// 对于云服务器 通常是4KB每页

using std::cout;
using std::endl;
using std::vector;

static void *&ObjNext(void *obj)
{ // 返回引用，没有引用返回的就是右值
    return *(void **)obj;
}

class FreeList
{
public:
    void Push(void *obj)
    {
        assert(obj);

        // 回收空间
        // 头插法
        ObjNext(obj) = _freeList;
        _freeList = obj;
        _size++;
    }

    // 用于FetchFromCentralCache中，将[start, end]的内存块加入freeList
    void PushRange(void* start, void* end, size_t size){
        ObjNext(end) = _freeList;
        _freeList = start;
        _size += size;
    }

    void *Pop()
    {
        assert(_freeList);
        // 拿出空间给申请者
        // 头删法
        void *obj = _freeList;
        _freeList = ObjNext(obj);
        _size--;
        return obj;
    }

    void PopRange(void*& start, void*& end, size_t n){
        assert(n <= _size); // 块数不能超过_size

        start = end = _freeList;

        for(size_t i = 0; i < n - 1; ++i){
            end = ObjNext(end);
        }

        _freeList = ObjNext(end);
        ObjNext(end) = nullptr;
        _size -= n;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }

    size_t& MaxSize()
    {
        return _maxSize;
    }

    size_t Size()
    {
        return _size;
    }

private:
    void *_freeList = nullptr; // 自由链表, 初始为空
    size_t _maxSize = 1; // 【慢开始反馈调节】未达到上限时，当前能够申请的最大块空间是多少
    size_t _size = 0; // 当前自由链表中有多少块空间
};

typedef size_t PageId; // size_t 会根据平台不同而不同，无需其他处理

struct Span{
    PageId _pageId = 0; // 页号
    size_t _n = 0; // 页数
    size_t _objSize = 0; // span管理页被切分的块有多大

    void* _freelist = nullptr; // 自由链表
    size_t use_count = 0; // 使用计数

    Span* _next = nullptr; // 双向链表
    Span* _prev = nullptr; // 双向链表

    bool isUse = false; // true: 在cc中， false: 在pc中， 辅助回收
};

class SpanList{

public:
    std::mutex _mtx; // 每个桶有自己的锁

public:
    SpanList(){
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }
    void PushFront(Span* span){
        Insert(Begin(), span);
    }

    // 获取到_head后的第一个span
    Span* PopFront(){
        assert(!Empty());
        
        // 先获取到_head后的第一个span
        Span* front = _head->_next;
        // 删除这个span
        Erase(front);

        // 返回原来的第一个span
        return front;
    }


    void Insert(Span* pos, Span* ptr){
        assert(pos);
        assert(ptr);

        Span* prev = pos->_prev;

        // prev <-> pos
        // prev <-> ptr <-> pos

        prev->_next = ptr;
        ptr->_prev = prev;
        ptr->_next = pos;
        pos->_prev = ptr;
    }

    void Erase(Span* pos){
        assert(pos);
        assert(pos != _head);

        // prev <-> pos <-> next
        Span* prev = pos->_prev;
        Span* next = pos->_next;

        prev->_next = next;
        next->_prev = prev;

        pos->_next = nullptr;
        pos->_prev = nullptr;
    }

    Span* Begin(){
        return _head->_next;    
    }
    Span* End(){
        return _head;
    }

    bool Empty(){
        return _head->_next == _head;
    }

private:
    Span* _head = nullptr;
    
};



class SizeClass{

public:
    static size_t RoundUp(size_t size){
        if(size <= 128){
            // [1, 128] 8B
            return _RoundUp(size, 8);
        }else if(size <= 1024){
            // [128, 1024] 16B
            return _RoundUp(size, 16);
        }else if(size <= 8*1024){
            // [1024, 8*1024] 64B
            return _RoundUp(size, 64);
        }else if(size <= 64*1024){
            // [8*1024, 64*1024] 1024B
            return _RoundUp(size, 1024);
        }else if(size <= 256*1024){
            // [64*1024, 256*1024] 8KB
            return _RoundUp(size, 8*1024);
        }else{
            assert(false); // 不可能发生
            return -1;
        }
    }

    // 计算映射的哪一个桶下标
    static size_t Index(size_t size){
        assert(size <= MAX_BYTES);

        // 每个区间有多少桶
        static int group_array[4] = {16, 56, 56, 56};
        if(size <= 128){
            return _Index(size, 3);
        }else if(size <= 1024){
            return group_array[0] + _Index(size - 128, 4);
        }else if(size <= 8*1024){
            return group_array[0] + group_array[1] + _Index(size - 1024, 7);
        }else if(size <= 64*1024){
            return group_array[0] + group_array[1] + group_array[2] + _Index(size - 8*1024, 10);
        }else if(size <= 256*1024){
            return group_array[0] + group_array[1] + group_array[2] + group_array[3] + _Index(size - 64*1024, 13);
        }else{
            assert(false); // 不可能发生
        }
        return -1;
    }

    // 单次申请块空间申请上限块数
    static size_t NumMoveSize(size_t size){
        assert(size > 0);

        int num = MAX_BYTES / size; // 单次申请块空间申请上限块数

        if(num > 512){
            num = 512;
        }

        if(num < 2){
            num = 2;
        }

        return num;
    }

    // 块页匹配算法 用于cc申请pc页时计算最大应该申请的页数
    static size_t NumMovePage(size_t size){

        size_t num = NumMoveSize(size); // 单次pc最多申请块数 

        size_t npage = num*size; // 通过最多申请块计算单次最大申请空间（不是size， size是实际请求的）

        npage >>= PAGE_SHIFT; // 右移13位得到页数， 向下取整应该没有问题？

        if(npage == 0){ // 最少分配1页
            npage = 1;
        }

        return npage;
    }

private:
    static size_t _RoundUp(size_t size, size_t alignNum){
        return (size + alignNum - 1) & ~(alignNum - 1); // 位运算，向上取整
    }

    static size_t _Index(size_t size, size_t align_shift){
        return ((size + ( 1 << align_shift) - 1) >> align_shift) - 1; // 位运算计算hash桶下标
    }
};


inline static void* SystemAlloc(size_t kpage){

    void* ptr = nullptr;

    auto size = kpage << PAGE_SHIFT;
    ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED){
        throw std::bad_alloc();
    }
    return ptr;

}