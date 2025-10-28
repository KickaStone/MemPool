// single level

#include <cstdint>
#include <cstring>
#include <assert.h>
#include "ObjectPool.h"
#include "Common.h"

using namespace std;
typedef uintptr_t Number;

/**
 * 假设32位平台，一页8KB算，页内偏移就是13位，页号是19位，即32-PAGE_SHIFT, PAGE_SHIFT = 13, BITS=19
 * 基数树是为了处理pageCache的地址映射，存储的是Span*类型，也就是需要2^19个Span*，空间需求是2^19 * 4B = 2^21B 即2MB空间 是可以接收的
 * 
 * 如果是64位平台，一页8KB算，页内偏移就是13位，页号是51位，即64-PAGE_SHIFT, PAGE_SHIFT = 13, BITS=51
 * 这里一级基数树需要开2^51 * 4B = 2^53B = 4PB空间，这个空间需求太大了，不能接收
 * 
 * 如果使用两极
 * 
 */

/**
 * Single level radix tree, store (Number, void*) pairs
 * Just use a pointer array to store the pointers. 
 * If we want to store a number of uint_8, BITS = 8, LENGTH = 256
 */
// template <int BITS>
// class Leaf{
// private:
//     static const int LENGTH = 1 << BITS; // 2^BITS, 要开辟的数组空间
//     void** _array;

// public:
//     explicit Leaf(){
//         _array = (void**)SystemAlloc(LENGTH * sizeof(void*));
//         memset(_array, 0, LENGTH * sizeof(void*));
//     }

//     ~Leaf(){
//         delete[] _array;
//     }

//     void *get(Number k){
//         if((k >> BITS) > 0){
//             return nullptr;
//         }
//         return _array[k];
//     }

//     void set(Number k, void *v){
//         _array[k] = v;
//     }
// };

// Two-level radix tree
template <int BITS>
class TCMalloc_PageMap2 {
private:
	// Put 32 entries in the root and (2^BITS)/32 entries in each leaf.
	static const int ROOT_BITS = 5; // 32位下前5位搞一个第一层的数组
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS; // 32位下后14位搞成第二层的数组
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Leaf node
	struct Leaf { // 叶子就是后14位的数组
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // 根就是前5位的数组
public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap2() { // 直接把所有的空间都开好
		memset(root_, 0, sizeof(root_));
		PreallocateMoreMemory(); // 直接开2M的span*全开出来
	}

	void* get(Number k) const {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}

	// 确保从start开始往后的n页空间开好了
	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// Check for overflow
			if (i1 >= ROOT_LENGTH)
				return false;

			// 如果没开好就开空间
			if (root_[i1] == NULL) {
				static ObjectPool<Leaf>	leafPool;
				// leafPool._poolMtx.lock();
				Leaf* leaf = (Leaf*)leafPool.New();
				// leafPool._poolMtx.unlock();
				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	// 提前开好空间，这里就把2M的直接开好
	void PreallocateMoreMemory() {
		// Allocate enough to keep track of all possible pages
		Ensure(0, 1 << BITS);
	}
};


template <int BITS>
class TCMalloc_PageMap3 {
private:
	// 64bit 
	static const int ROOT_BITS = 12; 
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = 20; 
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	static const int MIDDLE_BITS = BITS - ROOT_BITS - LEAF_BITS;
	static const int MIDDLE_LENGTH = 1 << MIDDLE_BITS;

	// Leaf node
	struct Leaf { // 叶子就是后14位的数组
		void* values[LEAF_LENGTH];
	};

	struct Middle {
		Leaf* values[MIDDLE_LENGTH];
	};
	
	Middle* root_[ROOT_LENGTH]; // 根就是前5位的数组
public:
	typedef uintptr_t Number;

	explicit TCMalloc_PageMap3() { // 直接把所有的空间都开好
		memset(root_, 0, sizeof(root_));
	}

	void* get(Number k) const {
		const Number i1 = k >> (LEAF_BITS + MIDDLE_BITS);
		const Number i2 = k >> LEAF_BITS & (MIDDLE_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		if ((k >> (LEAF_BITS + MIDDLE_BITS)) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		if(root_[i1]->values[i2] == NULL){
			return NULL;
		} 
		if(root_[i1]->values[i2]->values[i3] == NULL){
			return NULL;
		}
		return root_[i1]->values[i2]->values[i3];
	}

	void set(Number k, void* v) {
		if(k >> BITS > 0){
			fprintf(stderr, "TCMalloc_PageMap3::set: %lu out of range\n", k);
			abort();
			return;
		}

		const Number i1 = k >> (LEAF_BITS + MIDDLE_BITS); // root
		const Number i2 = k >> LEAF_BITS & (MIDDLE_LENGTH - 1); // middle
		const Number i3 = k & (LEAF_LENGTH - 1); // leaf
		
		if(root_[i1] == NULL){
			static ObjectPool<Middle> middlePool;
			// middlePool._poolMtx.lock();
			Middle* middle = (Middle*)middlePool.New();
			// middlePool._poolMtx.unlock();
			memset(middle, 0, sizeof(*middle));
			root_[i1] = middle;
		}
		
		if(root_[i1]->values[i2] == NULL){
			static ObjectPool<Leaf>	leafPool;
			// leafPool._poolMtx.lock();
			Leaf* leaf = (Leaf*)leafPool.New();
			// leafPool._poolMtx.unlock();
			memset(leaf, 0, sizeof(*leaf));
			root_[i1]->values[i2] = leaf;
		}
		root_[i1]->values[i2]->values[i3] = v;
	}
};
