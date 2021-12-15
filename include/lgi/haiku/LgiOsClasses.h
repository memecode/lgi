#ifndef __OS_CLASS_H
#define __OS_CLASS_H

class LLocker
{
    BHandler *hnd;
    const char *file = NULL;
    int line = 0;

protected:
    bool locked = false;

public:
    LLocker(BHandler *h, const char *File, int Line)
    {
        hnd = h;
        file = File;
        line = Line;
    }

    ~LLocker()
    {
        Unlock();
    }

    bool Lock()
    {
        LAssert(!locked);
        LAssert(hnd != NULL);

        while (!locked)
        {
            status_t result = hnd->LockLooperWithTimeout(1000 * 1000);
            if (result == B_OK)
            {
                locked = true;
                break;
            }
            else if (result == B_TIMED_OUT)
            {
                // Warn about failure to lock...
                thread_id Thread = hnd->Looper()->LockingThread();
                printf("%s:%i - Warning: can't lock. Myself=%i\n", _FL, LGetCurrentThread(), Thread);
            }
            else if (result == B_BAD_VALUE)
            {
                break;
            }
            else
            {
                // Warn about error
                printf("%s:%i - Error from LockLooperWithTimeout = 0x%x\n", _FL, result);
            }
        }

        return locked;
    }

    status_t LockWithTimeout(int64 time)
    {
        LAssert(!locked);
        LAssert(hnd != NULL);
        
        status_t result = hnd->LockLooperWithTimeout(time);
        if (result == B_OK)
            locked = true;
        
        return result;
    }

    void Unlock()
    {
        if (locked)
        {
            hnd->UnlockLooper();
            locked = false;
        }
    }
};

#endif
