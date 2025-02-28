# WHEEL

WHEEL (Wye-Homologous Event Emitter Library) is a lightweight, type-safe C++
event system designed to provide flexible event handling with minimal overhead
and thread-safe operation.

## Table of Contents
- [Overview](#overview)
- [Installation](#installation)
  - [Options](#options)
- [Quick Start](#quick-start)
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
  GIT_TAG v1.0.1 # (latest version)
)
FetchContent_MakeAvailable(wheel);

# Link against wheel in your project
target_link_libraries(<your_target> PRIVATE wheel)
```

Alternatively, clone the repository along with its dependencies (check the
`CMakeLists.txt` for details) and place the headers on your include path.

### Options

Tailor the build to your needs by adjusting the available options. WHEEL itself
does not have any options, but its dependencies can be configured:
- [APORT](https://github.com/wye-sh/aport)

## Quick Start

To get started:

```cpp
// Create an emitter that can hold events of any type
wheel::emitter Events;

// Alternatively, specify a default type for automatic event creation
// (autovivification) when accessing non-existent events
// # wheel::emitter<void(int)> Events;

// Create a simple event that takes no parameters
Events.create<void()>("window-entered");

// Create an event with parameters
Events.create<void(int, action)>("key-press");

// Subscribe to events using lambda functions
Events["window-entered"] = []() { printf("First!\n"); };

// Store a handle to remove the callback later
wheel::handle Handle = *Events;
// You can remove it later as follows using the handle:
// # Events["window-entered"].remove(Handle);

// Subscribe with a callback that accepts parameters
Events["key-press"] = [](int Key, action Action) {
if (Action == action::down)
  printf("Pressed down key `%d`.\n", Key);
};

// Add multiple callbacks to the same event
Events["window-entered"] = []() { printf("Second!\n"); };

// Control execution order with weighted callbacks (higher weight = heigher
// execution priority)
Events["window-entered"].insert([]() {
  printf("I run before the one who says \"First!\"\n");
}, 1);

// Trigger events, executing their callbacks
Events["key-press"].emit(12, action::down);
// Output:
// $ Pressed down key `12`.

Events["window-entered"].emit();
// Output:
// $ I run before the one who says "First!"
// $ First!
// $ Second!
```

To set up custom events:

```cpp
// Create an event with metadata and custom execution
Events.create<void(int)>("repeater")
  .meta_accepts<tuple<int>>()
  .set_interceptor([](wheel::handle &Handle, function<void(int)> Target, int _) {
    auto [ N ] = Events["repeater"].get_meta<tuple<int>>(Handle);
	for (int I = 0; I < N; ++ I)
	  Target(I);
  });

// Attach callbacks with metadata (5 and 2 repetitions)
Events["repeater"](5) = [](int I) { printf("X: [%d]\n", I); };
Events["repeater"](2) = [](int I) { printf("Y: [%d]\n", I); };

// Metadata flow: Users set metadata with the call operator (shown above),
// while event creators can access or modify it via the get_meta() and
// set_meta() methods using a valid wheel::handle.

// Metadata-driven emit
Events["repeater"].emit(0);
// Output:
// $ X: [0]
// $ X: [1]
// $ X: [2]
// $ X: [3]
// $ X: [4]
// $ Y: [0]
// $ Y: [1]

// Create an event using callback lifecycle hooks
Events.create<void()>("self-destruct")
  .set_on_insert([&](wheel::handle &Handle) {
    // Remove callback directly when it was added
    Events["self-destruct"].remove(Handle);
  })
  .set_on_remove([&](wheel::handle &Handle) {
    printf("Boom!\n");
  });

// Add callback and emit()
Events["self-destruct"] = []() { printf("This will not run.\n"); };
Events["self-destruct"].emit();
// Output:
// $ Boom!
// The "self-destruct" event will always be zero-length

// Disable custom behaviours
Events["repeater"].unset_interceptor();
Events["self-destruct"].unset_on_insert();
Events["self-destruct"].unset_on_remove();
// "repeater" and "self-destruct" will now behave normally
```

## Documentation
<template:toc>

<template:body>
