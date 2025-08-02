# WHEEL

WHEEL (Wye-Homologous Event Emitter Library) is a lightweight, type-safe C++
event system designed to provide flexible event handling with minimal overhead
and thread-safe operation.

## Table of Contents
- [Overview](#overview)
- [Installation](#installation)
  - [Options](#options)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Documentation](#documentation)

## Overview

Built on an emitter-based architecture, the library allows you to register,
extend, manage, and emit strongly-typed events. Each event in an emitter can
have multiple callbacks, which can be ordered by weight to control execution
priority. Events support metadata attachment to callbacks, enabling custom data
to flow between event creators and handlers. Additionally, the interceptor
system lets event creators customize how callbacks are executed, providing even
finer control over event handling.

## Installation

If you're using CMake, add the following to your `CMakeLists.txt`:
```cmake
include(FetchContent)
FetchContent_Declare(
  wheel
  GIT_REPOSITORY https://github.com/wye-sh/wheel
  GIT_TAG v2.0.2 # (latest version)
)
FetchContent_MakeAvailable(wheel);

# Link against wheel in your project
target_link_libraries(<your_target> PRIVATE wheel)
```

Alternatively, clone the repository along with its dependencies (check the
`CMakeLists.txt` for details) and place the headers on your include path.

### Options

You have the opportunity for further customization before your call to
`FetchContent_MakeAvailable()`.
```cmake
# If on, enables faster pointer address comparison for `type_info` objects,
# which assumes that only one library instance is linked
set(RTTI_POINTER_COMPARISON_MODE <OFF|ON>) # Default: ON
```
These dependencies can also be configured:
- [APORT](https://github.com/wye-sh/aport)

## Quick Start

To get started:

```cpp
// Create emitter that can store multiple events
wheel::emitter Events;

// Alternatively, specify a default type for automatic event creation
// (autovivification) when one that does not exist is accessed.
// # wheel::emitter<void (int)> Events;

// Add "key-down" event that takes a `char32_t`
Events.create<void (char32_t)>("key-down");

// Insert a handler
Events["key-down"] = [](char32_t Codepoint) {
  printf("[1]: Key Down: %c", (char) Codepoint);
};

// Save handle to most recently added handler
wheel::handle Handle = *Events;

// Insert a weighted handler (default weight is zero)
Events["key-down"].insert([](char32_t Codepoint) {
  printf("[0]: Key Down: %c", (char) Codepoint);
}, 1);

// Call all "key-down" handlers
Events["key-down"].emit((char32_t) 'p');
// Output:
// $ [0] Key Down: p
// $ [1] Key Down: p

// Remove handler using saved handle. The handle we retrieved earlier can be
// used to remove the handler it references from anywhere, including from
// inside itself.
Events["key-down"].remove(Handle);

// Call "key-down" handlers again
Events["key-down"].emit((char32_t) 'r');
// Output:
// $ [0] Key Down: r
```

## Examples

More advanced examples:

### Self-Destruct Event

```cpp
// Create emitter
wheel::emitter Events;

// Add event that removes handlers as they are added
Events
  .create<void ()>("self-destruct")
  .set_on_insert([&](wheel::handle &Handle) {
    Events["self-destruct"].remove(Handle);
  })
  .set_on_remove([](wheel::handle &Handle) {
    printf("Boom!\n");
  });

// Insert handlers
Events["self-destruct"] = []() { /* ... */ };
// Output:
// $ Boom!
Events["self-destruct"] = []() { /* ... */ };
// Output:
// $ Boom!

// emit() does nothing because there are no handlers
Events["self-destruct"].emit();
```

### Repeater Event

```cpp
// Create emitter
wheel::emitter Events;

// Add event where handlers run a configurable number of times
Events
  .create<void (int)>("repeater")
  .meta_accepts<int>()
  .set_interceptor
    ([](wheel::handle &Handle, function<void (int)> Target, int Unused) {
      auto [ N ] = Events["repeater"].get_meta<int>();
      for (int I = 0; I < N; ++ I)
        Target(I);
    });

// Insert a handler whose body will be repeated five times
Events["repeater"](5) = [](int I) {
  printf("One: %d/5\n", I + 1);
};

// Insert a handler whose body will be repeated two times
Events["repeater"](2) = [](int I) {
  printf("Two: %d/2\n", I + 1);
};

// Call all "repeater" handlers (`int` argument is still required to conform
// with the event type)
Events["repeater"].emit(0);
// Output:
// $ One: 1/5
// $ One: 2/5
// $ One: 3/5
// $ One: 4/5
// $ One: 5/5
// $ Two: 1/2
// $ Two: 2/2
```

### Tick Event

```cpp
// Implement limiter constraining the number of calls to N number of calls over
// a specified time period. We assume that now() is a function that returns the
// current time as a `double` in seconds.
struct limiter {
  double TimePerCall;
  double PreviousTime;
  
  limiter (int NCalls, int TimePeriod)
    : TimePerCall(TimePeriod / NCalls),
      PreviousTime(now()) {
    // ...
  } // limiter()

  int n () {
    double Time = now();
    double DeltaTime = Time - PreviousTime;
    int N = floor(DeltaTime / TimePerCall);
    PreviousTime += N * TimePerCall;
    return N;
  } // n()
}; // struct limiter

// Create emitter
wheel::emitter Events;

// Add event that is called N times over a specified time period
Events.create<void ()>("tick");

// Retrieve reference to "tick" event for faster access
wheel::emitter<>::event &TickEvent = &hooks["tick"];

// Implement "tick" event
TickEvent
  .meta_accepts<int, double>()
  .set_on_insert([&](wheel::handle &Handle) {
    auto [ N, TimePeriod ] = TickEvent.get_meta<int, double>(Handle);
    TickEvent.set_meta(Handle,limiter_t(N, TimePeriod));
  })
  .set_interceptor([&](wheel::handle &Handle, function<void ()> Target) {
    auto &[ Limiter ] = TickEvent.get_meta<limiter>(Handle);
    int N = Limiter.n();
    for (int I = 0; I < N; ++ I)
      Target();
  });

// Add handlers
TickEvent(60, 1.0) = []() { /* Will be called 60 times per second */ };
TickEvent(30, 2.0) = []() { /* Will be called 15 times per second */ };

// Main loop. We assume the existance of an `IsRunning` boolean that is `true`
// so long as the application is running.
while (IsRunning) {
  TickEvent.emit();
}
```

## Documentation
<template:toc>

<template:body>
