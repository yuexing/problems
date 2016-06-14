#include "jobs.hpp"

#include <gtest/gtest.h>

#include <thread>

class ThreadExecutor {
public:
  ThreadExecutor() : m_call(0) {}

  template <class Functor, class ...Params>
  void
  operator()(Functor &&func, Params&&... params) {
    m_call++;
    std::thread(std::forward<Functor>(func), std::forward<Params>(params)...).detach();
  }

  unsigned get_num_call() { return m_call; }

private:
  unsigned m_call;
};

class AsyncTest : public ::testing::Test {
protected:
  ThreadExecutor exec;
};

TEST_F(AsyncTest, MakeJobSyncSync) {
  tango::async::Job<int> job = tango::async::make_job(
    []() {
      return 5;
    });

  EXPECT_EQ(5, job().get());
  EXPECT_EQ(0, exec.get_num_call());
}

TEST_F(AsyncTest, MakeJobSyncAsync) {
  tango::async::Job<int> job = tango::async::make_job(
    &exec,
    []() {
      return 5;
    });

  EXPECT_EQ(5, job().get());
  EXPECT_EQ(1, exec.get_num_call());
}

TEST_F(AsyncTest, MakeJobAsyncSync) {
  auto &executor = exec;
  tango::async::Job<int> job = tango::async::make_job_from_async(
    [&executor](std::function<void(int)> done) {
      executor(done, 5);
    });

  EXPECT_EQ(5, job().get());
  EXPECT_EQ(1, exec.get_num_call());
}

TEST_F(AsyncTest, MakeJobAsyncAsync) {
  auto &executor = exec;
  tango::async::Job<int> job = tango::async::make_job_from_async(
    &exec,
    [&executor](std::function<void(int)> done) {
      executor(done, 5);
    });

  EXPECT_EQ(5, job().get());
  EXPECT_EQ(2, exec.get_num_call());
}

TEST_F(AsyncTest, Assign) {
  tango::async::Job<int, std::string> job = tango::async::make_job(
    [](std::string) { return 1; }
  );
}

// Test then
TEST_F(AsyncTest, ThenSyncSync) {
  tango::async::Job<std::string> job = tango::async::make_job(
    []() {
      return 5;
    }
  ).then(
    [](int num) {
      return std::to_string(num);
    }
  );

  EXPECT_EQ("5", job().get());
  EXPECT_EQ(0, exec.get_num_call());
}

TEST_F(AsyncTest, ThenSyncAsync) {
  tango::async::Job<std::string> job = tango::async::make_job(
    []() {
      return 5;
    }
  ).then(
    &exec,
    [](int num) {
      return std::to_string(num);
    }
  );

  EXPECT_EQ("5", job().get());
  EXPECT_EQ(1, exec.get_num_call());
}

TEST_F(AsyncTest, ThenAsyncSync) {
  auto &executor = exec;
  tango::async::Job<std::string> job = tango::async::make_job(
    []() {
      return 5;
    }
  ).then_async(
    [&executor](std::function<void(std::string)> done, int num) {
      return executor(done, std::to_string(num));
    }
  );

  EXPECT_EQ("5", job().get());
  EXPECT_EQ(1, exec.get_num_call());
}

TEST_F(AsyncTest, ThenAsyncAsync) {
  auto &executor = exec;
  tango::async::Job<std::string> job = tango::async::make_job(
    []() {
      return 5;
    }
  ).then_async(
    &exec,
    [&executor](std::function<void(std::string)> done, int num) {
      return executor(done, std::to_string(num));
    }
  );

  EXPECT_EQ("5", job().get());
  EXPECT_EQ(2, exec.get_num_call());
}

// Test exception handling
TEST_F(AsyncTest, ThrowSync)
{
  tango::async::Job<int> job = tango::async::make_job(
    []() -> int {
      throw std::logic_error("You're logic is bad!");
    });

  EXPECT_THROW(job().get(), std::logic_error);
}

TEST_F(AsyncTest, ThrowAsync)
{
  tango::async::Job<int> job = tango::async::make_job(
    &exec,
    []() -> int {
      throw std::logic_error("You're logic is bad!");
    });

  EXPECT_THROW(job().get(), std::logic_error);
}

TEST_F(AsyncTest, Void)
{
  ASSERT_NO_THROW(
    tango::async::make_job([]() {})
    .then([]() {})
    .then(&exec, []() {})
    .then_async([](std::function<void(int)> done) { done(5); })
    .then_async(&exec, [](std::function<void()> done, int num) { done(); })
    ().get()
  );

  ASSERT_NO_THROW(
    tango::async::make_job(&exec, []() {})
    .then([]() {})
    .then(&exec, []() {})
    .then_async([](std::function<void(int)> done) { done(5); })
    .then_async(&exec, [](std::function<void()> done, int num) { done(); })
    ().get()
  );

  ASSERT_NO_THROW(
    tango::async::make_job_from_async([](std::function<void()> done) { done(); })
    .then([]() {})
    .then(&exec, []() {})
    .then_async([](std::function<void(int)> done) { done(5); })
    .then_async(&exec, [](std::function<void()> done, int num) { done(); })
    ().get()
  );

  ASSERT_NO_THROW(
    tango::async::make_job_from_async(&exec, [](std::function<void()> done) { done(); })
    .then([]() {})
    .then(&exec, []() {})
    .then_async([](std::function<void(int)> done) { done(5); })
    .then_async(&exec, [](std::function<void()> done, int num) { done(); })
    ().get()
  );
}

TEST_F(AsyncTest, CatchError)
{
  auto job = tango::async::make_job_from_async(
    [](std::function<void()> done) { done(); }
  ).then(
    []() { return 1; }
  ).then(
    &exec,
    [](int num) -> int { throw num; }
  ).then_async( // This function is completely skipped
    [](std::function<void(int)> done, int num) { done(num + 1); }
  ).onException(
    [](int &num) { return num - 1;}
  ).then_async(
    &exec,
    [](std::function<void(int)> done, int num) { done(num * 2); }
  ).then(
    [](int num) -> int { throw num; }
  ).onException(
    &exec,
    [](int num) { return num - 2; }
  ).then(
    [](int num) -> void { throw 5; }
  ).onException(
    [](int num) { }
  ).onAllExceptions(
    []() { throw 100; }
  ).onAllExceptions(
    &exec,
    []() { }
  );

  ASSERT_NO_THROW(job().get());
}

template <typename T>
void print(T) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
}

TEST_F(AsyncTest, QueueJob)
{
  auto job = tango::async::make_job(
    []() -> void { throw 0; }
  ).then(
    tango::async::make_job(
      []() { return -1; }
    ).onAllExceptions( // This should not catch exceptions from outer scope
      []() { return 1; }
    )
  ).then(
    tango::async::make_job(
      [](int) -> std::string { throw 0; }
    ).onAllExceptions( // This should not catch exceptions from outer scope
      []() { return std::string("inner"); }
    )
  ).onAllExceptions(
    []() { return std::string("outer"); } // This should catch all exceptions (inner, outer)
  );

  ASSERT_EQ("outer", job().get());
}

struct NonCopiable {
  NonCopiable(int i) : i(i) {}
  NonCopiable(const NonCopiable &) = delete;
  NonCopiable(NonCopiable &&) = default;

  int i;
};


TEST_F(AsyncTest, NonCopiable)
{
  auto job = tango::async::make_job(
    [](NonCopiable &&nc) { return nc.i; }
  ).then(
    [](int i) { return NonCopiable(i); }
  ).then(
    [](NonCopiable &&nc) {
      return nc.i;
    }
  );

  EXPECT_EQ(5, job(NonCopiable(5)).get());
}
