# AVS Native — C++ Style Guide

## Braces

Allman style: opening brace on its own line.

```cpp
void foo()
{
    if (cond)
    {
        doSomething();
    }
}
```

Single-line `if` bodies may omit braces but NOT `for` or `while`. Always put a linebreak before the body of the statement.

```cpp
if (!ok) 
    return false;
```

```cpp
// No
for (int32 ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
    CreateSlot(ScratchBuffers[ScratchIdx], Width, Height);

// Yes
for (int32 ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
{
    CreateSlot(ScratchBuffers[ScratchIdx], Width, Height);
}
```

## Indentation

4 spaces. No tabs.

## Naming

```
ClassName          PascalCase
MethodName         PascalCase
localVariable      PascalCase
memberVariable     PascalCase
CONSTANT_VALUE     upper snake (compile-time constants / macros only)
```

## No Alignment

Don't pad with spaces to align across lines.

```cpp
// No
int32 x    = 1;
float yVal = 2.0f;

// Yes
int32 x = 1;
float yVal = 2.0f;
```

## Pointers and References

Attached to the type, not the name.

```cpp
void* ptr;
const std::string& name;
```

## Loops

When iterating over a `std::vector` or other range, use a range-based for loop.

```cpp
// No
for (int32 I = 0; I < Entries.size(); I++)
{
    Entries[I].Render();
}

// Yes
for (auto& Entry : Entries)
{
    Entry.Render();
}
```

When an index is genuinely needed, give it a descriptive name — not `I`, `J`, or `K`.

```cpp
// No
for (int32 I = 0; I < SCRATCH_BUFFER_COUNT; I++)
{
    CreateSlot(ScratchBuffers[I], Width, Height);
}

// Yes
for (int32 ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
{
    CreateSlot(ScratchBuffers[ScratchIdx], Width, Height);
}
```

## Comments

Only when the **why** is non-obvious. No restating what the code does.

## Includes

Order: own header, project headers, third-party, standard library. One blank line between groups.

```cpp
#include "engine/Engine.h"

#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>
```
