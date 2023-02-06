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
        //! and the Promise was never resolved.
        bool abandoned() const {
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
        Future<T> getFuture() const { return _future; }

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

    class JobArena;

    /**
     * A job group. Dispatch jobs along with a group, and you 
     * can then wait on the entire group to finish.
     */
    class ROCKY_EXPORT JobGroup
    {
    public:
        //! Construct a new job group
        JobGroup();

        //! Construct a new named job group
        JobGroup(const std::string& name);

        //! Block until all jobs dispatched under this group are complete.
        void join();

        //! Block until all jobs dispatched under this group are complete,
        //! or the operation is canceled.
        void join(Cancelable&);

    private:
        std::shared_ptr<Semaphore> _sema;
        friend class JobArena;
    };

    /**
     * API for scheduling a task to run in the background.
     *
     * Example usage:
     *
     *   int a = 10, b = 20;
     *
     *   Job job;
     *   
     *   Future<int> result = job.dispatch<int>(
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
     * @notes Once you call dispatch, you can discard the job,
     *        or you can keep it around and dispatch it again later.
     *        Any changes you make to a Job after dispatch will
     *        not affect the already-dispatched Job.
     */
    class Job
    {
    public:

        //! Construct a new blank job
        Job() :
            _arena(nullptr), _group(nullptr), _priority(0.0f) { }

        //! Construct a new job in the specified arena
        Job(JobArena* arena) :
            _arena(arena), _group(nullptr), _priority(0.0f) { }

        //! Construct a new job in the specified arena and job group
        Job(JobArena* arena, JobGroup* group) :
            _arena(arena), _group(group), _priority(0.0f) { }

        //! Name of this job
        void setName(const std::string& value) {
            _name = value;
        }
        const std::string& getName() const {
            return _name;
        }

        //! Set the job arena in which to run this Job.
        void setArena(JobArena* arena) {
            _arena = arena;
        }
        inline void setArena(const std::string& arenaName);

        //! Static priority
        void setPriority(float value) {
            _priority = value;
        }

        //! Function to call to get this job's priority
        void setPriorityFunction(const std::function<float()>& func) {
            _priorityFunc = func;
        }

        //! Get the priority of this job
        float getPriority() const {
            return _priorityFunc != nullptr ? _priorityFunc() : _priority;
        }

        //! Assign this job to a group
        void setGroup(JobGroup* group) {
            _group = group;
        }
        JobGroup* getGroup() const {
            return _group;
        }

        //! Dispatch this job for asynchronous execution.
        //! @func Function to execute
        //! @return Future result. If this objects goes out of scope,
        //!         the job will be canceled and may not run at all.
        template<typename T>
        Future<T> dispatch(
            std::function<T(Cancelable&)> func) const;

        //! Dispatch the job for asynchronous execution and forget
        //! about it. No return value.
        inline void dispatch(
            std::function<void(Cancelable&)> func) const;

    private:
        std::string _name;
        float _priority;
        JobArena* _arena;
        JobGroup* _group;
        std::function<float()> _priorityFunc;
        friend class JobArena;
    };


    /**
     * Schedules asynchronous tasks on a thread pool.
     * You usually don't need to use this class directly.
     * Use Job::schedule() to queue a new job.
     */
    class ROCKY_EXPORT JobArena
    {
    public:
        //! Type of Job Arena (thread pool versus traversal)
        enum Type
        {
            THREAD_POOL,
            UPDATE_TRAVERSAL
        };

        //! Construct a new JobArena
        JobArena(
            const std::string& name = "",
            unsigned concurrency = 2u,
            const Type& type = THREAD_POOL);

        //! Destroy
        ~JobArena();

        //! Set the concurrency of this job arena
        //! (Only applies to THREAD_POOL type arenas)
        void setConcurrency(unsigned value);

        //! Get the target concurrency (thread count) 
        unsigned getConcurrency() const;

        //! Discard all queued jobs
        void cancelAll();

    public: // statics

        //! Access a named arena
        static JobArena* get(const std::string& name);

        //! Access the first arena of type "type". Typically use this to
        //! access an UPDATE_TRAVERSAL arena singleton.
        static JobArena* get(const Type& type);

        //! Sets the concurrency of a named arena
        static void setConcurrency(const std::string& name, unsigned value);

        //! Name of the arena to use when none is specified
        static const std::string& defaultArenaName();

    public:

        /**
         * Reflects the current state of the JobArena system
         * This structure is designed to be accessed atomically
         * with no lock contention
         */
        class ROCKY_EXPORT Metrics
        {
        public:
            //! Per-arena metrics.
            struct Arena
            {
                using Ptr = std::shared_ptr<Arena>;
                std::string arenaName;
                std::atomic_uint concurrency;
                std::atomic_uint numJobsPending;
                std::atomic_uint numJobsRunning;
                std::atomic_uint numJobsCanceled;

                Arena() : concurrency(0), numJobsPending(0), numJobsRunning(0), numJobsCanceled(0) { }
            };

            //! Report sent to the user reporting function if set
            struct Report
            {
                Report(const Job& job_, const std::string& arena_, const std::chrono::steady_clock::duration& duration_)
                    : job(job_), arena(arena_), duration(duration_) { }
                const Job& job;
                std::string arena;
                std::chrono::steady_clock::duration duration;
            };

            std::atomic<int> maxArenaIndex;

            //! create a new arena and return its index
            Arena::Ptr getOrCreate(const std::string& name);

            //! metrics about the arena at index "index"
            const Arena::Ptr arena(int index) const;

            //! Total number of pending jobs across all arenas
            int totalJobsPending() const;

            //! Total number of running jobs across all arenas
            int totalJobsRunning() const;

            //! Total number of canceled jobs across all arenas
            int totalJobsCanceled() const;

            //! Total number of active jobs in the system
            int totalJobs() const {
                return totalJobsPending() + totalJobsRunning();
            }

            //! Set a user reporting function and threshold
            void setReportFunction(
                std::function<void(const Report&)> function,
                std::chrono::steady_clock::duration minDuration = std::chrono::steady_clock::duration(0))
            {
                _report = function;
                _reportMinDuration = minDuration;
            }

        private:
            Metrics();
            std::vector<Arena::Ptr> _arenas;
            std::function<void(const Report&)> _report;
            std::chrono::steady_clock::duration _reportMinDuration;
            friend class JobArena;
        };

        //! Access to the system-wide job arena metrics
        static Metrics& allMetrics() { return _allMetrics; }

        //! Metrics object for thie arena
        Metrics::Arena::Ptr metrics() { return _metrics; }


    public: // INTERNAL

        //! Run one or more pending jobs.
        //! Internal function - do not call this directly.
        void runJobs();

    private:

        void startThreads();

        void stopThreads();

        static void shutdownAll();

        using Delegate = std::function<bool()>;

        //! Schedule an asynchronous task on this arena.
        //! Use Job::dispatch to run jobs (no need to call this directly)
        //! @param job Job details
        //! @param delegate Function to execute
        void dispatch(
            const Job& job,
            Delegate& delegate);

        struct QueuedJob {
            QueuedJob() { }
            QueuedJob(const Job& job, const Delegate& delegate, std::shared_ptr<Semaphore> sema) :
                _job(job), _delegate(delegate), _groupsema(sema) { }
            Job _job;
            Delegate _delegate;
            std::shared_ptr<Semaphore> _groupsema;
            bool operator < (const QueuedJob& rhs) const { 
                return _job.getPriority() < rhs._job.getPriority();
            }
        };

        // pool name
        std::string _name;
        // type of arena
        Type _type;
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
        Metrics::Arena::Ptr _metrics;

        static std::mutex _arenas_mutex;
        static std::unordered_map<std::string, unsigned> _arenaSizes;
        static std::unordered_map<std::string, std::shared_ptr<JobArena>> _arenas;
        static std::string _defaultArenaName;
        static Metrics _allMetrics;

        friend class Job; // allow access to private dispatch method
    };

    // Job inlines
    void Job::setArena(const std::string& arenaName)
    {
        setArena(JobArena::get(arenaName));
    }

    template<typename T>
    Future<T> Job::dispatch(std::function<T(Cancelable&)> function) const
    {
        Promise<T> promise;
        Future<T> future = promise.getFuture();
        JobArena::Delegate delegate = [function, promise]() mutable
        {
            bool good = !promise.abandoned();
            if (good)
                promise.resolve(function(&promise));
            return good;
        };
        JobArena* arena = _arena ? _arena : JobArena::get("");
        arena->dispatch(*this, delegate);
        return std::move(future);
    }

    void Job::dispatch(std::function<void(Cancelable& p)> function) const
    {
        JobArena* arena = _arena ? _arena : JobArena::get("");
        JobArena::Delegate delegate = [function]() {
            function(IOOptions());
            return true;
        };
        arena->dispatch(*this, delegate);
    }

} } // namepsace rocky::Threading

#define ROCKY_THREAD_NAME(name) rocky::util::setThreadName(name);

#define ROCKY_SCOPED_THREAD_NAME(base,name) rocky::util::ScopedThreadName _scoped_threadName(base,name);
