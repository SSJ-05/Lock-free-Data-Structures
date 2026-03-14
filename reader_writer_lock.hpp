// Reader-Writer Lock// 14.03.26// ZeroK
// Single Writer given precedence over other writers and readers
// this avoids readers reading stale data


#pragma once

#include <thread>
#include <condition_variable>
#include <semaphore>
#include <mutex>
#include <cstdint>

class RW_lock {
    private:
        std::mutex _m;
        std::condition_variable _reader_cv;
        std::condition_variable _writer_cv;

        bool _writing { false };
        std::uint64_t _readers { 0 };
        std::uint64_t _pending_writers { 0 };

    public:
        // reader will wait to acquire lock if writer is writing - avoid reading stale data
        // or if there are pending writers - waste of processing power reading 
        // stale data when data is about to be updated
        // readerlock returns number of readers
        std::uint64_t readerlock () {
            std::unique_lock lk (_m);
            _reader_cv.wait(lk, [this] { return !_writing && !_pending_writers; } );
            return ++_readers;
        }

        // notify pending writers to acquire lock once all readers are done reading
        std::uint64_t readerunlock () {
            std::unique_lock lk (_m);
            if (!--_readers && _pending_writers) _writer_cv.notify_one();
            return _readers;
        }

        // every new writer is added to pending writers
        // pending writer will be promoted to a writer after acquiring lock
        // lock will be acquired when no readers are reading and
        // no writers writing
        void writerlock () {
            std::unique_lock lk (_m);
            ++_pending_writers;
            _writer_cv.wait(lk, [this] { return !_readers && !_writing; } );

            --_pending_writers;
            _writing = true;
        }
        
        // decrement current writers and let pending writers write one by one
        // after all pending writers are done, we notify readers to acquire lock
        void writerunlock () {
            std::unique_lock lk (_m);
            _writing = false;
            if (_pending_writers > 0) _writer_cv.notify_one();
            else _reader_cv.notify_all();
        }
};


// RAII guard for readers and writers
struct ReadGuard {
    RW_lock& _rw;

    explicit ReadGuard (RW_lock& rw) : _rw (rw) { _rw.readerlock(); }
    ~ReadGuard () { _rw.readerunlock(); }

    ReadGuard (const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;
};

struct WriteGuard {
    RW_lock& _rw;

    explicit WriteGuard (RW_lock& rw) : _rw (rw) { _rw.writerlock(); }
    ~WriteGuard () { _rw.writerunlock(); }

    WriteGuard (const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;
};


