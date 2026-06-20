#pragma once
#include <new>       
#include <utility>   
#include<sys/mman.h>
#include<unistd.h>


template<typename T , size_t Capacity>

class ObjectPool{
private:
    uint8_t* memory_block;
    size_t total_bytes;
    T** free_list;
    size_t free_index;
public:
    // Constructor
    ObjectPool()
    {
        total_bytes = Capacity*sizeof(T);
        void* raw_mem = mmap
        (
            nullptr, 
            total_bytes, 
            PROT_READ | PROT_WRITE,      
            MAP_PRIVATE | MAP_ANONYMOUS,  
            -1, 
            0
        );
        if (raw_mem == MAP_FAILED) 
        {
            throw std::bad_alloc();
        }
        memory_block = reinterpret_cast<uint8_t*>(raw_mem);

        // Pre-Faulting
        size_t page_size = sysconf(_SC_PAGESIZE); 
        for (size_t i = 0; i < total_bytes; i += page_size) {
            memory_block[i] = 0;
        }
        if (total_bytes > 0) {
            memory_block[total_bytes - 1] = 0;
        }
        free_list = new T*[Capacity];
        for(size_t i=0;i<Capacity;++i)
        {
            free_list[i] = reinterpret_cast<T*>(memory_block + i*sizeof(T));
        }
        free_index=Capacity;
    }    

    template <typename... Args>
    T* allocate(Args&&... args)
    {
        if(free_index==0) return nullptr;
        --free_index;
        T* raw_addr = free_list[free_index];
        return new (raw_addr) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr)
    {
        if(!ptr) return;
        ptr->~T(); //calls destructor without freeing the memory
        free_list[free_index] = ptr;
        ++free_index;

    }

    ~ObjectPool()
    {
        delete[] free_list;
        munmap(memory_block,total_bytes);
    }
};
