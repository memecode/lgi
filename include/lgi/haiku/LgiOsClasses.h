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

public:
    LLocker(BHandler *h, const char *File, int Line);
    ~LLocker();

    bool Lock();
    status_t LockWithTimeout(int64 time);
    void Unlock();
};

#endif
