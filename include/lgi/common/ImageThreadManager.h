#pragma once

#include "lgi/common/EventTargetThread.h"

#if 0
#define ITM_LOG(...)	LgiTrace(__VA_ARGS__)
#else
#define ITM_LOG(...)
#endif

/// Thread pool to load images...
class LImageThreadManager : public LThread, public LMutex, public LCancel
{
    enum TMessages
    {
        M_LOAD_IMG = M_USER + 100,
    };

    struct WorkerThread : public LEventTargetThread
    {
        bool busy = false;
        
        WorkerThread(LString n) : LEventTargetThread(n)
        {
        }

    	LAutoPtr<LSocketI> CreateSock(const char *Proto)
    	{
    		LAutoPtr<LSocketI> s;
    		if (Proto && !_stricmp(Proto, "https"))
    		{
    			SslSocket *ss;
    			s.Reset(ss = new SslSocket);
    			ss->SetSslOnConnect(false);
    		}
    		else
    			s.Reset(new LSocket);
    		
    		return s;
    	}

        LMessage::Result OnEvent(LMessage *Msg) override
        {
            switch (Msg->Msg())
            {
                case M_LOAD_IMG:
                {
                    auto Job = Msg->AutoA<LDocumentEnv::LoadJob>();
        			if (!Job)
        			{
    					ITM_LOG("%s:%i - No job obj\n", _FL);
    					LAssert(!"no job");
        			    break;
        			}

                    if (!Job->Env)
                    {
    					ITM_LOG("%s:%i - No env for '%s'\n", _FL, Job->Uri.Get());
    					LAssert(!"no env");
    					break;
    				}

    				busy = true;
    			
    				LUri u(Job->Uri);
    				if (u.IsFile())
    				{
    					// Local document?
    					if (Job->pDC.Reset(GdcD->Load(Job->Uri)))
                    		Job->Env->OnDone(Job);
    					else
    					    ITM_LOG("%s:%i - img load failed for '%s'\n", _FL, Job->Uri.Get());
    				}
    				else
    				{
    					LMemQueue p(1024);
    					LError Err;
    					auto r = LGetUri(this, &p, &Err, Job->Uri);
    					if (r)
    					{
    						auto Hint = p.Peek(16);
    						auto Filter = LFilterFactory::New(u.sPath, FILTER_CAP_READ, (uchar*)Hint.Get());
    						if (Filter)
    						{
    							LAutoPtr<LSurface> Img(new LMemDC(_FL));
    							LFilter::IoStatus Rd = Filter->ReadImage(Img, &p);
    							if (Rd == LFilter::IoSuccess)
    							{
    								Job->pDC = Img;
    								if (Job->Env)
    								{
    									ITM_LOG("Loaded '%s' as image %ix%i\n", Job->Uri.Get(), Job->pDC->X(), Job->pDC->Y());
    									Job->Env->OnDone(Job);
    								}
    								else
    								{
    								}
    							}
    							else LgiTrace("%s:%i - Failed to read '%s'\n", _FL, Job->Uri.Get());
    						}
    						else LgiTrace("%s:%i - Failed to find filter for '%s'\n", _FL, Job->Uri.Get());
    					}
    					else LgiTrace("%s:%i - Failed to get '%s'\n", _FL, Job->Uri.Get());
        			}
        			
    				busy = false;
                    break;
                }
            }
            return 0;
        };
    };

    // lock before use:
	LArray<LDocumentEnv::LoadJob*> In;
	
	// no locking, this thread only:
	LArray<WorkerThread*> pool;
	int workerCount = 1;

	LAutoPtr<LDocumentEnv::LoadJob> GetJob()
	{
		LAutoPtr<LDocumentEnv::LoadJob> j;
		if (auto lck = Auto(this, _FL))
		{
			if (In.Length())
			{
			    if (auto lj = dynamic_cast<LDocumentEnv::LoadJob*>(In[0]))
			    {
				    j.Reset(lj);
				    In.DeleteAt(0, true);
				}
				else LAssert(!"wrong obj");
			}
		}
		return j;
	}
	
	WorkerThread *GetFreeWorker()
	{
	    for (auto t: pool)
	    {
	        if (!t->busy)
	            return t;
	    }
	    return nullptr;
	}
	
	int Main()
	{
	    for (int i=0; i<workerCount; i++)
	    {
	        pool.Add(new WorkerThread(LString::Fmt("HtmlImgLd.%i", i)));
	    }
	
		while (!IsCancelled())
		{
		    if (auto worker = GetFreeWorker())
		    {
		        if (auto job = GetJob())
		        {
		            ITM_LOG("%s:%i - load '%s' with '%s'\n", _FL, job->Uri.Get(), worker->LThread::GetName());
		            worker->PostEvent(M_LOAD_IMG, (LMessage::Param)job.Release());
		            continue;
		        }
		    }
			
			// don't eat cpu...
			LSleep(20);
		}

        // Cancel all the workers...
        ITM_LOG("%s:%i - cancelling workers...\n", _FL);
		for (auto t: pool)
		    t->Cancel(true);
		    
		// Wait for them to stop...
		while (pool.Length())
		{
		    for (auto t: pool)
		    {
		        if (t->IsExited())
		        {
                    ITM_LOG("%s:%i - worker '%s' done\n", _FL, t->LThread::GetName());
		            pool.Delete(t);
		            break;
		        }
		    }
		    LSleep(1);
		}
	
        ITM_LOG("%s:%i - ImageThreadManager main finished.\n", _FL);
		return 0;
	}

public:
	LImageThreadManager(int threadCount = 8) :
		LThread("ImgThMan.Th"),
		LMutex("ImgThMan.Lk"),
		workerCount(threadCount)
	{
		Run();
	}
	
	~LImageThreadManager()
	{
		Cancel(true);
		while (!IsExited())
			LSleep(1);
	}
	
    /// Add an image to the worker que:
	void Add(LAutoPtr<LDocumentEnv::LoadJob> j)
	{
		if (Lock(_FL))
		{
			In.Add(j.Release());
			Unlock();
		}
	}
};

