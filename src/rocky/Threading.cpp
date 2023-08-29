/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Threading.h"
#include "IOTypes.h"
#include "Metrics.h"
#include "Utils.h"
#include "Instance.h"

#include <cstdlib>
#include <climits>
#include <mutex>
#include <iomanip>

#ifdef _WIN32
#   include <Windows.h>
//#   include <processthreadsapi.h>
#elif defined(__APPLE__) || defined(__LINUX__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__ANDROID__)
#   include <unistd.h>
#   include <sys/syscall.h>
#   include <pthread.h>
#endif

// b/c windows defines override std:: functions
#undef min
#undef max

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

//...................................................................

//#ifdef ROCKY_PROFILING
//#define MUTEX_TYPE tracy::Lockable<std::recursive_mutex>
//#else
//#define MUTEX_TYPE std::recursive_mutex
//#endif

#ifdef ROCKY_MUTEX_CONTENTION_TRACKING

Mutex::Mutex() :
    _handle(nullptr),
    _metricsData(nullptr)
{
    if (Metrics::enabled())
    {
        tracy::SourceLocationData* s = new tracy::SourceLocationData();
        s->name = nullptr;
        s->function = "unnamed";
        s->file = __FILE__;
        s->line = __LINE__;
        s->color = 0;
        _handle = new tracy::Lockable<std::mutex>(s);
        _metricsData = s;
    }
    else
    {
        _handle = new std::mutex();
    }
}

Mutex::Mutex(const std::string& name, const char* file, std::uint32_t line) :
    _name(name),
    _handle(nullptr),
    _metricsData(nullptr)
{
    if (Metrics::enabled())
    {
        tracy::SourceLocationData* s = new tracy::SourceLocationData();
        s->name = nullptr;
        s->function = _name.c_str();
        s->file = file;
        s->line = line;
        s->color = 0;
        _handle = new tracy::Lockable<std::mutex>(s);
        _metricsData = s;
    }
    else
    {
        _handle = new std::mutex();
    }
}

Mutex::~Mutex()
{
    if (_metricsData)
        delete static_cast<tracy::Lockable<std::mutex>*>(_handle);
    else
        delete static_cast<std::mutex*>(_handle);
}

void
Mutex::setName(const std::string& name)
{
    _name = name;
    if (_metricsData)
    {
        tracy::SourceLocationData* s = static_cast<tracy::SourceLocationData*>(_metricsData);
        s->function = _name.c_str();
    }
}

void
Mutex::lock()
{
    //if (_name.empty()) {
    //    volatile int x =0 ; // breakpoint for finding unnamed mutexes
    //}

    if (_metricsData)
        static_cast<tracy::Lockable<std::mutex>*>(_handle)->lock();
    else
        static_cast<std::mutex*>(_handle)->lock();
}

void
Mutex::unlock()
{
    if (_metricsData)
        static_cast<tracy::Lockable<std::mutex>*>(_handle)->unlock();
    else
        static_cast<std::mutex*>(_handle)->unlock();
}

bool
Mutex::try_lock()
{
    if (_metricsData)
        return static_cast<tracy::Lockable<std::mutex>*>(_handle)->try_lock();
    else
        return static_cast<std::mutex*>(_handle)->try_lock();
}

#endif // ROCKY_MUTEX_CONTENTION_TRACKING

//...................................................................

#ifdef ROCKY_MUTEX_CONTENTION_TRACKING

RecursiveMutex::RecursiveMutex() :
    _enabled(true),
    _metricsData(nullptr)
{
    if (Metrics::enabled())
    {
        tracy::SourceLocationData* s = new tracy::SourceLocationData();
        s->name = nullptr;
        s->function = "unnamed recursive";
        s->file = __FILE__;
        s->line = __LINE__;
        s->color = 0;
        _handle = new tracy::Lockable<std::recursive_mutex>(s);
        _metricsData = s;
    }
    else
    {
        _handle = new std::recursive_mutex();
    }
}

RecursiveMutex::RecursiveMutex(const std::string& name, const char* file, std::uint32_t line) :
    _name(name),
    _enabled(true),
    _metricsData(nullptr)
{
    if (Metrics::enabled())
    {
        tracy::SourceLocationData* s = new tracy::SourceLocationData();
        s->name = nullptr;
        s->function = _name.c_str();
        s->file = file;
        s->line = line;
        s->color = 0;
        _handle = new tracy::Lockable<std::recursive_mutex>(s);
        _metricsData = s;
    }
    else
    {
        _handle = new std::recursive_mutex();
    }
}

RecursiveMutex::~RecursiveMutex()
{
    if (_handle)
    {
        if (_metricsData)
            delete static_cast<tracy::Lockable<std::recursive_mutex>*>(_handle);
        else
            delete static_cast<std::recursive_mutex*>(_handle);
    }
}

void
RecursiveMutex::disable()
{
    _enabled = false;
}

void
RecursiveMutex::setName(const std::string& name)
{
    _name = name;

    if (_metricsData)
    {
        tracy::SourceLocationData* s = static_cast<tracy::SourceLocationData*>(_metricsData);
        s->function = _name.c_str();
    }
}

void
RecursiveMutex::lock()
{
    if (_enabled)
    {
        if (_metricsData)
            static_cast<tracy::Lockable<std::recursive_mutex>*>(_handle)->lock();
        else
            static_cast<std::recursive_mutex*>(_handle)->lock();
    }
}

void
RecursiveMutex::unlock()
{
    if (_enabled)
    {
        if (_metricsData)
            static_cast<tracy::Lockable<std::recursive_mutex>*>(_handle)->unlock();
        else
            static_cast<std::recursive_mutex*>(_handle)->unlock();
    }
}

bool
RecursiveMutex::try_lock()
{
    if (_enabled)
    {
        if (_metricsData)
            return static_cast<tracy::Lockable<std::recursive_mutex>*>(_handle)->try_lock();
        else
            return static_cast<std::recursive_mutex*>(_handle)->try_lock();
    }
    else return true;
}

#endif // ROCKY_MUTEX_CONTENTION_TRACKING

//...................................................................

unsigned rocky::util::getConcurrency()
{
    int value = std::thread::hardware_concurrency();
    return value > 0 ? (unsigned)value : 4u;
}

void
rocky::util::setThreadName(const std::string& name)
{
#if (defined _WIN32 && defined _WIN32_WINNT_WIN10 && defined _WIN32_WINNT && _WIN32_WINNT >= _WIN32_WINNT_WIN10) || (defined __CYGWIN__)
    wchar_t buf[256];
    mbstowcs(buf, name.c_str(), 256);

    // Look up the address of the SetThreadDescription function rather than using it directly.
    typedef ::HRESULT(WINAPI* SetThreadDescription)(::HANDLE hThread, ::PCWSTR lpThreadDescription);
    auto set_thread_description_func = reinterpret_cast<SetThreadDescription>(::GetProcAddress(::GetModuleHandle("Kernel32.dll"), "SetThreadDescription"));
    if (set_thread_description_func)
    {
        set_thread_description_func(::GetCurrentThread(), buf);
    }

#elif defined _GNU_SOURCE && !defined __EMSCRIPTEN__ && !defined __CYGWIN__

    const auto sz = strlen(name.c_str());
    if (sz <= 15)
    {
        pthread_setname_np(pthread_self(), name.c_str());
    }
    else
    {
        char buf[16];
        memcpy(buf, name.c_str(), 15);
        buf[15] = '\0';
        pthread_setname_np(pthread_self(), buf);
    }
#endif
}





#undef LC
#define LC "[Semaphore]"

Semaphore::Semaphore() :
    _count(0)
{
    //nop
}

Semaphore::Semaphore(const std::string& name) :
    _count(0)
{
    //nop
}

void
Semaphore::acquire()
{
    std::scoped_lock lock(_m);
    ++_count;
}

void
Semaphore::release()
{
    std::scoped_lock lock(_m);
    _count = std::max(_count - 1, 0);
    if (_count == 0)
        _cv.notify_all();
}

void
Semaphore::reset()
{
    std::scoped_lock lock(_m);
    _count = 0;
    _cv.notify_all();
}

std::size_t
Semaphore::count() const
{
    std::scoped_lock lock(_m);
    return _count;
}

void
Semaphore::join()
{
    std::scoped_lock lock(_m);
    _cv.wait(
        _m,
        [this]()
        {
            return _count == 0;
        }
    );
}

void
Semaphore::join(Cancelable& p)
{
    std::scoped_lock lock(_m);
    _cv.wait_for(
        _m,
        std::chrono::milliseconds(1),
        [this, &p]() {
            return
                (_count == 0) ||
                (p.canceled());
        }
    );
    _count = 0;
}


#undef LC
#define LC "[job_group]"

job_group::job_group() :
    _sema(std::make_shared<Semaphore>())
{
    //nop
}

job_group::job_group(const std::string& name) :
    _sema(std::make_shared<Semaphore>(name))
{
    //nop
}

void
job_group::join()
{
    if (_sema != nullptr && _sema.use_count() > 1)
    {
        _sema->join();
    }
}

void
job_group::join(Cancelable& p)
{
    if (_sema != nullptr && _sema.use_count() > 1)
    {
        _sema->join(p);
    }
}


#undef LC
#define LC "[job] "


void
job::scheduler_dispatch(std::function<bool()> delegate, const job& config)
{
    job_scheduler* scheduler = config.scheduler ? config.scheduler : job_scheduler::get("");

    if (scheduler)
        scheduler->dispatch(config, delegate);
}

#undef LC
#define LC "[job_scheduler] "

// job_scheduler statics:
//std::mutex job_scheduler::_schedulers_mutex;
//std::unordered_map<std::string, std::shared_ptr<job_scheduler>> job_scheduler::_schedulers;
//std::unordered_map<std::string, unsigned> job_scheduler::_schedulersizes;
//job_metrics job_metrics::_singleton;

#define ROCKY_SCHEDULER_DEFAULT_SIZE 2u

job_scheduler::job_scheduler(const std::string& name, unsigned concurrency) :
    _name(name),
    _targetConcurrency(concurrency),
    _done(false)
{
    // find a slot in the stats
    _metrics = &job_metrics::_singleton.scheduler(_name);
    startThreads();
}

job_scheduler::~job_scheduler()
{
    stopThreads();
}

void
job_scheduler::shutdownAll()
{
    std::scoped_lock lock(_schedulers_mutex);
    //Log::info() << LC << "Shutting down all job schedulers." << std::endl;
    _schedulers.clear();
}

job_scheduler*
job_scheduler::get(const std::string& name)
{
    std::scoped_lock lock(_schedulers_mutex);

    std::shared_ptr<job_scheduler>& sched = _schedulers[name];
    if (sched == nullptr)
    {
        auto iter = _schedulersizes.find(name);
        unsigned numThreads = iter != _schedulersizes.end() ? iter->second : ROCKY_SCHEDULER_DEFAULT_SIZE;

        sched = std::make_shared<job_scheduler>(name, numThreads);
    }
    return sched.get();
}

unsigned
job_scheduler::getConcurrency() const
{
    return _targetConcurrency;
}

void
job_scheduler::setConcurrency(unsigned value)
{
    value = std::max(value, 1u);

    if (_targetConcurrency != value)
    {
        _targetConcurrency = value;
        startThreads();
    }
}
void
job_scheduler::setConcurrency(const std::string& name, unsigned value)
{
    // this method exists so you can set an scheduler's concurrency
    // before the scheduler is actually created

    value = std::max(value, 1u);

    std::scoped_lock lock(_schedulers_mutex);
    if (_schedulersizes[name] != value)
    {
        _schedulersizes[name] = value;

        auto iter = _schedulers.find(name);
        if (iter != _schedulers.end())
        {
            std::shared_ptr<job_scheduler> scheduler = iter->second;
            ROCKY_SOFT_ASSERT_AND_RETURN(scheduler != nullptr, void());
            scheduler->setConcurrency(value);
        }
    }
}

void
job_scheduler::cancelAll()
{
    std::unique_lock lock(_queueMutex);
    _queue.clear();
    _metrics->canceled += _metrics->pending;
    _metrics->pending = 0;
}

void
job_scheduler::dispatch(const job& job, std::function<bool()>& delegate)
{
    // If we have a group semaphore, acquire it BEFORE queuing the job
    job_group* group = job.group;
    std::shared_ptr<Semaphore> sema = group ? group->_sema : nullptr;
    if (sema)
    {
        sema->acquire();
    }

    if (_targetConcurrency > 0)
    {
        std::unique_lock lock(_queueMutex);
        if (!_done)
        {
            _queue.emplace_back(job, delegate, sema);
            _metrics->pending++;
            _block.notify_one();
        }
    }
    else
    {
        // no threads? run synchronously.
        delegate();

        if (sema)
        {
            sema->release();
        }
    }
}

void
job_scheduler::run()
{
    // cap the number of jobs to run (applies to TRAVERSAL modes only)
    int jobsLeftToRun = INT_MAX;

    while (!_done)
    {
        QueuedJob next;

        bool have_next = false;
        {
            std::unique_lock lock(_queueMutex);
            
            _block.wait(lock, [this] {
                return _queue.empty() == false || _done == true;
                });

            if (!_queue.empty() && !_done)
            {
                // Find the highest priority item in the queue.
                // Note: We could use std::partial_sort or std::nth_element,
                // but benchmarking proves that a simple brute-force search
                // is always the fastest.
                // (Benchmark: https://stackoverflow.com/a/20365638/4218920)
                // Also note: it is indeed possible for the results of 
                // priority() to change during the search. We don't care.
                int index = -1;
                float highest_priority = -FLT_MAX;
                for (unsigned i = 0; i < _queue.size(); ++i)
                {
                    float priority = _queue[i]._job.priority != nullptr ?
                        _queue[i]._job.priority() :
                        0.0f;

                    if (index < 0 || priority > highest_priority)
                    {
                        index = i;
                        highest_priority = priority;
                    }
                }
                if (index < 0)
                    index = 0;

                next = std::move(_queue[index]);
                have_next = true;
                
                // move the last element into the empty position:
                if (index < _queue.size()-1)
                {
                    _queue[index] = std::move(_queue.back());
                }

                // and remove the last element.
                _queue.erase(_queue.end() - 1);
            }
        }

        if (have_next)
        {
            _metrics->running++;
            _metrics->pending--;

            auto t0 = std::chrono::steady_clock::now();

            bool job_executed = next._delegate();

            auto duration = std::chrono::steady_clock::now() - t0;

            if (job_executed)
            {
                jobsLeftToRun--;
            }
            else
            {
                _metrics->canceled++;
                //Log::info() << "Job " << next._job.name << " canceled" << std::endl;
            }

            // release the group semaphore if necessary
            if (next._groupsema != nullptr)
            {
                next._groupsema->release();
            }

            _metrics->running--;
        }

        // See if we no longer need this thread because the
        // target concurrency has been reduced
        std::scoped_lock lock(_quitMutex);

        if (_targetConcurrency < _metrics->concurrency)
        {
            _metrics->concurrency--;
            break;
        }
    }
}

void
job_scheduler::startThreads()
{
    _done = false;

    //Log::info() << LC << "Scheduler \"" << _name << "\" concurrency=" << _targetConcurrency << std::endl;

    // Not enough? Start up more
    while(_metrics->concurrency < _targetConcurrency)
    {
        _metrics->concurrency++;

        _threads.push_back(std::thread([this]
            {
                util::setThreadName(_name.c_str());
                run();
            }
        ));
    }
}

void job_scheduler::stopThreads()
{
    _done = true;

    // Clear out the queue
    {
        std::unique_lock lock(_queueMutex);

        // reset any group semaphores so that job_group.join()
        // will not deadlock.
        for (auto& queuedjob : _queue)
        {
            if (queuedjob._groupsema != nullptr)
            {
                queuedjob._groupsema->reset();
            }
        }
        _queue.clear();

        // wake up all threads so they can exit
        _block.notify_all();
    }

    // wait for them to exit
    for (unsigned i = 0; i < _threads.size(); ++i)
    {
        if (_threads[i].joinable())
        {
            _threads[i].join();
        }
    }

    _threads.clear();
}

const std::vector<std::shared_ptr<job_metrics::scheduler_metrics>>&
job_metrics::get()
{
    return _singleton._schedulers;
}

job_metrics::job_metrics() :
    max_index(-1)
{
    // to prevent thread safety issues
    _schedulers.resize(128);
}

job_metrics::scheduler_metrics&
job_metrics::scheduler(const std::string& name)
{
    for (int i = 0; i < _schedulers.size(); ++i)
    {
        if (_schedulers[i] && _schedulers[i]->name == name)
        {
            return *_schedulers[i];
        }
    }

    int m = ++max_index;

    if (m >= _schedulers.size())
    {
        ROCKY_SOFT_ASSERT(m >= _schedulers.size(), "Ran out of scheduler space...using scheduler[0] :(");
        return *_schedulers[0];
    }

    _schedulers[m] = std::make_shared<scheduler_metrics>();
    _schedulers[m]->name = name;
    return *_schedulers[m];
}

job_metrics::scheduler_metrics&
job_metrics::scheduler(int index)
{
    return index <= max_index ? *_schedulers[index] : *_schedulers[0];
}

int
job_metrics::totalJobsPending() const
{
    int count = 0;
    for (int i = 0; i <= max_index; ++i)
        count += _schedulers[i]->pending;
    return count;
}

int
job_metrics::totalJobsRunning() const
{
    int count = 0;
    for (int i = 0; i <= max_index; ++i)
        count += _schedulers[i]->running;
    return count;
}

int
job_metrics::totalJobsCanceled() const
{
    int count = 0;
    for (int i = 0; i <= max_index; ++i)
        count += _schedulers[i]->canceled;
    return count;
}
