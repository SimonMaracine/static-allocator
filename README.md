# static-allocator

A global static stateless allocator. Header only. It is well suited for frequently allocated objects that have similar
known sizes and when it is known an upper limit of the maximum amount of objects allocated at once.

Simply copy the header file and integrate it into your build system however you want. Note the license.

```cpp
#include <list>

#include "allocator.hpp"

using MyObject1Storage = allocator::StaticAllocatorStorage<32, 8, 4>;
using MyObject2Storage = allocator::StaticAllocatorStorage<64, 24, 8>;

struct MyObject1 : allocator::StaticAllocated<MyObject1, MyObject1Storage> {
    int a, b;
};

struct MyObject2 {
    int a, b;
};

int main() {
    MyObject1* obj1 {new MyObject1};
    delete obj1;

    std::list<MyObject2, allocator::StaticAllocator<MyObject2, MyObject2Storage>> obj2;
}
```

Define `ALLOCATOR_THROW_ON_FAILURE` in order to make the allocator throw a runtime exception on failure.
