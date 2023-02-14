/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <type_traits>
#include <mutex>

// to include the file and line as the mutex name
#define ROCKY_MUTEX_NAME __FILE__ ":" ROCKY_STRINGIFY(__LINE__)

// uncomment to activate mutex contention tracking when profiling
// #define ROCKY_MUTEX_CONTENTION_TRACKING

namespace ROCKY_NAMESPACE { namespace util
{
    /**
     * Gets the approximate number of available threading contexts.
     * Result is guaranteed to be greater than zero
     */
    extern ROCKY_EXPORT unsigned getConcurrency();

    /**
     * Gets the unique ID of the running thread.
     */
    //extern ROCKY_EXPORT unsigned getCurrentThreadId();

    /**
     * Event with a binary signaled state, for multi-threaded sychronization.
     *
     * The event has two states:
     *  "set" means that a call to wait() will not block;
     *  "unset" means that calls to wait() will block until another thread calls set().
     *
     * The event starts out unset.
     *
     * Typical usage: Thread A creates Thread B to run asynchronous code. Thread A
     * then calls wait(), which blocks Thread A. When Thread B is finished, it calls
     * set(). Thread A then wakes up and continues execution.
     *
     * NOTE: ALL waiting threads will wake up when the Event is cleared.
     */
    class ROCKY_EXPORT Event
    {
    public:
        //! Construct a new event
        Event();

        //! DTOR
        ~Event();

        //! Block until the event is set, then return true.
        bool wait();

        //! Block until the event is set or the timout expires.
        //! Return true if the event has set, otherwise false.
        bool wait(unsigned timeout_ms);

        //! Like wait(), but resets the state and always returns true.
        bool waitAndReset();

        //! Set the event state, causing any waiters to unblock.
        void set();

        //! Reset (unset) the event state; new waiters will block until set() is called.
        void reset();

        //! Whether the event state is set (waiters will not block).
        inline bool isSet() const { return _set; }

    protected:
        std::mutex _m; // do not use Mutex, we never want tracking
        std::condition_variable_any _cond;
        bool _set;
    };

    /**
     * Future is the consumer-side interface to an asynchronous operation.
     *
     * Usage:
     *   Producer (usually an asynchronous function call) creates a Promise<T>
     *   and immediately returns promise.getFuture(). The Consumer then performs other
     *   work, and eventually (or immediately) checks isAvailable() for a result or
     *   isAbandoned() for cancelation.
     */
    template<typename T>
    class Future : public Cancelable
    {
    public:
        using Callback = std::function<void(const T&)>;

    private:
        // internal structure to track referenced to the result
        struct Container {
            Container() { }
            void set(const T& obj) { 
                std::scoped_lock lock(_m);
                _obj = obj;
            }
            void set(T&& obj) {
                std::scoped_lock lock(_m);
                _obj = std::move(obj);
            }
            const T& obj() const {
                std::scoped_lock lock(_m);
                return _obj;
            }
            T _obj;
            mutable std::mutex _m;
        };

    public:
        //! Blank CTOR
        Future() {
            _ev = std::make_shared<Event>();
            _shared = std::make_shared<Container>();
        }

        //! Copy CTOR
        Future(const Future& rhs) :
            _ev(rhs._ev),
            _shared(rhs._shared) { }

        //! Assignment
        Future<T>& operator = (const Future<T>& rhs) {
            _ev = rhs._ev;
            _shared = rhs._shared;
            return *this;
        }

        //! True if the promise was resolved and a result if available.
        bool available() const {
            return _ev->isSet();
        }

        //! True if the Promise that generated this Future no longer exists
        //! and the Promise was never resolved; or if this Future was never
        //! associated with a Promise in the first place.
        bool abandoned() const {
            return !available() && _shared.use_count() == 1;
        }

        //! True is this Future is unused or abandoned and contains no data.
        bool idle() const {
            return !available() && _shared.use_count() == 1;
        }

        //! True if a Promise exists, but has not yet been fulfilled.
        bool working() const {
            return !available() && !abandoned();
        }

        //! Synonym for isAbandoned. (Canceleble interface)
        bool canceled() const override {
            return abandoned();
        }

        //! Deference the result object. Make sure you check isAvailable()
        //! to check that the future was actually resolved; otherwise you
        //! will just get the default object.
        T get() const {
            return _shared->obj();
        }

        //! Alias for get()
        const T* operator -> () const {
            return &(_shared->obj());
        }

        //! Same as get(), but if the result is available will reset the
        //! future before returning the result object.
        T release() {
            bool avail = available();
            T result = get();
            if (avail)
                reset();
            return result;
        }

        //! Blocks until the result becomes available or the future is abandoned;
        //! then returns the result object.
        T join() const {
            while (
                !abandoned() &&
                !_ev->wait(1u));
            return get();
        }

        //! Blocks until the result becomes available or the future is abandoned
        //! or a cancelation flag is set; then returns the result object.
        T join(const Cancelable& p) const {
            while (
                !available() &&
                !abandoned() &&
                !(p.canceled()))
            {
                _ev->wait(1u);
            }
            return get();
        }

        //! Release reference to a promise, resetting this future to its default state
        void abandon() {
            _shared.reset(new Container());
            _ev.reset(new Event());
        }

        //! synonym for abandon.
        void reset() {
            abandon();
        }

        //! The number of objects, including this one, that
        //! refernece the shared container. if this method
        //! returns 1, that means this is the only object with
        //! access to the data. This method will never return zero.
        unsigned refs() const {
            return _shared.use_count();
        }

        //! Function to execute upon a Promise resolving this Future.
        void whenAvailable(const Callback& cb) {
            _whenAvailable = cb;
        }

    private:
        std::shared_ptr<Event> _ev;
        std::shared_ptr<Container> _shared;
        Callback _whenAvailable;
        template<typename U> friend class Promise;
    };

    /**
     * Promise is the producer-side interface to an asynchronous operation.
     *
     * Usage: The code that initiates an asychronous operation creates a Promise
     *   object, dispatches the asynchronous code, and immediately returns
     *   Promise.getFuture(). The caller can then call future.get() to block until
     *   the result is available.
     */
    template<typename T>
    class Promise : public Cancelable
    {
    public:
        Promise() { }

        //! This promise's future result.
        Future<T> future() const { return _future; }

        //! Resolve (fulfill) the promise with the provided result value.
        void resolve(const T& value) {
            _future._shared->set(value);
            _future._ev->set();
            if (_future._whenAvailable)
                _future._whenAvailable(_future.get());
        }

        //! Resolve (fulfill) the promise with a default result.
        void resolve() {
            _future._ev->set();
            if (_future._whenAvailable)
                _future._whenAvailable(_future.get());
        }

        //! True if the promise is resolved and the Future holds a valid result.
        bool resolved() const {
            return _future._ev->isSet();
        }

        //! True is there are no Future objects waiting on this Promise.
        bool abandoned() const {
            return _future._shared.use_count() == 1;
        }

        bool canceled() const override {
            return abandoned();
        }

    private:
        Future<T> _future;
    };

    /**
    * Mutex that locks on a per-object basis
    */
    template<typename T>
    class Gate
    {
    public:
        inline void lock(const T& key) {
            std::unique_lock lock(_m);
            auto thread_id = std::this_thread::get_id(); // getCurrentThreadId();
            for (;;) {
                auto i = _keys.emplace(key, thread_id);
                if (i.second)
                    return;
                ROCKY_HARD_ASSERT(i.first->second != thread_id, "Recursive Gate access attempt");
                _unlocked.wait(lock);
            }
        }

        inline void unlock(const T& key) {
            std::unique_lock lock(_m);
            _keys.erase(key);
            _unlocked.notify_all();
        }

    private:
        std::mutex _m;
        std::condition_variable_any _unlocked;
        std::unordered_map<T,std::thread::id> _keys;
    };

    //! Gate the locks for the duration of this object's scope
    template<typename T>
    struct ScopedGate {
    public:
        //! Lock a gate based on key "key"
        ScopedGate(Gate<T>& gate, const T& key) : 
            _gate(gate), 
            _key(key), 
            _active(true)
        {
            _gate.lock(key);
        }

        //! Lock a gate based on key "key" IFF the predicate is true,
        //! else it's a nop.
        ScopedGate(Gate<T>& gate, const T& key, std::function<bool()> pred) :
            _gate(gate),
            _key(key),
            _active(pred())
        {
            if (_active)
                _gate.lock(_key);
        }

        //! End-of-scope destructor unlocks the gate
        ~ScopedGate()
        {
            if (_active)
                _gate.unlock(_key);
        }

    private:
        Gate<T>& _gate;
        T _key;
        bool _active;
    };

    //! Sets the name of the curent thread
    extern ROCKY_EXPORT void setThreadName(const std::string& name);

    /**
     * Sempahore lets N users aquire it and then notifies when the
     * count goes back down to zero.
     */
    class Semaphore
    {
    public:
        //! Construct a semaphore
        Semaphore();

        //! Construct a named semaphore
        Semaphore(const std::string& name);

        //! Acquire, increasing the usage count by one
        void acquire();

        //! Release, decreasing the usage count by one.
        //! When the count reaches zero, joiners will be notified and
        //! the semaphore will reset to its initial state.
        void release();

        //! Reset to initialize state; this will cause a join to occur
        //! even if no acquisitions have taken place.
        void reset();

        //! Current count in the semaphore
        std::size_t count() const;

        //! Block until the semaphore count returns to zero.
        //! (It must first have left zero)
        //! Warning: this method will block forever if the count
        //! never reaches zero!
        void join();

        //! Block until the semaphore count returns to zero, or
        //! the operation is canceled.
        //! (It must first have left zero)
        void join(Cancelable& cancelable);

    private:
        int _count;
        std::condition_variable_any _cv;
        mutable std::mutex _m;
    };


    /** Per-thread data store */
    template<class T>
    struct ThreadLocal : public std::mutex
    {
        T& get() {
            std::scoped_lock lock(*this);
            return _data[std::this_thread::get_id()];
        }

        void clear() {
            std::scoped_lock lock(*this);
            _data.clear();
        }

        using container_t = typename std::unordered_map<std::thread::id, T>;
        using iterator = typename container_t::iterator;

        // NB. lock before using these!
        iterator begin() { return _data.begin(); }
        iterator end() { return _data.end(); }

    private:
        container_t _data;
    };

    class job_scheduler;

    /**
     * A job group. Dispatch jobs along with a group, and you 
     * can then wait on the entire group to finish.
     */
    class ROCKY_EXPORT job_group
    {
    public:
        //! Construct a new job group
        job_group();

        //! Construct a new named job group
        job_group(const std::string& name);

        //! Block until all jobs dispatched under this group are complete.
        void join();

        //! Block until all jobs dispatched under this group are complete,
        //! or the operation is canceled.
        void join(Cancelable&);

    private:
        std::shared_ptr<Semaphore> _sema;
        friend class job_scheduler;
    };

    /**
     * API for scheduling a task to run in the background.
     *
     * Example usage:
     *
     *   int a = 10, b = 20;
     *   
     *   Future<int> result = job::dispatch<int>(
     *      [a, b](Cancelable& p) {
     *          return (a + b);
     *      }
     *   );
     *
     *   // later...
     *
     *   if (result.available()) {
     *       std::cout << "Answer = " << result.get() << std::endl;
     *   }
     *   else if (result.abandoned()) {
     *       // task was canceled
     *   }
     * 
     * For more control over the job, you can also pass in settings:
     * 
     *   auto result = job::dispatch<int>(
     *     [a, b](Cancelable& p) { return a+b; },
     *     { "Job name",               // name of this job
     *       []() { return 1.0f; },    // function returning jbo priority
     *       job_scheduler::get("adders"),  // which arena (thread pool) to run job in
     *       "job group name"          // group to run job in
     *     });
     *       
     */
    struct job
    {
        std::string name;
        std::function<float()> priority = nullptr;
        job_scheduler* scheduler = nullptr;
        job_group* group = nullptr;

        //! Run the job and return a future result.
        //! @param func Function to run in a thread
        //! @param settings Optional configuration for the asynchronous function call
        //! @return Future result of the async function call
        template<typename T> static inline Future<T> dispatch(
            std::function<T(Cancelable&)> task,
            const job& config = { })
        {
            Promise<T> promise;
            return dispatch(task, promise, config);
        }

        //! Run the job and return a future result.
        //! @param func Function to run in a thread
        //! @param promise User-supplied promise object
        //! @param settings Optional configuration for the asynchronous function call
        //! @return Future result of the async function call
        template<typename T> static inline Future<T> dispatch(
            std::function<T(Cancelable&)> task,
            Promise<T> promise,
            const job& config = { })
        {
            Future<T> future = promise.future();

            job_scheduler::Delegate delegate = [task, promise]() mutable
            {
                bool good = !promise.abandoned();
                if (good && task)
                    promise.resolve(task(promise));
                return good;
            };
            job_scheduler* arena = config.scheduler ? config.scheduler : job_scheduler::get("");
            if (arena)
                arena->dispatch(config, delegate);
            return std::move(future);
        }
    };

    class job_metrics
    {
    public:
        //! Per-arena metrics.
        struct scheduler_metrics
        {
            std::string name;
            std::atomic_uint concurrency = 0u;
            std::atomic_uint pending = 0u;
            std::atomic_uint running = 0u;
            std::atomic_uint canceled = 0u;
        };

        std::atomic<int> max_index;

        //! get or create a scheduler entry and return its index
        scheduler_metrics& scheduler(const std::string& name);

        //! metrics about the arena at index "index"
        scheduler_metrics& scheduler(int index);

        //! Total number of pending jobs across all schedulers
        int totalJobsPending() const;

        //! Total number of running jobs across all schedulers
        int totalJobsRunning() const;

        //! Total number of canceled jobs across all schedulers
        int totalJobsCanceled() const;

        //! Total number of active jobs in the system
        int totalJobs() const {
            return totalJobsPending() + totalJobsRunning();
        }

    private:
        job_metrics();
        std::vector<std::unique_ptr<scheduler_metrics>> _schedulers;
        friend class job_scheduler;
        static job_metrics _singleton;
    };

    /**
     * Schedules asynchronous tasks on a thread pool.
     * You usually don't need to use this class directly.
     * Use Job::schedule() to queue a new job.
     */
    class ROCKY_EXPORT job_scheduler
    {
    public:
        //! Construct a new scheduler
        job_scheduler(
            const std::string& name = "",
            unsigned concurrency = 2u);

        //! Destroy
        ~job_scheduler();

        //! Set the concurrency of this job scheduler
        void setConcurrency(unsigned value);

        //! Get the target concurrency (thread count) 
        unsigned getConcurrency() const;

        //! Discard all queued jobs
        void cancelAll();

        using Delegate = std::function<bool()>;

        //! Schedule an asynchronous task on this scheduler
        //! Use job::dispatch to run jobs (usually no need to call this directly)
        //! @param job Job details
        //! @param delegate Function to execute
        void dispatch(const job& config, Delegate& delegate);

    public: // statics

        //! Access a named arena
        static job_scheduler* get(const std::string& name = {});

        //! Sets the concurrency of a named arena
        static void setConcurrency(const std::string& name, unsigned value);

        static void shutdownAll();

    private:

        //! Pulls queued jobs and runs them in whatever thread run() is called from.
        //! Runs in a loop until _done is set.
        void run();

        //! Spawn all threads in this scheduler
        void startThreads();

        //! Join and destroy all threads in this scheduler
        void stopThreads();

        struct QueuedJob {
            QueuedJob() { }
            QueuedJob(const job& job, const Delegate& delegate, std::shared_ptr<Semaphore> sema) :
                _job(job), _delegate(delegate), _groupsema(sema) { }
            job _job;
            Delegate _delegate;
            std::shared_ptr<Semaphore> _groupsema;
            bool operator < (const QueuedJob& rhs) const {
                float lp = _job.priority ? _job.priority() : -FLT_MAX;
                float rp = rhs._job.priority ? rhs._job.priority() : -FLT_MAX;
                return lp < rp;
            }
        };

        // pool name
        std::string _name;
        // queued operations to run asynchronously
        using Queue = std::vector<QueuedJob>;
        Queue _queue;
        // protect access to the queue
        mutable std::mutex _queueMutex;
        mutable std::mutex _quitMutex;
        // target number of concurrent threads in the pool
        std::atomic<unsigned> _targetConcurrency;
        // thread waiter block
        std::condition_variable_any _block;
        // set to true when threads should exit
        bool _done;
        // threads in the pool
        std::vector<std::thread> _threads;
        // pointer to the stats structure for this arena
        job_metrics::scheduler_metrics* _metrics = nullptr;

        static std::mutex _schedulers_mutex;
        static std::unordered_map<std::string, unsigned> _schedulersizes;
        static std::unordered_map<std::string, std::shared_ptr<job_scheduler>> _schedulers;

        friend struct job;
    };

} } // namepsace rocky::Threading

#define ROCKY_SCOPED_THREAD_NAME(base,name) rocky::util::ScopedThreadName _scoped_threadName(base,name);
