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
  GIT_TAG v2.0.1 # (latest version)
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
- namespace wheel
  - [struct wheel::exception](#struct-wheelexception)
  - [struct wheel::wrong_type](#struct-wheelwrong_type)
  - [struct wheel::wrong_arguments](#struct-wheelwrong_arguments)
  - [struct wheel::no_such_event](#struct-wheelno_such_event)
  - [struct wheel::emitter](#struct-wheelemitter)
    - [struct emitter::event](#struct-emitterevent)
      - [using event::weight](#using-eventweight)
      - [using event::on_function](#using-eventon_function)
      - [event::get_function()](#eventget_function)
      - [event::get_meta()](#eventget_meta)
      - [event::set_meta()](#eventset_meta)
      - [event::is_meta_of()](#eventis_meta_of)
      - [event::meta_accepts()](#eventmeta_accepts)
      - [event::meta_accepts_anything()](#eventmeta_accepts_anything)
      - [event::set_on_insert()](#eventset_on_insert)
      - [event::unset_on_insert()](#eventunset_on_insert)
      - [event::set_on_remove()](#eventset_on_remove)
      - [event::unset_on_remove()](#eventunset_on_remove)
      - [event::set_interceptor()](#eventset_interceptor)
      - [event::unset_interceptor()](#eventunset_interceptor)
      - [event::operator()()](#eventoperator)
      - [event::insert()](#eventinsert)
      - [event::operator=()](#eventoperator-1)
      - [event::operator*()](#eventoperator-2)
      - [event::remove()](#eventremove)
      - [event::emit()](#eventemit)
      - [event::clear()](#eventclear)
      - [event::length()](#eventlength)
      - [event::empty()](#eventempty)
    - [emitter::operator*()](#emitteroperator)
    - [emitter::create()](#emittercreate)
    - [emitter::retire()](#emitterretire)
    - [emitter::contains()](#emittercontains)
    - [emitter::operator\[\]()](#emitteroperator-1)
  - [using wheel::handle](#using-wheelhandle)
- [with()](#with)

##

### struct wheel::exception
```cpp
struct exception;
```
This struct serves as the root of the WHEEL exception hierarchy, allowing applications to catch all WHEEL-specific exceptions through a single catch block. All custom exceptions in the WHEEL framework extend this struct.

##

### struct wheel::wrong_type
```cpp
struct wrong_type;
```
Thrown when a function of incorrect type is used. Includes details about the actual type that was provided and the accepted types that should have been used instead.

##

### struct wheel::wrong_arguments
```cpp
struct wrong_arguments;
```
Thrown when emit() receives arguments contrasting the underlying accepted function type. This could include a mismatch in arity or argument types. This type must be created with the static factory create() method.

##

### struct wheel::no_such_event
```cpp
struct no_such_event;
```
Thrown if no event by the given name exists.

##

### struct wheel::emitter
```cpp
template<typename DefaultEventType = void>
struct emitter;
```
Stores multiple event types that are accessible by name, where new such types can be registered, have callbacks added to them, and executed.

#### Template Parameters
- `DefaultEventType`: If non-void, will create new event on access to an event that does not exist.

##

### struct emitter::event
`movable`
```cpp
struct event;
```
Configurable event structure enabling callback modification. Events can be configured with before/after hooks that run when callbacks are added or removed, or with an interceptor capable of changing the arguments that are passed through or make the callback run multiple times or not at all.

##

### using event::weight
```cpp
using weight = unsigned short;
```
Specifies the level of precedence a callback takes in the order of execution when the event is emitted with emit().

##

### using event::on_function
```cpp
using on_function = function<void (handle &)>;
```
The function type used for set_on_insert() and set_on_remove(), which define the behaviour that happens before and respectively after a callback function is added or removed.

##

### event::get_function()
```cpp
template<typename T>
function<T> &get_function (handle &Handle);
```
Retrieves function associated with handle.

#### Template Parameters
- `T`: Type the function is in.

#### Parameters
- `Handle`: Handle from which a lambda will be retrieved.

#### Returns
Lambda object associated with hanled.

#### Throws
`wrong_type` if `T` does not match the underlying lambda type.

##

### event::get_meta()
```cpp
template<typename... Args>
tuple<Args...> &get_meta (handle &Handle);
```
Retrieves metadata tuple with specified types.

#### Template Parameters
- `Args`: Types that the metadata tuple stores.

#### Parameters
- `Handle`: Refers to the callback that metadata will be retrieved from.

#### Returns
Metadata tuple associated with the specified handle.

#### Throws
`wrong_type` if provided `Args` do not match the underlying types of the metadata for `Handle`.

##

### event::set_meta()
```cpp
template<typename... Args>
void set_meta (handle &Handle, Args... Arguments);
```
Regenerate the metadata `tuple` with new values once there is a handle with associated metadata. To generate metadata for the right thereafter inserted callback, instead use operator()().

#### Parameters
- `Handle`: Refers to the callback for which metadata should be regenerated.

#### Returns
Arguments that the new metadata tuple should be made from.

##

### event::is_meta_of()
```cpp
template<typename... Args>
bool is_meta_of (handle &Handle);
```
Checks if the types of the metadata object associated with `Handle` is the specified arguments `Args`.

#### Template Parameters
- `Args`: Types we are comparing with the metadata type to see if they match.

#### Parameters
- `Handle`: Handle associated with a callback function.

#### Returns
`true` if the types of metadata match `Args` and `false` otherwise.

##

### event::meta_accepts()
```cpp
template<typename... Args>
event &meta_accepts ();
```
Call this once for each type the metadata can have. If the user has not specified metadata using operator()() that meets the criteria, the insert() and operator=() functions will throw an exception.

#### Template Parameters
- `Args`: Types of a single accepted metadata type.

#### Returns
`*this` for chaining.

##

### event::meta_accepts_anything()
```cpp
event &meta_accepts_anything ();
```
This only needs to be called if meta_accepts() has already been called, and the desired behaviour is to undo all hitherto specified accepted meta types, making operator() revert to accept any metadata types whatever.

#### Returns
`*this` for chaining.

##

### event::set_on_insert()
```cpp
template<typename F>
requires same_as<typename function_traits<F>::function, on_function>
event &set_on_insert (F Function);
```
Sets a function that will run whenever a callback function is inserted into the event.

#### Parameters
- `Function`: Function to be run when a callback is inserted.

#### Returns
`*this` for chaining.

##

### event::unset_on_insert()
```cpp
event &unset_on_insert ();
```
Unsets the function previously set through set_on_insert().

#### Returns
`*this` for chaining.

##

### event::set_on_remove()
```cpp
template<typename F>
requires same_as<typename function_traits<F>::function, on_function>
event &set_on_remove (F Function);
```
Sets a function that will run right before a callback function is removed from the event.

#### Parameters
- `Function`: Function to be run when a callback is removed.

#### Returns
`*this` for chaining.

##

### event::unset_on_remove()
```cpp
event &unset_on_remove ();
```
Unsets the function previously set through set_on_remove().

#### Returns
`*this` for chaining.

##

### event::set_interceptor()
```cpp
template<typename F>
event &set_interceptor (F Function);
```
Sets a function that will wrap every callback function that is inserted past this point. The handler is inputted as a parameter to the interceptor, so that the interceptor can modify the behaviour surrounding the handler as it sees fit.

#### Parameters
- `Function`: Interceptor that wraps handlers inserted past this point.

#### Returns
`*this` for chaining.

#### Throws
`wrong_type` if `F` is not of a valid interceptor function type.

##

### event::unset_interceptor()
```cpp
event &unset_interceptor ();
```
Unsets the function previously set through set_interceptor().

#### Returns
`*this` for chaining.

##

### event::operator()()
```cpp
template<typename... Args>
event &operator() (Args... Arguments);
```
Generates a metadata `tuple` that can be used to communicate with the event creator. The generated tuple will be attached to the callback that is inserted next.

#### Parameters
- `Arguments`: Arguments the metadata `tuple` should be created from.

#### Returns
`*this` for chaining.

##

### event::insert()
`thread-safe`
```cpp
template<typename F, typename = enable_if_callable<F>>
event &insert (const F &Function, weight Weight = 0);
```
Inserts a callback to be outputted when emit() is called by the event creator.

#### Parameters
- `Function`: Handler that will become part of the event, that will run whenever emit() is called.
- `Weight`: Priority level for `Function` in the handler list.

#### Returns
`*this` for chaining.

#### Throws
- `wrong_type` if `F` is not of the accepted function type.
- `wrong_type` if the user-provided metadata is of a type that is not accepted.

##

### event::operator=()
```cpp
template<typename F, typename = enable_if_callable<F>>
void operator= (const F &Function);
```
Syntactic sugar for callback insertion. Same as insert() but without the possibility of specifying a weight for the callback in the list of handlers.

#### Parameters
- `Function`: Handler that will become part of the event, that will run whenever emit() is called.

##

### event::operator*()
```cpp
handle operator* ();
```
Retrieves a handle to the last added callback. Following the insertion of a callback into the event is the only opportunity to retrieve such a handle.

#### Returns
Handle to the last added callback function.

##

### event::remove()
`thread-safe`
```cpp
event &remove (handle &Handle);
```
Removes a callback function per its handle, which was retrieved by dereferencing `*this` after the handler insertion.

#### Parameters
- `Handle`: Handle associated with a callback function.

#### Returns
`*this` for chaining.

##

### event::emit()
`thread-safe`
```cpp
template<typename... Args>
void emit (Args... Arguments);
```
Runs all callback functions (or handlers) of the event.

#### Parameters
- `Arguments`: Arguments that will be forwarded to all handlers.

#### Throws
`wrong_arguments` if the arguments inputted do not correspond to the event argument types.

##

### event::clear()
`thread-safe`
```cpp
event &clear ();
```
Removes all callback functions from this event.

#### Returns
`*this` for chaining.

##

### event::length()
`thread-safe`
```cpp
size_t length ();
```
The number of callback functions that are managed and run by the event.

#### Returns
Number of functions stored in this event. This includes deferred inserts (callbacks that have not been inserted yet because emit() was running when it was added and it has not finished running).

##

### event::empty()
`thread-safe`
```cpp
bool empty ();
```
Checks if there are any callbacks managed by the event.

#### Returns
`true` if length() returns `0`, and `false` otherwise.

##

### emitter::operator*()
```cpp
handle operator* ();
```
Alias: event::operator*().

##

### emitter::create()
```cpp
template<typename Q>
event &create (string Name);
```
Creates an event by name `Name` that can be configured using the available methods in the returned `event`. Then, emit() can be used to call all the user added callbacks (or handlers).

#### Parameters
- `Name`: Name of the event to be created.

#### Returns
Newly created event that is now tracked by the emitter.

##

### emitter::retire()
```cpp
emitter &retire (string Name);
```
Retires an event, removing it from the managed events.

#### Parameters
- `Name`: Name of the event to be removed.

#### Returns
`*this` for chaining.

##

### emitter::contains()
```cpp
bool contains (string Name);
```
Check if an event exists.

#### Parameters
- `Name`: Name of the event to be checked if it exists.

#### Returns
`true` if an event by name `Name` exists.

##

### emitter::operator\[\]()
```cpp
event &operator[] (string Name);
```
Retrieve an event by name, or insert one if it does not exist before retrieving it if the emitter has its `DefaultEventType` specified.

#### Parameters
- `Name`: Name of the event to be retrieved.

#### Returns
Event by name `Name` if it exists or was implicitly created.

#### Throws
`no_such_event` if no event by name `Name` exists.

##

### using wheel::handle
```cpp
using handle = shared_ptr<int>;
```
The type of a handle used for the removal and otherwise identification of callbacks inside an event.

##

### with()
```cpp
#define with(Struct, ...)
```
A way to initialize an inline struct with some starting values.

#### Parameters
- `Struct`: Curly-brace-enclosed struct body.
- `...`: Initial values of the struct fields.
