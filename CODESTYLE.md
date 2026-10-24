# Code style

This document describes the way this project uses C.  

1. All allocations must be done through context. This allows for centralized error handling of allocation failures.  
Possible `NULL` as allocation result must be handled, and tested.  
The only exception is test allocations.  

2. This project imports `.c` files directly, without using `.h` files (for its internal modules, at least).  
This allows for less repetition.  
It is to stay like this until some problem is discovered about having only one compilation unit.

3. `snake_case` is used for all identifiers. `UPPER_SNAKE_CASE` is used for constants (including enum values).  

4. This project is written in OOP style. However, C doesn't support OOP; so instead, every "class" is a declaration of a structure, with `classname_init` and `classname_deinit` "methods".  
Don't allocate memory for object itself in `classname_init` function; accept a reference to an object and assume it's not null. This allows for more flexible memory control - for example, embedding the object within another object, which is great for cache locality and reducing memory indirection.  
Zero-fill object at the start of `classname_init`. Zero-fill object at the end of `classname_deinit`. This allows for more predictable inits and deinits, especially useful in case of allocation failure recovery - sometimes such recovery can cause double-frees; after failed init object must be left in initial state.  
Avoid accessing "class" fields directly outside of the file that declares it; if you need to - create inlineable "method".  

5. Avoid preprocessor as much as possible.  
Declare constants using `constexpr`, as it allows for explicit typing.  
Inline functions with `INLINE` macro, not by making macro of those functions you want to inline.  
Notable exceptions: `#include`, `#pragma once`.  

6. Avoid platform-specific and compiler-specific code as much as possible.  
If you have to have it - support clang and MSVC.  

7. Don't use enums, as they are implicitly-typed.  
If you need to declare enum - declare type separately with `typedef some_sort_of_int my_enum_name`, and values separately as `constexpr`s.  

8. Don't use unsigned int types unless you absolutely need to use full range of its bits without surprises about the sign.  
Unsigned types tend to introduce undefined behavior if intermediate calculation value can go below zero.  
This includes `size_t`. Prefer `int64` or `int32`.  

9. This project doesn't use `_t` suffix for types.  
Rationale for having suffixes is "you can tell at a glance if it's a type or a value", but with modern IDEs you can tell even without suffix; instead, having suffixes is just a code litter; we should not have it.  
If you want to use a type from standard library that has the suffix - consider declaring a `typedef sometype_t sometype` in `commons.c`.  
