//                                  Written by
//                              Alexander Hållenius
//                              Copyright (c) 2025
//                                      ~~~
//     WHEEL (Wye-Homologous Event Emitter Library) is a lightweight, type-
//    safe C++ event system designed to provide flexible event handling with
//                  minimal overhead and thread-safe operation.
//                                      ~~~

#pragma once
#ifndef _WHEEL_CC
#define _WHEEL_CC

#include <aport/aport.h>
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <typeinfo>
#include <cxxabi.h>
#include <optional>
#include <utility>
#include <mutex>

namespace wheel {

using namespace std;

/*
 * demangle()
 *   Converts an environment specific type info string `Name` or type info
 *   `Info` into a human readable format. Returns the human readable
 *   representation as a `string`.
 */
inline string demangle (const char *Name) {
  int Status = -1;
  unique_ptr<char, void(*)(void *)> Resource {
    abi::__cxa_demangle(Name, NULL, NULL, &Status),
    std::free
  };
  return (Status == 0) ? Resource.get() : Name;
} // demangle()
inline string demangle (const type_info &Info) { return demangle(Info.name()); }

/*
 * demangle_rcv()
 *   Version of demangle that maintains reference-const-volatile markers.
 */
template<typename T>
inline string demangle_rcv () {
  using NoRef = remove_reference_t<T>;
  string Base = demangle(typeid(NoRef).name());
  if constexpr (is_const_v<NoRef>)
    Base += " const";
  if constexpr (is_volatile_v<NoRef>)
    Base += " volatile";
  if constexpr (is_lvalue_reference_v<T>)
    Base += "&";
  else if constexpr (is_rvalue_reference_v<T>)
    Base += "&&";
  return Base;
} // demangle_rcv()

/*
 * stringify_parameter_pack()
 *   Will demangle each template argument and generate from them a comma-
 *   separated list string, which is returned.
 */
template<typename... Args>
inline string stringify_parameter_pack () {
  string String;
  size_t Index = 0;
  ((String += ((Index++ == 0) ? "" : ", ") + demangle_rcv<Args>()), ...);
  return String;
} // stringify_parameter_pack()

/*
 * struct parameter_pack_stringifier
 *   In a similar manner to stringify_parameter_pack(), will stringify the par-
 *   ameter types of a function with type `T`. Create the struct by specifying
 *   the type, then retrieve the string using the `stringify` method.
 */
template <typename T>
struct parameter_pack_stringifier;
template <typename R, typename... Args> // Function
struct parameter_pack_stringifier<function<R(Args...)>> {
  /*
   * stringify()
   *   Calls stringify_parameter_pack() under the hood, returning a comma-
   *   separated string of all the type names.
   */
  static string stringify() {
    return stringify_parameter_pack<Args...>();
  }
}; // struct parameter_pack_stringifier
template <typename C, typename R, typename... Args> // Lambda
struct parameter_pack_stringifier<R(C::*)(Args...) const> {
  static string stringify() {
    return stringify_parameter_pack<Args...>();
  }
}; // struct parameter_pack_stringifier

/*
 * struct is_callable
 *   Type trait to check if `T` is callable with any number of arguments that
 *   are arbitrary in type.
 */
template<typename T>
struct is_callable {
private:
  // Test convertible to any type
  struct any { template<class U> operator U(); };
  // Try to call with any arguments
  template<typename X, typename... Args>
  static auto check(X&& XV, Args&&... Arguments) ->
    decltype(XV(Arguments...), true_type());
  // Fallback if call fails
  static false_type check(...);
public:
  static const bool value = decltype
    (check(declval<T>(), any(), any(), any()))::value;
};
template<typename T>
using enable_if_callable = enable_if<is_callable<T>::value>;

/**---------------------------------------------------------------------------
 * struct exception
 *   This struct serves as the root of the WHEEL exception hierarchy, allowing
 *   applications to catch all WHEEL-specific exceptions through a single catch
 *   block. All custom exceptions in the WHEEL framework extend this struct.
 *---------------------------------------------------------------------------**/
struct exception: std::exception {};

/**---------------------------------------------------------------------------
 * struct wrong_type
 *   Thrown when a function of incorrect type is used. Includes details about
 *   the actual type that was provided and the accepted types that should have
 *   been used instead.
 *---------------------------------------------------------------------------**/
struct wrong_type: exception {
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  public instance variables  /////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  vector<string> AcceptedTypes;
  string         UserType;
  string         EventName;
  string         Message;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /* ... */
  wrong_type
    (string                     EventName,
     vector<const type_info *>  AcceptedTypes,
     const type_info           *UserType,
     string                     What  = "",
     string                     Scope = "")
      : EventName(EventName),
	UserType (demangle(*UserType)) {
    for (const type_info *AcceptedType : AcceptedTypes)
      this->AcceptedTypes.push_back(demangle(*AcceptedType));
    
    string Location =
      Scope.empty()
      ? "for"
      : format("for {} in", Scope);
    Message = format
      ("Wrong {} type {} event \"{}\":\n",
       What, Location, EventName);
    for (string &AcceptedType : this->AcceptedTypes)
      Message += format("  expected: {}\n", AcceptedType);
    Message += format("     found: {}\n", this->UserType);
  } // wrong_type()

  /* ... */
  const char *what () const noexcept override {
    return Message.c_str();
  } // what()
}; // struct wrong_type

/**---------------------------------------------------------------------------
 * struct wrong_arguments
 *   Thrown when emit() receives arguments contrasting the underlying accepted
 *   function type. This could include a mismatch in arity or argument types.
 *   This type must be created with the static factory create() method.
 *---------------------------------------------------------------------------**/
struct wrong_arguments: exception {
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  public instance variables  /////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  string AcceptedFunctionParameterString;
  string EmitParameterString;
  string EventName;
  string Message;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /* ... */
  const char *what () const noexcept override {
    return Message.c_str();
  } // what()

  /*
   * create()
   *   We are required to use a factory method for creating this exception
   *   since it is not possible in C++ to specify the template arguments for a
   *   constructor outside of normal deduction. And we cannot template the
   *   struct because then the exception name becomes unresolvable for most
   *   test frameworks. Therefore, we have chosen to make the constructor
   *   private (so no one accidentaly calls it) and to go for this approach
   *   instead.
   * see: struct wrong_arguments
   */
  template<typename... Args>
  static wrong_arguments create
    (string AcceptedFunctionParameterString,
     string EventName) {
    return wrong_arguments
      (AcceptedFunctionParameterString,
       stringify_parameter_pack<Args...>(),
       EventName);
  } // create()

private: ///////////////////////////////////////////////////////////////////////
  /* ... */
  wrong_arguments
    (string AcceptedFunctionParameterString,
     string EmitParameterString,
     string EventName)
      : AcceptedFunctionParameterString(AcceptedFunctionParameterString),
	EmitParameterString            (EmitParameterString),
	EventName                      (EventName) {
    Message = format
      ("Wrong arguments for emit() in event \"{}\":\n"
       "  expected: ({})\n"
       "     found: ({})",
       this->EventName,
       this->AcceptedFunctionParameterString,
       this->EmitParameterString);
  } // wrong_arguments()
}; // struct wrong_arguments

/**---------------------------------------------------------------------------
 * struct no_such_event
 *   Thrown if no event by the given name exists.
 *---------------------------------------------------------------------------**/
struct no_such_event: exception {
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  public instance variables  /////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  string KeyName;
  string Message;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /* ... */
  no_such_event (string KeyName): KeyName((KeyName)) {
    Message = format("No such : \"{}\".", this->KeyName);
  } // no_such_key()

  /* ... */
  const char *what () const noexcept override {
    return Message.c_str();
  } // what()
}; // struct no_such_event

/**---------------------------------------------------------------------------
 * using handle
 *   The type of a handle used for the removal and otherwise identification of
 *   callbacks inside an event.
 *---------------------------------------------------------------------------**/
using handle = shared_ptr<int>;

/*
 * struct thread_local_storage
 *   Minimal thread local storage object that simulates the behaviour you would
 *   expect had it been possible to use thread_local member variables.
 */
template<typename T>
struct thread_local_storage {
  inline static thread_local unordered_map<thread_local_storage *, T> Values
    = unordered_map<thread_local_storage *, T>();

  /* ... */
  ~thread_local_storage () {
    Values.erase(this);
  } // ~thread_local_storage

  /*
   * operator*()
   *   Access the element for this instance.
   */
  T &operator* () {
    return Values[this];
  } // operator*()

  /*
   * operator=()
   *   Sets it equal to the value of another thread local storage object.
   */
  thread_local_storage &operator= (const thread_local_storage &Value) {
    Values[this] = Values[const_cast<thread_local_storage *>(&Value)];
    return *this;
  } // operator=()

  /*
   * operator=()
   *   Assignment from a value of type `T` (or that is convertible to it) with
   *   perfect forwarding. Will be selected so long as the argument is not
   *   another `thread_local_storage`.
   */
  template<
    typename U,
    typename = enable_if_t<
        !is_same_v<decay_t<U>, thread_local_storage>
      && is_constructible_v<T, U&&>>>
  thread_local_storage &operator= (U &&Value) {
    Values[this] = std::forward<U>(Value);
    return *this;
  } // operator=()
}; // struct thread_local_storage

/**---------------------------------------------------------------------------
 * struct emitter
 *   Stores multiple event types that are accessible by name, where new such
 *   types can be registered, have callbacks added to them, and executed.
 *   <DefaultEventType>, if non-void, will create new event on access to an
 *   event that does not exist. [movable]
 *---------------------------------------------------------------------------**/
template<typename DefaultEventType = void>
struct emitter {
  /**-------------------------------------------------------------------------
   * struct event
   *   [movable] Configurable event structure enabling callback modification.
   *   Events can be configured with before/after hooks that run when callbacks
   *   are added or removed, or with an interceptor capable of changing the
   *   arguments that are passed through or make the callback run multiple times
   *   or not at all.
   *-------------------------------------------------------------------------**/
  struct event {
    /**-----------------------------------------------------------------------
     * using weight
     *   Specifies the level of precedence a callback takes in the order of
     *   execution when the event is emitted with emit().
     *-----------------------------------------------------------------------**/
    using weight = unsigned short;

    /**-----------------------------------------------------------------------
     * using on_function
     *   The function type used for set_on_insert() and set_on_remove(),
     *   which define the behaviour that happens before and respectively
     *   after a callback function is added or removed.
     *-----------------------------------------------------------------------**/
    using on_function = function<void (handle &)>;
    
    friend struct emitter;
    
  private: /////////////////////////////////////////////////////////////////////
    /*
     * struct function_traits
     *   Helper to figure out the types that make up a function or lambda.
     */
    template<typename T>
    struct function_traits;
    template<typename C, typename R, typename... Args> // Lambda
    struct function_traits<R(C::*)(Args...) const> {
      using function    = std::function<void (Args...)>;
      using interceptor = std::function<void (handle &, function, Args...)>;

      /*
       * construct_heap_function()
       *   Constructs a function, adding any other necessary functions that
       *   ought to be called in conjunction with it, but ONLY if required. We
       *   construct the smallest possible function.
       */
      static function construct_heap_function
        (event       *Event,
	 handle       Handle,
	 any          InterceptorAny,
	 function     Function) {
	// We don't need to try-catch here because we have already type-checked
	// the interceptor
	interceptor Interceptor = any_cast<interceptor>(InterceptorAny);
	
	return [=](Args... args) mutable {
	  Interceptor(Handle, Function, args...);
	};
      } // construct_heap_function()
    }; // struct function_traits
    template <typename F> // Deduce correct specialization
    struct function_traits : function_traits<decltype(&F::operator())> {};

  public: //////////////////////////////////////////////////////////////////////
    // Delete copy constructor and assignment operator
    event (const event&)            = delete;
    event &operator= (const event&) = delete;

    /* ... */
    ~event () {
      clear();
    } // ~event()
    
    /*
     * event()
     *   Move constructor.
     */
    event (event &&Other) noexcept {
      Name                            = std::move(Other.Name);
      Parent                          = Other.Parent;
      AcceptedFunctionType            = Other.AcceptedFunctionType;
      AcceptedFunctionParameterString = std::move(Other.AcceptedFunctionParameterString);
      AcceptedInterceptorType         = Other.AcceptedInterceptorType;
      NextMetaType                    = Other.NextMetaType;
      Slots                           = std::move(Other.Slots);
      NextHandle                      = Other.NextHandle;
      NextMeta                        = Other.NextMeta;
      OnInsertFunction                = std::move(Other.OnInsertFunction);
      OnRemoveFunction                = std::move(Other.OnRemoveFunction);
      InterceptorFunction             = std::move(Other.InterceptorFunction);
      IsCalling                       = false;
      DeferredRemoves                 = vector<handle>();
      DeferredInserts                 = vector<slot>();
    } // event()

    /*
     * operator=()
     *   Move assignment.
     */
    event &operator= (event &&Other) noexcept {
      if (this != &Other) {
	this->~event();                     // Call destructor
	new (this) event(std::move(Other)); // Move constructor placement new
      }
      return *this;
    } // operator=()

    /**-----------------------------------------------------------------------
     * get_function()
     *   Retrieves function associated with handle. <Handle> is the handle from
     *   which a lambda will be retrieved. <T> is the type the function is in.
     *   <Throws> `wrong_type` if `T` does not match the underlying lambda
     *   type. <Returns> the lambda object associated with hanled.
     *-----------------------------------------------------------------------**/
    template<typename T>
    function<T> &get_function (handle &Handle) {
      using F = function<T>;
      
      // exception: wrong_type /////////////////////////////////////////////////
      if (&typeid(F) != AcceptedFunctionType)
	throw wrong_type
	  (Name, { AcceptedFunctionType }, &typeid(F),
	   "lambda", "get_lambda()");
      //////////////////////////////////////////////////////////////////////////
      
      slot &Slot = Slots[*Handle];
      shared_ptr<void> &FunctionPointer = Slot.Function;
      F *Function = (F *)FunctionPointer.get();
      return *Function;
    } // get_function()
    
    /**-----------------------------------------------------------------------
     * get_meta()
     *   Retrieves metadata tuple with specified types. <Args> are the types
     *   that the metadata tuple stores. <Handle> refers to the callback that
     *   metadata will be retrieved from. <Throws> `wrong_type` if provided
     *   `Args` do not match the underlying types of the metadata for `Handle`.
     *   <Returns> the metadata tuple associated with the specified handle.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    tuple<Args...> &get_meta (handle &Handle) {
      using T = tuple<Args...>;
      
      slot &Slot = Slots[*Handle];
      
      // exception: wrong_type /////////////////////////////////////////////////
      if (&typeid(T) != Slot.MetaType)
	throw wrong_type
	  (Name, { Slot.MetaType }, &typeid(T),
	   "meta", "get_meta()");
      //////////////////////////////////////////////////////////////////////////
      
      return any_cast<T &>(Slot.Meta);
    } // get_meta()

    /**-----------------------------------------------------------------------
     * set_meta()
     *   Regenerate the metadata `tuple` with new values once there is a
     *   handle with associated metadata. To generate metadata for the right
     *   thereafter inserted callback, instead use operator()(). <Handle>
     *   refers to the callback for which metadata should be regenerated.
     *   <Arguments...> is the arguments that the new metadata tuple should
     *   be made from.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    void set_meta (handle &Handle, Args... Arguments) {
      tuple<Args...> Meta = make_tuple(Arguments...);
      slot &Slot    = Slots[*Handle];
      Slot.Meta     = Meta;
      Slot.MetaType = const_cast<type_info *>(&typeid(Meta));
    } // set_meta()

    /**-----------------------------------------------------------------------
     * is_meta_of()
     *   Checks if the types of the metadata object associated with `Handle`
     *   is the specified arguments `Args`. <Handle> is the handle associated
     *   with a callback function. <Args> are the types we are comparing with
     *   the metadata type to see if they match. <Returns> `true` if the
     *   types of metadata match `Args` and `false` otherwise.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    bool is_meta_of (handle &Handle) {
      return Slots[*Handle].Meta.type() == typeid(tuple<Args...>);
    } // is_meta_of()
    
    /**-----------------------------------------------------------------------
     * meta_accepts()
     *   Call this once for each type the metadata can have. If the user has
     *   not specified metadata using operator()() that meets the criteria,
     *   the insert() and operator=() functions will throw an exception.
     *   <Args> are the types of a single accepted metadata type. <Returns>
     *   `*this` for chaining.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    event &meta_accepts () {
      AcceptedMetaTypes.push_back(&typeid(tuple<Args...>));
      return *this;
    } // meta_accepts()

    /**-----------------------------------------------------------------------
     * meta_accepts_anything()
     *   This only needs to be called if meta_accepts() has already been
     *   called, and the desired behaviour is to undo all hitherto specified
     *   accepted meta types, making operator() revert to accept any metadata
     *   types whatever. <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &meta_accepts_anything () {
      AcceptedMetaTypes.clear();
      return *this;
    } // meta_accepts_anything()

    /**-----------------------------------------------------------------------
     * set_on_insert()
     *   Sets a function that will run whenever a callback function is
     *   inserted into the event. <Function> is the function to be run when
     *   a callback is inserted. <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    template<typename F>
    requires same_as<typename function_traits<F>::function, on_function>
    event &set_on_insert (F Function) {
      OnInsertFunction = Function;
      return *this;
    } // set_on_insert()

    /**-----------------------------------------------------------------------
     * unset_on_insert()
     *   Unsets the function previously set through set_on_insert().
     *   <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &unset_on_insert () {
      OnInsertFunction.reset();
      return *this;
    } // unset_on_insert()

    /**-----------------------------------------------------------------------
     * set_on_remove()
     *   Sets a function that will run right before a callback function is
     *   removed from the event. <Function> is the function to be run when
     *   a callback is removed. <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    template<typename F>
    requires same_as<typename function_traits<F>::function, on_function>
    event &set_on_remove (F Function) {
      OnRemoveFunction = Function;
      return *this;
    } // set_on_remove()

    /**-----------------------------------------------------------------------
     * unset_on_remove()
     *   Unsets the function previously set through set_on_remove().
     *   <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &unset_on_remove () {
      OnRemoveFunction.reset();
      return *this;
    } // unset_on_remove()

    /**-----------------------------------------------------------------------
     * set_interceptor()
     *   Sets a function that will wrap every callback function that is
     *   inserted past this point. The handler is inputted as a parameter to
     *   the interceptor, so that the interceptor can modify the behaviour
     *   surrounding the handler as it sees fit. <Throws> `wrong_type` if `F`
     *   is not of a valid interceptor function type. <Function> is the
     *   interceptor that wraps handlers inserted past this point. <Returns>
     *   `*this` for chaining.
     *-----------------------------------------------------------------------**/
    template<typename F>
    event &set_interceptor (F Function) {
      using Traits = function_traits<F>;
      using function = Traits::function;
      function NormalizedFunction(Function); // Uniformalize the type

      // exception: wrong_type /////////////////////////////////////////////////
      if (&typeid(NormalizedFunction) != AcceptedInterceptorType)
	throw wrong_type
	  (Name, { AcceptedInterceptorType }, &typeid(NormalizedFunction),
	   "function", "set_interceptor()");
      //////////////////////////////////////////////////////////////////////////
      
      InterceptorFunction = NormalizedFunction;
      return *this;
    } // set_interceptor()

    /**-----------------------------------------------------------------------
     * unset_interceptor()
     *   Unsets the function previously set through set_interceptor().
     *   <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &unset_interceptor () {
      InterceptorFunction.reset();
      return *this;
    } // unset_interceptor()

    /**-----------------------------------------------------------------------
     * operator()
     *   Generates a metadata `tuple` that can be used to communicate with
     *   the event creator. The generated tuple will be attached to the
     *   callback that is inserted next. <Arguments> are the arguments the
     *   metadata `tuple` should be created from. <Returns> `*this` for
     *   chaining.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    event &operator() (Args... Arguments) {
      tuple<Args...> Meta = make_tuple(Arguments...);
      NextMetaType = const_cast<type_info *>(&typeid(Meta));
      NextMeta     = Meta;
      return *this;
    } // operator() ()
    
    /**-----------------------------------------------------------------------
     * insert()
     *   [thread-safe] Inserts a callback to be outputted when emit() is called
     *   by the event creator. <Throws> `wrong_type` if `F` is not of the
     *   accepted function type. <Throws> `wrong_type` if the user-provided
     *   metadata is of a type that is not accepted. <Function> is the handler
     *   that will become part of the event, that will run whenever emit() is
     *   called. <Weight> is the priority level for `Function` in the handler
     *   list. <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    template<typename F, typename = enable_if_callable<F>>
    event &insert (const F &Function, weight Weight = 0) {
      using FunctionTraits = function_traits<F>;
      using function = FunctionTraits::function;
      
      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      // exception: wrong_type /////////////////////////////////////////////////
      if (&typeid(function) != AcceptedFunctionType)
	throw wrong_type
	  (Name, { AcceptedFunctionType }, &typeid(function), "function");
      //////////////////////////////////////////////////////////////////////////

      // exception: wrong_type /////////////////////////////////////////////////
      // Check if the type of the meta needs to have constraints reinforced
      if (!AcceptedMetaTypes.empty()) {
	bool IsAccepted = false;
	const type_info *MetaType = &(*NextMeta).type();
	for (const type_info *AcceptedMetaType : AcceptedMetaTypes)
	  if (MetaType == AcceptedMetaType) {
	    IsAccepted = true;
	    break;
	  }
	if (!IsAccepted) {
	  throw wrong_type
	    (Name, AcceptedMetaTypes, MetaType,
	     "meta", "insert()");
	}
      }
      //////////////////////////////////////////////////////////////////////////
      
      // We set up the next handle while the size of functions is that which the
      // index will be
      NextHandle = make_shared<int>(Slots.size() + DeferredInserts.size());
      Parent->NextHandle = &*NextHandle; // It needs to be retrievable from
                                         // the parent

      // Figure out which function type will be called (and if it is wrapped)
      function *FunctionOnHeap;

      bool HasInterceptorFunction = (bool) InterceptorFunction.has_value();
      
      if (HasInterceptorFunction) {
	FunctionOnHeap = new function
	  (FunctionTraits::construct_heap_function
	   (this, *NextHandle,
	    InterceptorFunction, Function));
      } else
	FunctionOnHeap = new function(Function);

      // Generate a deleter function to delete the `function_on_heap` memory
      auto Deleter = [FunctionOnHeap](void *_) {
	delete FunctionOnHeap;
      };

      // Tabulate arguments for `Insert`
      shared_ptr<void> FunctionArgument = shared_ptr<void>
	((void *)FunctionOnHeap, Deleter);
      handle HandleArgument = *NextHandle;
      any MetaArgument = *NextMeta;
      (*NextMeta).reset(); // Reset `NextMeta` for next run
      
      slot Slot = {
	FunctionArgument,
	HandleArgument,
	MetaArgument,
	NextMetaType,
	Weight
      };
      if (!IsCalling)
	// NOT currently calling, inserting is safe
	internal_insert(Slot);
      else
	// We ARE calling currently, we cannot modify any of the vectors without
	// trouble, as it were
	DeferredInserts.push_back(Slot);

      //////////////////////////////////////////////////////////////////////////
      return *this;
    } // insert()

    /**-----------------------------------------------------------------------
     * operator=()
     *   Syntactic sugar for callback insertion. Same as insert() but without
     *   the possibility of specifying a weight for the callback in the list
     *   of handlers. <Function> is the handler that will become part of the
     *   event, that will run whenever emit() is called.
     *-----------------------------------------------------------------------**/
    template<typename F, typename = enable_if_callable<F>>
    void operator= (const F &Function) {
      insert<F>(Function);
    } // operator=()

    /**-----------------------------------------------------------------------
     * operator*()
     *   Retrieves a handle to the last added callback. Following the
     *   insertion of a callback into the event is the only opportunity to
     *   retrieve such a handle. <Returns> the handle to the last added
     *   callback function.
     *-----------------------------------------------------------------------**/
    handle operator* () {
      return *NextHandle;
    } // operator*()

    /**-----------------------------------------------------------------------
     * remove()
     *   [thread-safe] Removes a callback function per its handle, which was
     *   retrieved by dereferencing `*this` after the handler insertion.
     *   <Handle> is the handle associated with a callback function. <Returns>
     *   `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &remove (handle &Handle) {
      if (Handle == nullptr || *Handle == -1)
	// Make sure `Handle` is valid
	return *this;

      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      if (IsCalling) {
	// If we are currently calling the functions stored in this event, we
	// must defer removing to not upset the iteration
	
	Slots[*Handle].ScheduledForRemoval = true;
	DeferredRemoves.push_back(Handle);
	return *this; // [return]
      }
      
      internal_remove<false>(Handle);
      //////////////////////////////////////////////////////////////////////////

      return *this;
    } // remove()

    /**-----------------------------------------------------------------------
     * emit()
     *   [thread-safe] Runs all callback functions (or handlers) of the event.
     *   <Throws> `wrong_arguments` if the arguments inputted do not correspond
     *   to the event argument types. <Arguments> are the arguments that will
     *   be forwarded to all handlers.
     *-----------------------------------------------------------------------**/
    template<typename... Args>
    void emit (Args... Arguments) {
      using function = function<void (Args...)>;

      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      // exception: wrong_arguments ////////////////////////////////////////////
      if (&typeid(function) != AcceptedFunctionType)
	throw wrong_arguments::create<Args...>
	  (AcceptedFunctionParameterString, Name);
      //////////////////////////////////////////////////////////////////////////
      IsCalling = true;
      int NSlots = Slots.size();
      for (int I = 0; I < NSlots; ++ I) {
	slot &Slot = Slots[I];
	if (Slot.ScheduledForRemoval)
	  continue;
	shared_ptr<void> &FunctionPointer = Slot.Function;
	function *Function = (function *)FunctionPointer.get();
	(*Function)(Arguments...);
      }
      IsCalling = false;

      // If deferred inserts accumulated during run, we need to add those first
      // before doing anything else, so to not upset the handle locations
      if (!DeferredInserts.empty()) {
	for (slot &Slot : DeferredInserts)
	  internal_insert(Slot);
	DeferredInserts.clear();
      }

      // If deferred removes have accumulated during run, we need to run the
      // Remove method on them.
      if (!DeferredRemoves.empty()) {
	for (handle &Handle : DeferredRemoves)
	  internal_remove(Handle);
	DeferredRemoves.clear();
      }
      //////////////////////////////////////////////////////////////////////////
    } // emit()

    /**-----------------------------------------------------------------------
     * clear()
     *   [thread-safe] Removes all callback functions from this event.
     *   <Returns> `*this` for chaining.
     *-----------------------------------------------------------------------**/
    event &clear () {
      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      while (!Slots.empty()) {
	internal_remove(Slots[0].Handle);
      }
      //////////////////////////////////////////////////////////////////////////

      return *this;
    } // clear()

    /**-----------------------------------------------------------------------
     * length()
     *   [thread-safe] The number of callback functions that are managed and
     *   run by the event. <Returns> the number of functions stored in this
     *   event. This includes deferred inserts (callbacks that have not been
     *   inserted yet because emit() was running when it was added and it has
     *   not finished running).
     *-----------------------------------------------------------------------**/
    size_t length () {
      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      return Slots.size() + DeferredInserts.size();
    } // length()

    /**-----------------------------------------------------------------------
     * empty()
     *   [thread-safe] Checks if there are any callbacks managed by the event.
     *   <Returns> `true` if length() returns `0`, and `false` otherwise.
     *-----------------------------------------------------------------------**/
    bool empty () {
      // lock_guard: Mutex /////////////////////////////////////////////////////
      lock_guard Guard(Mutex);

      return length() == 0;
    } // empty()

  private: /////////////////////////////////////////////////////////////////////
    /*
     * struct slot
     *   This structure stores all the information about a function that is
     *   callable through calling the `emit` method.
     */
    struct slot {
      shared_ptr<void>  Function;
      handle            Handle;
      any               Meta;
      type_info        *MetaType;
      weight            Weight;
      bool              ScheduledForRemoval {false};
    }; // struct slot
    
    ////////////////////////////////////////////////////////////////////////////
    ///////////////////////  private instance variables  ///////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // Note: Don't forget to modify move constructor accordingly when adding new
    //     | member variables.
    string                        Name;
    emitter                      *Parent;
    type_info                    *AcceptedFunctionType;
    string                        AcceptedFunctionParameterString;
    type_info                    *AcceptedInterceptorType;
    type_info                    *NextMetaType;
    vector<const type_info *>     AcceptedMetaTypes;
    vector<slot>                  Slots;
    thread_local_storage<handle>  NextHandle;
    thread_local_storage<any>     NextMeta;
    optional<on_function>         OnInsertFunction    {};
    optional<on_function>         OnRemoveFunction    {};
    any                           InterceptorFunction {};
    bool                          IsCalling           {false};
    vector<handle>                DeferredRemoves;
    vector<slot>                  DeferredInserts;
    recursive_mutex               Mutex               {};
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    /*
     * event()
     *   Create event with name `Name` and parent `Parent` (being the `emitter
     *   that stores `this`).
     */
    event (string Name, emitter *Parent)
      : Name                    (Name),
	Parent                  (Parent),
	Slots                   (vector<slot>()),
	AcceptedFunctionType    (const_cast<type_info *>(&typeid(nullptr))),
	AcceptedInterceptorType (const_cast<type_info *>(&typeid(nullptr))),
	NextMetaType            (const_cast<type_info *>(&typeid(nullptr))) {
      // ...
    } // event()
    
    /*
     * constrain()
     *   Specifies the accepted function type, generating any internal data
     *   necessary for type checking.
     */
    template<typename F>
    void constrain () {
      using FunctionTraits = function_traits<F>;
      using function    = FunctionTraits::function;
      using interceptor = FunctionTraits::interceptor;
      
      AcceptedFunctionType = const_cast<type_info *>(&typeid(function));
      AcceptedFunctionParameterString =
	parameter_pack_stringifier<F>::stringify();
      AcceptedInterceptorType = const_cast<type_info *>(&typeid(interceptor));
    } // constrain()
    
    /*
     * internal_insert()
     *   Insertion into `slots`. This is called from either `insert` or
     *   deferred inserts. One major difference between this method and
     *   `insert` is that this method is NOT thread-safe.
     */
    void internal_insert (slot &Slot) {
      // Push back function, handle, and meta
      if (Slot.Weight == 0)
	// Lowest possible weight always goes on the back
	Slots.push_back(Slot);
      else {
	int T = 0;
	for (auto I = Slots.begin(); I != Slots.end(); ++ I, ++ T) {
	  slot &Next = *I;
	  if (Next.Weight < Slot.Weight) {
	    I = Slots.insert(I, Slot);
	    for (; I != Slots.end(); ++ I, ++ T) {
	      slot &Next = *I;
	      *Next.Handle = T;
	      // ^- Correct the position for each handle so it refers to the
	      //    location its slot is now at
	    }
	    break; // Might make it ever-so slightly faster
	  }
	}
      }

      // If an on insert function is specified, run it now that the function has
      // been inserted
      if (OnInsertFunction) {
	auto DereferencedOnInsertFunction = *OnInsertFunction;
	DereferencedOnInsertFunction(Slot.Handle);
      }
    } // internal_insert()

    /*
     * internal_remove()
     *   Removal from `slots`. The `remove` method builds on this one and it
     *   will also be used for deferred removal. One major difference between
     *   this method and `remove` is that this method is NOT thread-safe.
     */
    template<bool CheckHandleValidity = true>
    void internal_remove (handle &Handle) {
      if constexpr (CheckHandleValidity) {
	if (Handle == nullptr || *Handle == -1)
	  // Make sure `Handle` is valid
	  return;
      }
      
      // If an on remove function is specified, we run it here prior to deleting
      // the element so its information still remains
      if (OnRemoveFunction) {
	auto DereferencedOnRemoveFunction = *OnRemoveFunction;
	DereferencedOnRemoveFunction(Handle);
      }

      int LastIndex = Slots.size() - 1;
      int Index = *Handle;
      *Handle = -1;
      // ^- In case the handle still is in use somewhere, we set it to `-1` to
      //    inform the user it is invalid
      
      if (Index < LastIndex) {
	Slots[Index] = std::move(Slots[LastIndex]);
	*Slots[Index].Handle = Index;
      }
      Slots.pop_back(); // Remove the last element
    } // internal_remove()
  }; // struct event

  // Delete copy constructor and assignment operator
  emitter (const emitter&)            = delete;
  emitter &operator= (const emitter&) = delete;

  /* ... */
  ~emitter () {
    // ...
  } // ~emitter()

  /* ... */
  emitter () {
    // ...
  } // emitter()

  /*
   * emitter()
   *   Move constructor.
   */
  emitter (emitter &&Other) noexcept {
    NameToEvent = std::move(Other.NameToEvent);
    NextHandle  = Other.NextHandle;
  } // emitter()

  /*
   * operator=()
   *   Move assignment.
   */
  emitter &operator= (emitter &&Other) noexcept {
    if (this != &Other) {
      this->~emitter();
      new (this) emitter(std::move(Other));
    }
    return *this;
  } // operator=()

  /**-------------------------------------------------------------------------
   * operator*()
   * alias: event::operator*()
   *-------------------------------------------------------------------------**/
  handle operator* () {
    return **NextHandle;
  } // operator*()

  /**-------------------------------------------------------------------------
   * create()
   *   Creates an event by name `Name` that can be configured using the
   *   available methods in the returned `event`. Then, emit() can be used
   *   to call all the user added callbacks (or handlers). <Name> is the
   *   name of the event to be created. <Returns> the newly created event
   *   that is now tracked by the emitter.
   *-------------------------------------------------------------------------**/
  template<typename Q>
  event &create (string Name) {
    if (!NameToEvent.contains(Name)) {
      // Insert event if it does not exist
      NameToEvent.insert(Name, event(Name, this));
      event &Event = NameToEvent.get(Name);
      Event.template constrain<function<Q>>();
    }
    return NameToEvent.get(Name);
  } // create()

  /**-------------------------------------------------------------------------
   * retire()
   *   Retires an event, removing it from the managed events. <Name> is the
   *   name of the event to be removed. <Returns> `*this` for chaining.
   *-------------------------------------------------------------------------**/
  emitter &retire (string Name) {
    NameToEvent.erase(Name);
    return *this;
  } // retire()

  /**-----------------------------------------------------------------------
   * contains()
   *   Check if an event exists. <Name> is the name of the event to be
   *   checked if it exists. <Returns> `true` if an event by name `Name`
   *   exists.
   *-----------------------------------------------------------------------**/
  bool contains (string Name) {
    return NameToEvent.contains(Name);
  } // constains()

  /**-------------------------------------------------------------------------
   * operator[]()
   *   Retrieve an event by name, or insert one if it does not exist before
   *   retrieving it if the emitter has its `DefaultEventType` specified.
   *   <Throws> `no_such_event` if no event by name `Name` exists. <Name> is
   *   the name of the event to be retrieved. <Returns> the event by name
   *   `Name` if it exists or was implicitly created.
   *-------------------------------------------------------------------------**/
  event &operator[] (string Name) {
    if constexpr (!is_void_v<DefaultEventType>) {
      if (!NameToEvent.contains(Name))
	return create<DefaultEventType>(Name);
    }
    try {
      return NameToEvent.get(Name);
    } catch (aport::no_such_key &no_such_key) {
      throw no_such_event(Name);
    }
  } // operator[]()

private: ///////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  private instance variables  ////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  aport::tree<event>             NameToEvent;
  thread_local_storage<handle *> NextHandle;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
}; // struct emitter

} // namespace wheel

/**---------------------------------------------------------------------------
 * with()
 *   A way to initialize an inline struct with some starting values. <Struct>
 *   is the curly-brace-enclosed struct body, while <...> are the initial
 *   values of the struct fields.
 *---------------------------------------------------------------------------**/
#define with(Struct, ...)		 \
  ([&]() {				 \
    struct state Struct;		 \
    auto Pointer = make_shared<state>(); \
    *Pointer = {__VA_ARGS__};		 \
    return Pointer;			 \
  })()

#endif
