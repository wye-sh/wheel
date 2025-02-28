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
- namespace wheel
  - [struct wheel::exception](#struct-wheelexception)
  - [struct wheel::wrong_type](#struct-wheelwrong_type)
  - [struct wheel::wrong_arguments](#struct-wheelwrong_arguments)
  - [struct wheel::no_such_event](#struct-wheelno_such_event)
  - [struct wheel::emitter](#struct-wheelemitter)
    - [struct emitter::event](#struct-emitterevent)
      - [using event::weight](#using-eventweight)
      - [using event::on_function](#using-eventon_function)
      - [event::get_meta()](#eventget_meta)
      - [event::set_meta()](#eventset_meta)
      - [event::is_meta_of()](#eventis_meta_of)
      - [event::meta_accepts()](#eventmeta_accepts)
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
  - [wheel::demangle_rcv()](#wheeldemangle_rcv)
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

### event::get_meta()
```cpp
template<typename T>
T &get_meta (handle &Handle);
```
Retrieves metadata object in specified `tuple` type.

#### Template Parameters
- `T`: Type the metadata is in, which must always include `tuple`.

#### Parameters
- `Handle`: That refers to the callback metadata will be retrieved from.

#### Returns
Metadata associated with the specified handle cast to the specified type.

#### Throws
`wrong_type` if `T` does not match the underlying metadata.

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
template<typename T>
bool is_meta_of (handle &Handle);
```
Checks what type a metadata object is.

#### Template Parameters
- `T`: Type to check if the metadata is of, and this will always be a `tuple` type.

#### Parameters
- `Handle`: Refers to the callback that owns the metadata to be checked.

#### Returns
`true` if it is of type `T`, and `false` if it is not.

##

### event::meta_accepts()
```cpp
template<typename... Args>
event &meta_accepts ();
```
When this is set, only the mentioned meta types are accepted when added by the user through the operator(). If the meta data type is not one of the ones specified here, operator() will henceforth throw a wrong_type exception.

#### Template Parameters
- `Args`: List of tuples the meta data are permitted to be of.

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

### wheel::demangle_rcv()
```cpp
template<typename T>
string demangle_rcv ();
```
Version of demangle that maintains reference-const-volatile markers.

##

### with()
```cpp
#define with(Struct, ...)
```
A way to initialize an inline struct with some starting values.

#### Parameters
- `Struct`: Curly-brace-enclosed struct body.
- `...`: Initial values of the struct fields.
