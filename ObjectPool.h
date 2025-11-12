/*定长内存池*/
#include <iostream>
#include <atomic>
#include "Common.h"
using std::cout;
using std::endl;

template <class T>
class ObjectPool
{
public:
	std::mutex _poolMtx;
	T *New() // 申请一个T类型大小的空间
	{
		T *obj = nullptr; // 最终返回的空间

		if (_freelist)
		{ // _freelist不为空，表示有回收的T大小的小块可以重复利用
			void *next = *(void **)_freelist;
			obj = (T *)_freelist;
			_freelist = next;
			// 头删操作
		}
		else
		{ // 自由链表中没有块，也就没有可以重复利用的空间
			// _memory中剩余空间小于T的大小的时候再开空间
			if (_remanentBytes < sizeof(T)) // 这样也会包含剩余空间为0的情况
			{

				if (sizeof(T) > 128 * 1024)
				{
					_remanentBytes = sizeof(T); // 应对大空间申请情况
				}
				else
				{
					_remanentBytes = 128 * 1024; // 开128K的空间 假设不会超过128KB
				}
				//_memory = (char*)malloc(_remanentBytes);

				// 右移PAGE_SHIFT位，就是除以页大小 向上取整
				size_t npage = (_remanentBytes + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
				_memory = (char *)SystemAlloc(npage);

				if (_memory == nullptr) // 开失败了抛异常
				{
					throw std::bad_alloc();
				}
			}

			obj = (T *)_memory; // 给定一个T类型的大小  并发执行下_memory可能是空指针，需要加锁保护
			// 判断一下T的大小，小于指针就给一个指针大小，大于指针就还是T的大小
			size_t objSize = sizeof(T) < sizeof(void *) ? sizeof(void *) : sizeof(T);
			_memory += objSize;		   // _memory后移一个T类型的大小
			_remanentBytes -= objSize; // 空间给出后_remanetBytes减少了T类型的大小
		}

		new (obj) T; // 通过定位new调用构造函数进行初始化

		return obj;
	}

	void Delete(T *obj) // 回收还回来的小空间
	{
		// 显示调用析构函数进行清理工作
		obj->~T();

		// 头插
		*(void **)obj = _freelist; // 新块指向旧块(或空)
		_freelist = obj;		   // 头指针指向新块
	}

private:
	char *_memory = nullptr;   // 指向内存块的指针
	size_t _remanentBytes = 0; // 大块内存在切分过程中的剩余字节数
	void *_freelist = nullptr; // 自由链表，用来连接归还的空闲空间
};

namespace lockfree
{
	template <class T>
	class ObjectPool
	{
	public:
		static const int objsize = sizeof(T) < sizeof(intptr_t) ? sizeof(intptr_t) : sizeof(T);
		static const int64_t aba_inc = 0x0001000000000000LL;  // ABA 计数每次需要增加的值
		static const int64_t aba_mark = 0xFFFF000000000000LL; // ABA Mark
		static const int64_t ptr_mark = 0x0000FFFFFFFFFFFFLL; // 指针 Mark

	public:
		ObjectPool() : _free_head(0), _memory(nullptr), _remanentBytes(0) {}

		~ObjectPool()
		{
			// 清理所有分配的内存块
			// 注意：这里简化处理，实际应该追踪所有分配的块
		}

		// 申请一个T类型大小的空间（无锁版本）
		T *New()
		{
			T *obj = nullptr;

			// 尝试从 freelist 中获取（无锁操作）
			while (true)
			{
				int64_t old_head = _free_head.load(std::memory_order_acquire);
				
				if (old_head == 0)
				{
					// freelist 为空，需要分配新内存
					break;
				}

				// 解析指针和ABA计数
				void *ptr = (void *)(old_head & ptr_mark);
				
				// 安全检查：确保指针有效
				if (ptr == nullptr)
				{
					break;
				}
				
				int64_t aba = old_head & aba_mark;

				// 获取next指针（这里存在竞态窗口，但ABA计数器会保护）
				void *next = *(void **)ptr;
				int64_t new_head = ((int64_t)next & ptr_mark) | ((aba + aba_inc) & aba_mark);

				// CAS 操作：如果 _free_head 还是 old_head，则更新为 new_head
				if (_free_head.compare_exchange_weak(old_head, new_head,
													  std::memory_order_release,
													  std::memory_order_acquire))
				{
					obj = (T *)ptr;
					break;
				}
				// CAS 失败，继续循环重试
			}

			// freelist 为空，从大块内存中分配
			if (obj == nullptr)
			{
				obj = (T *)allocate_from_chunk();
			}

			if (obj == nullptr)
			{
				throw std::bad_alloc();
			}

			// 调用构造函数
			new (obj) T;

			return obj;
		}

		// 回收空间（无锁版本）
		void Delete(T *obj)
		{
			if (obj == nullptr)
				return;

			// 显示调用析构函数
			obj->~T();

			// 无锁头插到 freelist
			while (true)
			{
				int64_t old_head = _free_head.load(std::memory_order_acquire);
				void *old_ptr = (void *)(old_head & ptr_mark);
				int64_t aba = old_head & aba_mark;

				// 将 obj 的前sizeof(void*)字节设置为指向旧的头节点
				*(void **)obj = old_ptr;

				// 构造新的 head（包含新指针和递增的ABA计数）
				int64_t new_head = ((int64_t)obj & ptr_mark) | ((aba + aba_inc) & aba_mark);

				// CAS 操作
				if (_free_head.compare_exchange_weak(old_head, new_head,
													  std::memory_order_release,
													  std::memory_order_acquire))
				{
					break;
				}
				// CAS 失败，继续循环重试
			}
		}

	private:
		// 从大块内存中分配（这里仍需要一些同步，使用轻量级自旋锁）
		void *allocate_from_chunk()
		{
			std::lock_guard<std::mutex> lock(_chunk_mtx);

			if (_remanentBytes < objsize)
			{
				size_t alloc_size;
				if (sizeof(T) > 128 * 1024)
				{
					alloc_size = sizeof(T);
				}
				else
				{
					alloc_size = 128 * 1024;
				}

				size_t npage = (alloc_size + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
				_memory = (char *)SystemAlloc(npage);

				if (_memory == nullptr)
				{
					return nullptr;
				}

				_remanentBytes = npage << PAGE_SHIFT;
			}

			void *obj = _memory;
			_memory += objsize;
			_remanentBytes -= objsize;

			return obj;
		}

	private:
		std::atomic<int64_t> _free_head; // 自由链表头（包含指针和ABA计数）
		
		// 用于chunk分配的成员（这部分仍需要轻量级锁）
		std::mutex _chunk_mtx;
		char *_memory;
		size_t _remanentBytes;
	};

} // namespace lockfree