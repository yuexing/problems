#pragma once

#include <functional>
#include <future>

namespace tango {
namespace async {

// Forward declaration
template <class, class ...> class Job;

namespace details {

// Synchronous function traits
template <typename T> struct function_traits : public function_traits<decltype(&T::operator())> {};

template <typename R, typename ...A>
struct function_traits<R(A...)> {
  using type = R(A...);
  using ret_type = R;
};

template<typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...)> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...) const> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...) volatile> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...) const volatile> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct function_traits<R(&)(A...)> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct function_traits<R(*)(A...)> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct function_traits<std::function<R(A...)>> : public function_traits<R(A...)> {};

// Asynchronous function traits
template <typename T> struct async_function_traits : public async_function_traits<decltype(&T::operator())> {};

template <typename R, typename ...A>
struct async_function_traits<void(std::function<void(R)>, A...)> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct async_function_traits<void(C::*)(std::function<void(R)>, A...)> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct async_function_traits<void(C::*)(std::function<void(R)>, A...) const> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct async_function_traits<void(C::*)(std::function<void(R)>, A...) volatile> : public function_traits<R(A...)> {};
template<typename C, typename R, typename... A>
struct async_function_traits<void(C::*)(std::function<void(R)>, A...) const volatile> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct async_function_traits<void(&)(std::function<void(R)>, A...)> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct async_function_traits<void(*)(std::function<void(R)>, A...)> : public function_traits<R(A...)> {};
template<typename R, typename... A>
struct async_function_traits<std::function<void(std::function<void(R)>, A...)>> : public function_traits<R(A...)> {};
template <typename ...A>
struct async_function_traits<void(std::function<void()>, A...)> : public function_traits<void(A...)> {};
template<typename C, typename... A>
struct async_function_traits<void(C::*)(std::function<void()>, A...)> : public function_traits<void(A...)> {};
template<typename C, typename... A>
struct async_function_traits<void(C::*)(std::function<void()>, A...) const> : public function_traits<void(A...)> {};
template<typename C, typename... A>
struct async_function_traits<void(C::*)(std::function<void()>, A...) volatile> : public function_traits<void(A...)> {};
template<typename C, typename... A>
struct async_function_traits<void(C::*)(std::function<void()>, A...) const volatile> : public function_traits<void(A...)> {};
template<typename... A>
struct async_function_traits<void(&)(std::function<void()>, A...)> : public function_traits<void(A...)> {};
template<typename... A>
struct async_function_traits<void(*)(std::function<void()>, A...)> : public function_traits<void(A...)> {};
template<typename... A>
struct async_function_traits<std::function<void(std::function<void()>, A...)>> : public function_traits<void(A...)> {};

template <typename T> struct error_handler_traits : public error_handler_traits<decltype(&T::operator())> {};

template <typename R, typename E>
struct error_handler_traits<R(E)> {
  using type = R(E);
  using error_type = E;
  using ret_type = R;
};

template<typename C, typename R, typename E>
struct error_handler_traits<R(C::*)(E)> : public error_handler_traits<R(E)> {};
template<typename C, typename R, typename E>
struct error_handler_traits<R(C::*)(E) const> : public error_handler_traits<R(E)> {};
template<typename C, typename R, typename E>
struct error_handler_traits<R(C::*)(E) volatile> : public error_handler_traits<R(E)> {};
template<typename C, typename R, typename E>
struct error_handler_traits<R(C::*)(E) const volatile> : public error_handler_traits<R(E)> {};
template<typename R, typename E>
struct error_handler_traits<R(&)(E)> : public error_handler_traits<R(E)> {};
template<typename R, typename E>
struct error_handler_traits<R(*)(E)> : public error_handler_traits<R(E)> {};
template<typename R, typename E>
struct error_handler_traits<std::function<R(E)>> : public error_handler_traits<R(E)> {};

template <typename ...> struct AppendNonVoidArg;
template <template <typename ...> class F, typename ...P, typename A> struct AppendNonVoidArg<F<P...>, A> {
  using type = F<P..., A>;
};
template <template <typename ...> class F, typename ...P> struct AppendNonVoidArg<F<P...>, void> {
  using type = F<P...>;
};

template <typename ...> struct AvoidVoidFunc;
template <typename F, typename ...P> struct AvoidVoidFunc<F, P...> {
  using type = F(P...);
};
template <typename F> struct AvoidVoidFunc<F, void> {
  using type = F();
};

template <class R, class ...A>
using NonVoidFunc = std::function<typename AvoidVoidFunc<R, A...>::type>;

template <class R>
using Callback = NonVoidFunc<void, R>;
using ErrorCallback = Callback<std::exception_ptr>;

// is_job
template <class T> class is_job : public std::false_type {};
template <class T, class ...U> class is_job<Job<T, U...>> : public std::true_type {};

template <class Ret, class ...Args>
struct Sync {
  template <class Functor>
  static void run(Functor &f, details::Callback<Ret> done, details::ErrorCallback error, Args &&...args) {
    try {
      done(f(std::forward<Args>(args)...));
    } catch (...) {
      error(std::current_exception());
    }
  }
};

template <class ...Args>
struct Sync<void, Args...> {
  template <class Functor>
  static void run(Functor &f, const details::Callback<void> &done, const details::ErrorCallback &error, Args &&...args) {
    try {
      f(std::forward<Args>(args)...);
      done();
    } catch (...) {
      error(std::current_exception());
    }
  }
};

template <class Ret, class Functor, class ...Args>
void async(Functor &f, const details::Callback<Ret> &done, const details::ErrorCallback &error, Args &&...args) {
  try {
    f(done, std::forward<Args>(args)...);
  } catch (...) {
    error(std::current_exception());
  }
}

template <class Ret, class ...Args>
struct Step {
  virtual ~Step() = default;
  virtual void operator()(details::Callback<Ret>, details::ErrorCallback, Args &&...) = 0;
};

template <class...> class SyncStep;

template <class Functor, class Ret, class ...Args>
class SyncStep<Functor, Ret(Args...)> : public Step<Ret, Args...> {
  Functor func_;

public:
  SyncStep(Functor &&func) : func_(std::move(func)) {}

  void operator()(details::Callback<Ret> done, details::ErrorCallback error, Args &&...args) override {
    Sync<Ret, Args...>::run(func_, done, error, std::forward<Args>(args)...);
  }
};

template <class...> class AsyncStep;

template <class Ret, class Functor, class ...Args>
class AsyncStep<Functor, Ret(Args...)> : public Step<Ret, Args...> {
  Functor func_;

public:
  AsyncStep(Functor &&functor) :
      func_(std::move(functor)) {}

  void operator()(details::Callback<Ret> done, details::ErrorCallback error, Args &&...args) override {
    async<Ret>(func_, done, error, std::forward<Args>(args)...);
  }
};

template <class Executor, class Functor, class Ret, class ...Args> class StepOnExecutor;

template <class Executor, class Functor, class Ret, class ...Args>
class StepOnExecutor<Executor, Functor, Ret(Args...)> : public Step<Ret, Args...> {
  Functor func_;
  Executor executor_;

public:
  StepOnExecutor(Executor executor, Functor &&func)
      : executor_(executor), func_(std::move(func)) {}

  void operator()(details::Callback<Ret> done, details::ErrorCallback error, Args &&...args) override {
    (*executor_)(std::move(func_), done, error, std::forward<Args>(args)...);
  }
};

template <class Functor, class NewRetType, class ...Old /* 1 or 0 */>
class SyncActionQueued {
  Functor func_;
  Callback<NewRetType> done_;
  ErrorCallback error_;

public:
  SyncActionQueued(Functor &&func, Callback<NewRetType> &&done, const ErrorCallback &error)
      : func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(Old &&...old) {
    Sync<NewRetType, Old...>::run(func_, done_, error_, std::forward<Old>(old)...);
  }
};

template <class Functor, class NewRetType, class ...Old /* 1 or 0 */>
class AsyncActionQueued {
  Functor func_;
  Callback<NewRetType> done_;
  ErrorCallback error_;

public:
  AsyncActionQueued(Functor &&func, Callback<NewRetType> &&done, const ErrorCallback &error)
      : func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(Old &&...old) {
    async<NewRetType>(func_, done_, error_, std::forward<Old>(old)...);
  }
};

template <class Executor, class Functor, class NewRetType, class ...Old>
class SyncActionQueuedExecutor {
  Executor executor_;
  Functor func_;
  Callback<NewRetType> done_;
  ErrorCallback error_;

public:
  SyncActionQueuedExecutor(Executor executor, Functor &&func, Callback<NewRetType> &&done, const ErrorCallback &error)
      : executor_(executor), func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(Old &&...old) {
    (*executor_)(SyncActionQueued<Functor, NewRetType, Old...>(std::move(func_), std::move(done_), error_), std::forward<Old>(old)...);
  }
};

template <class Executor, class Functor, class NewRetType, class ...Old>
class AsyncActionQueuedExecutor {
  Executor executor_;
  Functor func_;
  Callback<NewRetType> done_;
  ErrorCallback error_;

public:
  AsyncActionQueuedExecutor(Executor executor, Functor &&func, Callback<NewRetType> &&done, const ErrorCallback &error)
      : executor_(executor), func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(Old &&...old) {
    (*executor_)(AsyncActionQueued<Functor, NewRetType, Old...>(std::move(func_), std::move(done_), error_), std::forward<Old>(old)...);
  }
};

template <class Functor, class NewRet, class OldRet, class ...Args>
class SyncStepQueued : public Step<NewRet, Args...> {
  typedef std::shared_ptr<Step<OldRet, Args...>> Prev;
  Functor func_;
  Prev prev_;

public:
  SyncStepQueued(Functor &&func, Prev &&prev)
      : func_(std::move(func)), prev_(std::move(prev)) {}

  void operator()(Callback<NewRet> done, ErrorCallback error, Args &&...args) override {
    (*prev_)(
      typename AppendNonVoidArg<SyncActionQueued<Functor, NewRet>, OldRet>::type(std::move(func_), std::move(done), error),
      error,
      std::forward<Args>(args)...);
  }
};

template <class Functor, class NewRet, class OldRet, class ...Args>
class AsyncStepQueued : public Step<NewRet, Args...> {
  typedef std::shared_ptr<Step<OldRet, Args...>> Prev;
  Functor func_;
  Prev prev_;

public:
  AsyncStepQueued(Functor &&func, Prev &&prev)
      : func_(std::move(func)), prev_(std::move(prev)) {}

  void operator()(Callback<NewRet> done, ErrorCallback error, Args &&...args) override {
    (*prev_)(
      typename AppendNonVoidArg<AsyncActionQueued<Functor, NewRet>, OldRet>::type(std::move(func_), std::move(done), error),
      error,
      std::forward<Args>(args)...);
  }
};

template <class Executor, class Functor, class NewRet, class OldRet, class ...Args>
class SyncStepQueuedExecutor : public Step<NewRet, Args...> {
  typedef std::shared_ptr<Step<OldRet, Args...>> Prev;
  Executor executor_;
  Functor func_;
  Prev prev_;

public:
  SyncStepQueuedExecutor(Executor executor, Functor &&func, Prev &&prev)
      : executor_(executor), func_(std::move(func)), prev_(std::move(prev)) {}

  void operator()(Callback<NewRet> done, ErrorCallback error, Args &&...args) override {
    (*prev_)(
      typename AppendNonVoidArg<SyncActionQueuedExecutor<Executor, Functor, NewRet>, OldRet>::type(executor_, std::move(func_), std::move(done), error),
      error,
      std::forward<Args>(args)...);
  }
};

template <class Executor, class Functor, class NewRet, class OldRet, class ...Args>
class AsyncStepQueuedExecutor : public Step<NewRet, Args...> {
  typedef std::shared_ptr<Step<OldRet, Args...>> Prev;
  Executor executor_;
  Functor func_;
  Prev prev_;

public:
  AsyncStepQueuedExecutor(Executor executor, Functor &&func, Prev &&prev)
      : executor_(executor), func_(std::move(func)), prev_(std::move(prev)) {}

  void operator()(Callback<NewRet> done, ErrorCallback error, Args &&...args) override {
    (*prev_)(
      AsyncActionQueuedExecutor<Executor, Functor, NewRet, OldRet>(executor_, std::move(func_), std::move(done), error),
      error,
      std::forward<Args>(args)...);
  }
};

template <class NewRet, class ...Args>
class JobQueued {
  typedef std::shared_ptr<Step<NewRet, Args...>> Functor;
  Functor func_;
  Callback<NewRet> done_;
  ErrorCallback error_;

public:
  JobQueued(Functor &&func, Callback<NewRet> &&done, const ErrorCallback &error)
      : func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(Args &&...args) {
    (*func_)(done_, error_, std::forward<Args>(args)...);
  }
};

template <class NewRet, class OldRet, class ...Args>
class JobStepQueued : public Step<NewRet, Args...> {
  typedef std::shared_ptr<typename AppendNonVoidArg<Step<NewRet>, OldRet>::type> Functor;
  typedef std::shared_ptr<Step<OldRet, Args...>> Prev;
  Functor func_;
  Prev prev_;

public:
  JobStepQueued(Functor &&func, Prev &&prev)
      : func_(std::move(func)), prev_(std::move(prev)) {}

  void operator()(Callback<NewRet> done, ErrorCallback error, Args &&...args) override {
    (*prev_)(
      typename AppendNonVoidArg<JobQueued<NewRet>, OldRet>::type(std::move(func_), std::move(done), error),
      error,
      std::forward<Args>(args)...);
  }
};

template <class Ret, class Error>
class ExceptionHandler {
  typedef std::shared_ptr<Step<Ret, Error>> Functor;
  Functor func_;
  details::Callback<Ret> done_;
  details::ErrorCallback error_;

public:
  ExceptionHandler(Functor &&func, details::Callback<Ret> &&done, const details::ErrorCallback &error)
      : func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(std::exception_ptr exception) {
    try {
      std::rethrow_exception(exception);
    } catch (Error err) {
      (*func_)(done_, error_, static_cast<Error>(err));
    } catch (...) {
      error_(std::current_exception());
    }
  }
};

// Specialization for catch-all
template <class Ret>
class ExceptionHandler<Ret, void> {
  typedef std::shared_ptr<Step<Ret>> Functor;
  Functor func_;
  details::Callback<Ret> done_;
  details::ErrorCallback error_;

public:
  ExceptionHandler(Functor &&func, details::Callback<Ret> &&done, const details::ErrorCallback &error)
      : func_(std::move(func)), done_(std::move(done)), error_(error) {}

  void operator()(std::exception_ptr exception) {
    try {
      std::rethrow_exception(exception);
    } catch (...) {
      (*func_)(done_, error_);
    }
  }
};

template <class Error, class Ret, class ...Args>
class ExceptionHandlerStep : public Step<Ret, Args...> {
  typedef std::shared_ptr<typename AppendNonVoidArg<Step<Ret>, Error>::type> Functor;
  typedef std::shared_ptr<Step<Ret, Args...>> Prev;

  Functor func_;
  Prev prev_;

public:
  ExceptionHandlerStep(Functor &&func, Prev &&prev)
      : func_(std::move(func)), prev_(std::move(prev)){}

  void operator()(details::Callback<Ret> done, details::ErrorCallback error, Args &&... args) override {
    (*prev_)(
      std::move(done),
      ExceptionHandler<Ret, Error>(std::move(func_), std::move(done), error),
      std::forward<Args>(args)...);
  }
};

}

template <class RetType, class ...Params>
class Job {
private:
  typedef details::Callback<RetType> RetCallback;

  // Friends
  template <class, class ...> friend class Job;
  template <class F> friend Job<typename details::function_traits<F>::type> make_job(F&&);
  template <class F> friend Job<typename details::async_function_traits<F>::type> make_job_from_async(F&&);
  template <class E, class F> friend Job<typename details::function_traits<F>::type> make_job(E, F&&);
  template <class E, class F> friend Job<typename details::async_function_traits<F>::type> make_job_from_async(E, F&&);

  std::shared_ptr<details::Step<RetType, Params...>> job_;

protected:
  template <class Functor>
  Job(Functor &&func) : job_(new Functor(std::forward<Functor>(func))) {}

public:
  Job(Job<RetType(Params...)> &&job) : job_(std::move(job.job_)) {}

  template <typename R = RetType, typename std::enable_if<!std::is_void<R>::value, R>::type * = nullptr>
  std::future<RetType> operator()(Params &&...params)
  {
    auto p = std::make_shared<std::promise<RetType>>();
    auto f = p->get_future();

    (*this->job_)(
      [p](RetType ret){
        try {
          p->set_value(ret);
        } catch (const std::future_error &e) {
          p->set_exception(std::current_exception());
        }
      },
      [p](std::exception_ptr exception) {
        p->set_exception(exception);
      },
      std::forward<Params>(params)...);

    return f;
  }

  template <typename R = RetType, typename std::enable_if<std::is_void<R>::value, R>::type * = nullptr>
  std::future<void> operator()(Params &&...params)
  {
    auto p = std::make_shared<std::promise<void>>();
    auto f = p->get_future();

    (*this->job_)(
      [p](){
        try {
          p->set_value();
        } catch (const std::future_error &e) {
          p->set_exception(std::current_exception());
        }
      },
      [p](std::exception_ptr exception) {
        p->set_exception(exception);
      },
      std::forward<Params>(params)...);

    return f;
  }

  template <
    class Functor,
    typename std::enable_if<!details::is_job<Functor>::value>::type * = nullptr>
  Job<typename details::function_traits<Functor>::ret_type, Params...>
  then(Functor &&func)
  {
    typedef typename details::function_traits<Functor>::ret_type NewRet;

    return Job<NewRet, Params...>(
      details::SyncStepQueued<Functor, NewRet, RetType, Params...>(
        std::forward<Functor>(func),
        std::move(this->job_)));
  }

  template <
    class Functor,
    class Executor,
    typename std::enable_if<!details::is_job<Functor>::value>::type * = nullptr>
  Job<typename details::function_traits<Functor>::ret_type, Params...>
  then(Executor executor, Functor &&func)
  {
    typedef typename details::function_traits<Functor>::ret_type NewRet;

    return Job<NewRet, Params...>(
      details::SyncStepQueuedExecutor<Executor, Functor, NewRet, RetType, Params...>(
        executor,
        std::forward<Functor>(func),
        std::move(this->job_)));
  }

  // Queue actual jobs (useful if you want exceptions to not be treated
  // as exceptions from previous levels)
  template <class NewRet>
  Job<NewRet, Params...>
  then(Job<NewRet, RetType> other)
  {
    return Job<NewRet, Params...>(
      details::JobStepQueued<NewRet, RetType, Params...>(
        std::move(other.job_),
        std::move(this->job_)));
  }

  template <class NewRet>
  Job<NewRet, Params...>
  then(Job<NewRet> other)
  {
    return Job<NewRet, Params...>(
      details::JobStepQueued<NewRet, void, Params...>(
        std::move(other.job_),
        std::move(this->job_)));
  }

  // Async method overload
  template <
    class Functor,
    class NewRet = typename details::async_function_traits<Functor>::ret_type>
  Job<NewRet, Params...>
  then_async(Functor func)
  {
    return Job<NewRet, Params...>(
      details::AsyncStepQueued<Functor, NewRet, RetType, Params...>(
        std::forward<Functor>(func),
        std::move(this->job_)));
  }

  // Async method overload
  template <
    class Executor,
    class Functor,
    class NewRet = typename details::async_function_traits<Functor>::ret_type>
  Job<NewRet, Params...>
  then_async(Executor executor, Functor &&func)
  {
    return Job<NewRet, Params...>(
      details::AsyncStepQueuedExecutor<Executor, Functor, NewRet, RetType, Params...>(
        executor,
        std::forward<Functor>(func),
        std::move(this->job_)));
  }

  template <class Functor>
  Job onException(Functor &&handler)
  {
    typedef details::error_handler_traits<Functor> Error;
    typedef typename Error::error_type ErrorType;

    static_assert(
      std::is_same<RetType, typename Error::ret_type>::value,
      "Exception handler must return the same type as the job");

    return Job(
      details::ExceptionHandlerStep<ErrorType, RetType, Params...>(
        std::make_shared<details::SyncStep<Functor, RetType(ErrorType)>>(std::forward<Functor>(handler)),
        std::move(this->job_)));
  }

  template <class Functor, class Executor>
  Job onException(Executor executor, Functor &&handler)
  {
    typedef details::error_handler_traits<Functor> Error;
    typedef typename Error::error_type ErrorType;

    static_assert(
      std::is_same<RetType, typename Error::ret_type>::value,
      "Exception handler must return the same type as the job");

    return Job(
      details::ExceptionHandlerStep<ErrorType, RetType, Params...>(
        std::make_shared<details::StepOnExecutor<Executor, details::SyncStep<Functor, RetType(ErrorType)>, RetType(ErrorType)>>(executor, std::forward<Functor>(handler)),
        std::move(this->job_)));
  }

  template <class Functor>
  Job onAllExceptions(Functor &&handler)
  {
    static_assert(
      std::is_same<
        RetType,
        typename details::function_traits<Functor>::ret_type
      >::value,
      "Exception handler must return the same type as the job");

    return Job(
      details::ExceptionHandlerStep<void, RetType, Params...>(
        std::make_shared<details::SyncStep<Functor, RetType()>>(std::forward<Functor>(handler)),
        std::move(this->job_)));
  }

  template <class Functor, class Executor>
  Job onAllExceptions(Executor executor, Functor &&handler)
  {
    static_assert(
      std::is_same<
        RetType,
        typename details::function_traits<Functor>::ret_type
      >::value,
      "Exception must return the same type as the job");

    return Job(
      details::ExceptionHandlerStep<void, RetType, Params...>(
        std::make_shared<details::StepOnExecutor<Executor, details::SyncStep<Functor, RetType()>, RetType()>>(executor, std::forward<Functor>(handler)),
        std::move(this->job_)));
  }
};

template <class RetType, class ...Params>
class Job<RetType(Params...)> : public Job<RetType, Params...> {
  // Friends
  template <class E, class F> friend Job<typename details::function_traits<F>::type> make_job(E, F&&);
  template <class F> friend Job<typename details::function_traits<F>::type> make_job(F&&);
  template <class F> friend Job<typename details::async_function_traits<F>::type> make_job_from_async(F&&);
  template <class E, class F> friend Job<typename details::async_function_traits<F>::type> make_job_from_async(E, F&&);

public:
  template <class Functor> Job(Functor &&func) : Job<RetType, Params...>(std::forward<Functor>(func)) {}
};

template <class Functor>
inline Job<typename details::function_traits<Functor>::type>
make_job(
  Functor &&func)
{
  return Job<typename details::function_traits<Functor>::type>(
    details::SyncStep<Functor, typename details::function_traits<Functor>::type>(
      std::forward<Functor>(func)));
}

template <class Executor, class Functor>
inline Job<typename details::function_traits<Functor>::type>
make_job(
  Executor executor,
  Functor &&func)
{
  details::SyncStep<Functor, typename details::function_traits<Functor>::type> step(
    std::forward<Functor>(func));

  return Job<typename details::function_traits<Functor>::type>(
    details::StepOnExecutor<Executor, decltype(step), typename details::function_traits<Functor>::type>(
      executor,
      std::move(step)));
}

template <class Functor>
inline Job<typename details::async_function_traits<Functor>::type>
make_job_from_async(Functor &&func)
{
  return Job<typename details::async_function_traits<Functor>::type>(
    details::AsyncStep<Functor, typename details::async_function_traits<Functor>::type>(
      std::forward<Functor>(func)));
}

template <class Executor, class Functor>
inline Job<typename details::async_function_traits<Functor>::type>
make_job_from_async(Executor executor, Functor &&func)
{
  details::AsyncStep<Functor, typename details::async_function_traits<Functor>::type> step(
    std::forward<Functor>(func));

  return Job<typename details::async_function_traits<Functor>::type>(
    details::StepOnExecutor<Executor, decltype(step), typename details::async_function_traits<Functor>::type>(
      executor,
      std::move(step)));
}

}
}
