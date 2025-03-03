
#include <wheel/wheel.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <filesystem>
#include <tuple>
#include <thread>

using namespace std;

TEST_CASE("@", "[info]") {
  if (!filesystem::is_directory("build")) {
    fprintf(stderr, "Must run tests from project root.\n");
    exit(1);
  }
}

TEST_CASE("Creation, Insertion and Calling", "[test]") {
  list<int> Data;
  wheel::emitter Events;

  // Make sure "no_such_event" thrown when using non-created event
  REQUIRE_THROWS_AS(Events["test"], wheel::no_such_event);

  // Make sure events run in order they were added by default
  Events.create<void ()>("signal");
  Events["signal"] = [&]() { Data.push_back(0); };
  Events["signal"] = [&]() { Data.push_back(1); };
  Events["signal"] = [&]() { Data.push_back(2); };
  Events["signal"] = [&]() { Data.push_back(3); };
  Events["signal"] = [&]() { Data.push_back(4); };
  Events["signal"].emit();
  REQUIRE(Data == list({ 0, 1, 2, 3, 4 }));
  REQUIRE(Data != list({ 2, 3, 4 }));

  // Insertion during emitting
  Data.clear();
  Events.create<void ()>("test");
  function<void ()> adder;
  adder = [&]() {
    Data.push_back(Data.empty() ? 0 : Data.back() + 1);
    Events["test"] = adder;
  };
  Events["test"] = adder;
  Events["test"].emit(); // 1 callback spawns 1 new callback (+ 1 N)
  REQUIRE(Data == list({ 0 }));
  Events["test"].emit(); // 2 callbacks spawn 2 new callbacks (+ 2 N)
  REQUIRE(Data == list({ 0, 1, 2 }));
  Events["test"].emit(); // 4 callbacks spawn 4 new callbacks (+ 4 N)
  REQUIRE(Data == list({ 0, 1, 2, 3, 4, 5, 6 }));

  // Verify weighted insertion order
  Data.clear();
  Events.create<void ()>("weight");
  Events["weight"] = [&]() { Data.push_back(0); };
  Events["weight"] = [&]() { Data.push_back(1); };
  Events["weight"].insert([&]() { Data.push_back(2); }, 1);
  Events["weight"].emit();
  REQUIRE(Data == list({ 2, 0, 1 }));
  Data.clear();
  Events["weight"].insert([&]() { Data.push_back(3); }, 3);
  Events["weight"].insert([&]() { Data.push_back(4); }, 2);
  Events["weight"].emit();
  REQUIRE(Data == list({ 3, 4, 2, 0, 1 }));

  // Event that spawns an event
  Data.clear();
  Events["weight"].clear();
  Events["weight"] = [&]() {
    Data.push_back(0);
    Events["weight"].insert([&]() {
      Data.push_back(1);
    }, Data.size());
  };
  Events["weight"].emit(); // 1 callback spawns 1 new callback
  REQUIRE(Data == list({ 0 }));
  Events["weight"].emit(); // 2 callbacks spawn 1 new callback
  REQUIRE(Data == list({ 0, 1, 0 }));
  Events["weight"].emit(); // 3 callbacks spawn 1 new callback
  REQUIRE(Data == list({ 0, 1, 0, 1, 1, 0 }));

  REQUIRE(Events.contains("signal"));
  Events.retire("signal");
  REQUIRE(!Events.contains("signal"));
}

TEST_CASE("Type Checking", "[test]") {
  wheel::emitter Events;
  Events.create<void(int, int, int)>("test");

  // Verify "wrong_arguments"
  REQUIRE_THROWS_AS(Events["test"].emit(2.3), wheel::wrong_arguments);
  REQUIRE_THROWS_AS(Events["test"].emit("test"), wheel::wrong_arguments);
  REQUIRE_THROWS_AS(Events["test"].emit(string("a")), wheel::wrong_arguments);
  REQUIRE_THROWS_AS(Events["test"].emit(1, 2, 3.2), wheel::wrong_arguments);
  REQUIRE_THROWS_AS(Events["test"].emit(1, 2, 3, 4), wheel::wrong_arguments);
  REQUIRE_NOTHROW(Events["test"].emit(1, 2, 3));

  // Verify "wrong_type"
  REQUIRE_THROWS_AS(Events["test"] = [](int A) {}, wheel::wrong_type);
  REQUIRE_NOTHROW(Events["test"] = [](int A, int B, int C) {});

  // Verify meta extraction throws
  Events
    .create<void()>("meta")
    .set_on_insert([&](wheel::handle &Handle) {
      REQUIRE_THROWS_AS(Events["meta"].get_meta<int>(Handle), wheel::wrong_type);
      REQUIRE_NOTHROW(Events["meta"].get_meta<int, double>(Handle));
      Events["meta"].set_meta(Handle, 65, 32);
      REQUIRE_THROWS_AS((Events["meta"].get_meta<int, double>(Handle)), wheel::wrong_type);
      REQUIRE_NOTHROW(Events["meta"].get_meta<int, int>(Handle));
    });
  Events["meta"](12, 34.0) = []() {
    // ...
  }; // <- this is appended

  // Verify type checking for interceptors
  REQUIRE_THROWS_AS
    (Events["test"].set_interceptor
     ([](wheel::handle &Handle, function<void ()> Target) {}),
     wheel::wrong_type);
  REQUIRE_NOTHROW
    (Events["test"].set_interceptor
     ([](wheel::handle &Handle, function<void (int, int, int)> Target, int A, int B, int C) {}));

  // Verify meta constraining functionality
  Events
    .create<void ()>("constrain")
    .meta_accepts<int, double>();
  REQUIRE_THROWS_AS(Events["constrain"] = []() {}, wheel::wrong_type);
  REQUIRE_NOTHROW(Events["constrain"](1, 3.4) = []() {});
  Events["constrain"]
    .meta_accepts_anything()
    .meta_accepts<int>()
    .meta_accepts<double>();
  REQUIRE_THROWS_AS(Events["constrain"] = []() {}, wheel::wrong_type);
  REQUIRE_THROWS_AS(Events["constrain"](string("")) = []() {}, wheel::wrong_type);
  REQUIRE_NOTHROW(Events["constrain"](34) = []() {});
  REQUIRE_NOTHROW(Events["constrain"](98.7) = []() {});
  
  Events.create<void (float &)>("reference");
  Events["reference"] = [](float &A) {
    A = 12;
  };
  float B = 10;
  REQUIRE_NOTHROW(Events["reference"].emit<float &>(B));
  REQUIRE_THROWS_AS(Events["reference"].emit(B), wheel::wrong_arguments);
  REQUIRE(B == 12);
}

TEST_CASE("Removal", "[test]") {
  wheel::handle Handle1;
  wheel::handle Handle2;
  
  wheel::emitter Events;
  Events.create<void ()>("test");

  // Check if removing upcoming callback makes it so it does not run
  bool SecondRan = false;
  wheel::handle *Handle2Ptr = &Handle2;
  Events["test"] = [&, Handle2Ptr]() {
    Events["test"].remove(*Handle2Ptr);
  };
  Handle1 = *Events;
  Events["test"] = [&]() {
    SecondRan = true;
  };
  Handle2 = *Events;
  Events["test"].emit();
  REQUIRE(!SecondRan);
  SecondRan = false;
  Events["test"].emit();
  REQUIRE(!SecondRan);

  // Check if a callback may remove itself
  Events["test"].clear();
  wheel::handle *Handle1Ptr = &Handle1;
  Events["test"] = [&, Handle1Ptr]() {
    Events["test"].remove(*Handle1Ptr);
  };
  Handle1 = *Events;
  Events["test"].emit();
  REQUIRE(Events["test"].empty());

  // Check if we can remove from inside on_insert
  int N = 0;
  Events["test"].clear();
  Events["test"]
    .set_on_insert([&](wheel::handle &Handle) {
      // For instance, you might want to refuse lambdas with certain meta data
      Events["test"].remove(Handle);
    });
  Events["test"] = [&]() { ++ N; };
  Events["test"].emit();
  REQUIRE(N == 0);
  
  // Check if we can remove from inside interceptor
  N = 0;
  Events["test"]
    .unset_on_insert()
    .set_interceptor([&](wheel::handle &Handle, function<void ()> Target) {
      Target();
      Events["test"].remove(Handle);
    });
  Events["test"] = [&]() { ++ N; };
  Events["test"].emit();
  REQUIRE(N == 1);
  Events["test"].emit();
  REQUIRE(N == 1);
  REQUIRE(Events["test"].empty());

  // Check if get_function() works
  bool T = false;
  Events
    .create<void ()>("run")
    .set_on_insert([&](wheel::handle &Handle) {
      Events["run"].get_function<void ()>(Handle)();
    });
  Events["run"] = [&T]() {
    T = true;
  };
  REQUIRE(T == true);
}

TEST_CASE("Custom Events", "[test]") {
  int N = 0;
  wheel::emitter Events;

  // Verify event set up using "on_insert" and "interceptor"
  bool OnRemoveWasCalled = false;
  Events
    .create<void ()>("test")
    .set_on_insert([&](wheel::handle &Handle) {
      auto [ N ] = Events["test"].get_meta<int>(Handle);
      Events["test"].set_meta(Handle, N / 2);
    })
    .set_interceptor([&](wheel::handle &Handle, function<void ()> Target) {
      auto [ N ] = Events["test"].get_meta<int>(Handle);
      for (int i = 0; i < N; ++ i)
	Target();
      int Q = N - 1;
      if (Q == 0)
	Events["test"].remove(Handle);
      Events["test"].set_meta(Handle, Q);
    })
    .set_on_remove([&](wheel::handle &Handle) {
      OnRemoveWasCalled = true;
    });
  Events["test"](6) = [&]() { ++ N; };
  Events["test"].emit();
  REQUIRE(N == 3);
  Events["test"].emit();
  REQUIRE(N == 5);
  Events["test"].emit();
  REQUIRE(N == 6);
  REQUIRE(OnRemoveWasCalled);

  // See what happens if we remove on's and interceptor
  Events["test"](6) = [&]() { ++ N; };
  Events["test"].unset_interceptor();
  Events["test"].emit();
  REQUIRE(N == 9); // The callback was set up prior to unsetting interceptor
  Events["test"]
    .unset_on_insert()
    .unset_on_remove();
  OnRemoveWasCalled = false;
  Events["test"].emit();
  REQUIRE(N == 11);
  Events["test"].emit();
  REQUIRE(N == 12);
  REQUIRE(!OnRemoveWasCalled);
  REQUIRE(Events["test"].empty());
  Events["test"](100) = [&]() { ++ N; };
  Events["test"].emit();
  REQUIRE(N == 13);

  // Repeater event
  list<int> Ns;
  Events.create<void (int)>("repeater")
    .meta_accepts<int>()
    .set_interceptor([&](wheel::handle &Handle, function<void (int)> Target, int I) {
      auto [ N ] = Events["repeater"].get_meta<int>(Handle);
      for (int I = 0; I < N; ++ I)
	Target(I);
    });
  Events["repeater"](5) = [&](int I) {
    Ns.push_back(I);
  };
  Events["repeater"].emit(0);
  REQUIRE(Ns == list({ 0, 1, 2, 3, 4 }));
}

TEST_CASE("Tick Event", "[test]") {
  wheel::emitter<> Events;

  struct limiter_t {
    double time_per_call;
    double previous_time;

    limiter_t (uint32_t n_calls, double time_period):
      time_per_call(time_period / n_calls),
      previous_time(0.0) {}

    /**
     * Returns the number of calls that need to be processed before all caught up
     * with the timing specified in `this` limiter.
     */
    uint32_t N () {
      return 1;
    }
  }; // struct limiter_t
  
  Events.create<void ()>("tick");
  wheel::emitter<>::event *Tick = &Events["tick"];
  Tick
    ->meta_accepts<int, double>()
    .set_on_insert([=](wheel::handle &Handle) {
      auto [ N, T ] = Tick->get_meta<int, double>(Handle);
      Tick->set_meta(Handle, limiter_t(N, T));
    })
    .set_interceptor([=](wheel::handle &Handle, function<void ()> Target) {
      int I, N;
      auto [ Limiter ] = Tick->get_meta<limiter_t>(Handle);
      N = Limiter.N();
      for (I = 0; I < N; ++ I)
	Target();
    });
  
  wheel::handle Handle1;
  Events["tick"](60, 1.0) = [&]() {
    Events["tick"].remove(Handle1);
  };
  Handle1 = *Events;
  wheel::handle Handle2;
  Events["tick"](60, 1.0) = [&]() {
    Events["tick"].remove(Handle1);
  };
  Handle1 = *Events;
  wheel::handle Handle3;
  Events["tick"](60, 1.0) = [&]() {
    Events["tick"].remove(Handle1);
  };
  Handle1 = *Events;
  wheel::handle Handle4;
  Events["tick"](60, 1.0) = [&]() {
    Events["tick"].remove(Handle1);
  };
  Handle1 = *Events;
  wheel::handle Handle5;
  Events["tick"](60, 1.0) = [&]() {
    Events["tick"].remove(Handle1);
  };
  Handle1 = *Events;

  for (int I = 0; I < 50; ++ I)
    Events["tick"].emit();
}

TEST_CASE("Default Event Type", "[test]") {
  // Check usage of default event type, seeing if new events are indeed set up
  // automatically if one is specified (autovivification)
  wheel::emitter<void (int)> Events;
  REQUIRE_THROWS_AS(Events["hello-1"] = []() {}, wheel::wrong_type);
  REQUIRE_NOTHROW(Events["hello-2"] = [](int A) {});
}

TEST_CASE("Move", "[test]") {
  // Verify that we can move a wheel::emitter::event
  wheel::emitter Events;
  Events.create<void ()>("hello");
  Events.create<void ()>("goodbye");
  int N = 0;
  Events["hello"] = [&]() {
    ++ N;
  };
  Events["goodbye"] = std::move(Events["hello"]);
  // Events["hello"] is now in an unspecified state
  Events.retire("hello");
  Events["goodbye"].emit();
  REQUIRE(N == 1);

  // Verify that we can move a wheel::emitter
  wheel::emitter OtherEvents;
  OtherEvents = std::move(Events);
  // `Events` is now in an unspecified state
  OtherEvents["goodbye"].emit();
  REQUIRE(N == 2);
}

TEST_CASE("Multi-Threading", "[test]") {
  int N = 100;
  int X = 0; int Y = 0;
  wheel::emitter Events;
  Events.create<void ()>("test");
  int IsRunning = 2;
  
  thread Thread1([&]() {
    for (int I = 0; I < N; ++ I)
      Events["test"] = [&]() { ++ X; };
    Events["test"].emit();
    -- IsRunning;
  });
  Thread1.detach();

  thread Thread2([&]() {
    for (int I = 0; I < N; ++ I)
      Events["test"] = [&]() { ++ Y; };
    Events["test"].emit();
    -- IsRunning;
  });
  Thread2.detach();

  while (IsRunning)
    ;
  
  bool State = (X == N * 2) || (Y == N * 2);
  REQUIRE(State);
}

TEST_CASE("Comparison Between Types", "[benchmark]") {
  const auto Lambda = []() {};
  BENCHMARK("Run lambda") {
    Lambda();
  };

  function<void ()> Function = [&]() {};
  BENCHMARK("Run std::function") {
    Function();
  };
  
  wheel::emitter Events;
  Events.create<void ()>("test");
  Events["test"] = [&]() {};
  BENCHMARK("Run emit()") {
    Events["test"].emit();
  };

  auto &TestRef = Events["test"];
  BENCHMARK("Run emit() pointer") {
    TestRef.emit();
  };
}

TEST_CASE("Lambda Scaling", "[benchmark]") {
  // Helper to add N lambdas to an event
  auto setup_n_lambdas = [](wheel::emitter<void>& Events, int N) {
    Events.create<void()>("test");
    for(int i = 0; i < N; i++)
      Events["test"] = []() {};
  };

  wheel::emitter Events1;
  setup_n_lambdas(Events1, 1);
  BENCHMARK("1 lambda") {
    Events1["test"].emit();
  };

  wheel::emitter Events10;
  setup_n_lambdas(Events10, 10);
  BENCHMARK("10 lambdas") {
    Events10["test"].emit();
  };

  wheel::emitter Events100;
  setup_n_lambdas(Events100, 100);
  BENCHMARK("100 lambdas") {
    Events100["test"].emit();
  };

  wheel::emitter Events1000;
  setup_n_lambdas(Events1000, 1000);
  BENCHMARK("1000 lambdas") {
    Events1000["test"].emit();
  };
}

TEST_CASE("Lambda Scaling Reference", "[benchmark]") {
  // Helper to add N lambdas to an event
  auto setup_n_lambdas = [](wheel::emitter<void>& Events, int N) {
    Events.create<void()>("test");
    for(int i = 0; i < N; i++)
      Events["test"] = []() {};
  };

  wheel::emitter Events1;
  setup_n_lambdas(Events1, 1);
  auto &Test1 = Events1["test"];
  BENCHMARK("1 lambda") {
    Test1.emit();
  };

  wheel::emitter Events10;
  setup_n_lambdas(Events10, 10);
  auto &Test10 = Events10["test"];
  BENCHMARK("10 lambdas") {
    Test10.emit();
  };

  wheel::emitter Events100;
  setup_n_lambdas(Events100, 100);
  auto &Test100 = Events100["test"];
  BENCHMARK("100 lambdas") {
    Test100.emit();
  };

  wheel::emitter Events1000;
  setup_n_lambdas(Events1000, 1000);
  auto &Test1000 = Events1000["test"];
  BENCHMARK("1000 lambdas") {
    Test1000.emit();
  };
}

