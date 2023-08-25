#ifndef __OS_CLASS_H
#define __OS_CLASS_H

class LLocker
{
protected:
    BHandler *hnd = NULL;
    const char *file = NULL;
    int line = 0;
    bool locked = false;
    bool noThread = false;
    bool attempted = false;

public:
    LLocker(BHandler *h, const char *File, int Line);
    ~LLocker();

    // Lock with no timeout
    bool Lock(bool debug = false);
    
    // Lock with timeout.
    // \returns one of:
    //    - B_ERROR:     Fatal error occured.
    //    - B_OK:        Lock succeeded.
    //    - B_TIMED_OUT: Timeout occured.
    //    - B_BAD_VALUE: Handler not associated with a looper (anymore).
    status_t LockWithTimeout(int64 time, bool debug = false);
    
    // Waits 'timeout' milliseconds for lock, sleeping occasionally to avoid deadlocks
    bool WaitForLock(int timeout = 2000);
    
    // Unlocks the handler after a successful lock.
    void Unlock();
    
    operator bool();
};

#endif
