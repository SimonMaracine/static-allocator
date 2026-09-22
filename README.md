# static-allocator

A global static stateless allocator. Header only. It is well suited for frequently allocated objects that have similar
known sizes and when it is known an upper limit of the maximum amount of objects allocated at once.

Simply copy the header file and integrate it into your build system however you want. Note the license.

```cpp
#include <list>

#include "static_allocator/static_allocator.hpp"

using MyObject1Storage = static_allocator::StaticAllocatorStorage<32, 8, 4, false, false>;
using MyObject2Storage = static_allocator::StaticAllocatorStorage<64, 24, 8>;

struct MyObject1 : static_allocator::StaticAllocated<MyObject1, MyObject1Storage> {
    int a {};
    int b {};
};

struct MyObject2 {
    int a {};
    int b {};
};

int main() {
    MyObject1* obj1 = new MyObject1;
    delete obj1;

    std::list<MyObject2, static_allocator::StaticAllocator<MyObject2, MyObject2Storage>> obj2;
    obj2.emplace_back();
}
```
