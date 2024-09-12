///  @file    TimerEx.h
///  @date    2019/07/12
///  @author  Lee Jong Oh
#include "TimerEx.h"

#include <vector>
#include <memory>
#include <mutex>

#ifdef WIN32
#ifndef _WINDOWS_
#include <windows.h>
#endif
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#else
#include <csignal>
#include <ctime>
#include <unistd.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#endif

//////////////////////////////////////////////////////////////////////////
// Define
//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
typedef MMRESULT TimerIdOs;
#elif __linux
typedef timer_t TimerIdOs;
#endif // __linux

using namespace timer_ex;

//////////////////////////////////////////////////////////////////////////
// class CTimerImpl
//////////////////////////////////////////////////////////////////////////

class CTimerImpl
{
    friend class CTimerInstance;

public:
    struct ParamTimer
    {
        TimerIdOs id   = 0;
        int       ms   = 0;
        void*     ptr  = nullptr;
        bool      used = false;

        std::function<void(TimerIdEx id, void* ptr)> func;
    };

protected:
    static std::vector<ParamTimer> m_timers;
    static std::recursive_mutex    m_mutex_timers;

protected:
    static void CallBack(TimerIdEx id)
    {
        //std::lock_guard<std::recursive_mutex> lock(m_mutex_timers);

        ParamTimer& output = m_timers[(size_t)id];
        if (output.used && output.func)
            output.func(id, output.ptr);
    }

    TimerIdEx CreateTimerId()
    {
        int index = 0;
        for (ParamTimer& item : m_timers)
        {
            if (false == item.used)
                break;
            index++;
        }

        return (TimerIdEx)index;
    }

    int DeleteTimerAll()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_timers);

        int index = 0;
        for (ParamTimer& item : m_timers)
        {
            if (item.used)
                DeleteTimer((TimerIdEx)index);
            index++;
        }

        return 0;
    }

public:
    CTimerImpl()
    {
    }

    virtual ~CTimerImpl()
    {
    }

    virtual int Initialize() = 0;
    virtual int Finalize() = 0;
    virtual int CreateTimer(TimerIdEx& id, const ParamTimer& param) = 0;
    virtual int DeleteTimer(const TimerIdEx& id) = 0;
};

std::vector<CTimerImpl::ParamTimer> CTimerImpl::m_timers(64);
std::recursive_mutex CTimerImpl::m_mutex_timers;

//////////////////////////////////////////////////////////////////////////
// class CTimerWinmm
//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
class CTimerWinmm : public CTimerImpl
{
private:
    static void CALLBACK TimerCallback(UINT id, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
    {
        CallBack((TimerIdEx)dwUser);
    }

public:
    CTimerWinmm()
    {
    }

    virtual ~CTimerWinmm()
    {
    }

    virtual int Initialize() override
    {
        return 0;
    }

    virtual int Finalize() override
    {
        DeleteTimerAll();

        return 0;
    }

    virtual int CreateTimer(TimerIdEx& id, const ParamTimer& param) override
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_timers);

        id = CreateTimerId();

        timeBeginPeriod(1);
        MMRESULT hTimer = timeSetEvent(param.ms, 1, CTimerWinmm::TimerCallback, (DWORD_PTR)id, TIME_PERIODIC);
        if (NULL == hTimer)
            return 1;

        ParamTimer& item = m_timers[id];
        item.id   = hTimer;
        item.ms   = param.ms;
        item.func = param.func;
        item.ptr  = param.ptr;
        item.used = true;

        return 0;
    }

    virtual int DeleteTimer(const TimerIdEx& id) override
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_timers);

        size_t index = (size_t)id;
        if (m_timers.size() <= index)
            return 1;

        ParamTimer& item = m_timers[index];
        item.used = false;
        item.ptr  = nullptr;

        timeKillEvent((MMRESULT)item.id);
        timeEndPeriod(1);

        return 0;
    }
};
#endif

//////////////////////////////////////////////////////////////////////////
// class CTimerLinuxSignal
//////////////////////////////////////////////////////////////////////////

#ifdef __linux

//#define MY_TIMER_SIGNAL SIGRTMIN
#define MY_TIMER_SIGNAL SIGVTALRM
#define ONE_MSEC_TO_NSEC (1000000)
#define ONE_SEC_TO_NSEC  (1000000000)
typedef void(*sa_sigaction_ex)(int, siginfo_t*, void*);


class CTimerLinuxSignal : public CTimerImpl
{
private:
    std::thread m_threadTimerSignal;

    std::mutex m_mutex;
    std::condition_variable m_cv;

private:
    void threadTimerSignal()
    {
        //pthread_setname_np(pthread_self(), "TimerSignal");

        sigset_t tSigSetMask;

        sigemptyset(&tSigSetMask);
        sigaddset(&tSigSetMask, MY_TIMER_SIGNAL);
        sigprocmask(SIG_BLOCK, &tSigSetMask, NULL);
        pthread_sigmask(SIG_UNBLOCK, &tSigSetMask, NULL);

        //sigfillset(&sa.sa_mask);
        //sigdelset(&sa.sa_mask, MY_TIMER_SIGNAL);

        while (true) {
            std::unique_lock<std::mutex> locker(m_mutex);
            m_condition_variable.wait(locker);

            break;
        }

        return;
    }

    /**
     * @brief InitSignal
     *
     * 닫혀 있는 소켓에 데이터를 전송 할 시에 SIGPIPE 시그널이 발생하면서
     * 프로그램을 종료 하는 문제를 처리 하기 위함
     * @return
     */
    int InitSignal()
    {
        struct sigaction act;
        memset(&act, 0, sizeof(act));

        act.sa_handler = SIG_IGN;
        //act.sa_flags = SA_RESTART;
        act.sa_flags = 0;
        if (sigaction(SIGPIPE, &act, 0))
            fprintf(stderr, "sigaction");

        return 0;
    }

    static void TimerCallback(int sig, siginfo_t* si, void* context)
    {
        int key = si->si_value.sival_int;
        CallBack((TimerIdEx)key);
    }
public:
    CTimerLinuxSignal()
    {
    }

    virtual ~CTimerLinuxSignal()
    {
    }

    virtual int Initialize() override
    {
        InitSignal();

        // 모든 Thread 에서 Timer signal 이벤트를 받지 않도록 한다.
        signal(MY_TIMER_SIGNAL, SIG_IGN);

        sigset_t tSigSetMask;
        sigemptyset(&tSigSetMask);
        sigaddset(&tSigSetMask, MY_TIMER_SIGNAL);
        //pthread_sigmask(SIG_SETMASK, &tSigSetMask, 0);
        pthread_sigmask(SIG_BLOCK, &tSigSetMask, 0);

        // Timer Thread 에서만 Timer signal을 받도록 설정한다.
        // 다른 Thread 에서도 Timer signal 신호를 받게 되면 데드락 문제가 발생할 수 있다.
        m_threadTimerSignal = std::thread(std::bind(&CTimerLinuxSignal::threadTimerSignal, this));

        return 0;
    }

    virtual int Finalize() override
    {
        DeleteTimerAll();

        m_condition_variable.notify_one();

        if (m_threadTimerSignal.joinable())
            m_threadTimerSignal.join();

        return 0;
    }

    virtual int CreateTimer(TimerIdEx& id, const ParamTimer& param) override
    {
        TimerIdOs timerId = 0;

        // set up signal handler
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = CTimerInstance::TimerCallback;
        if (sigemptyset(&sa.sa_mask)) {
            return 1;
        }
        if (sigaction(MY_TIMER_SIGNAL, &sa, 0)) {
            return 2;
        }

        id = CreateTimerId();

        // create alarm
        struct sigevent sigEvt;
        memset(&sigEvt, 0x00, sizeof(sigEvt));
        sigEvt.sigev_notify = SIGEV_SIGNAL;
        sigEvt.sigev_signo = MY_TIMER_SIGNAL;
        sigEvt.sigev_value.sival_int = id;
        //sigEvt.sigev_value.sival_ptr = this;
        if (timer_create(CLOCK_REALTIME, &sigEvt, &timerId)) {
            return 3;
        }

        // Save Timer Inforamtion
        ParamTimer& item = m_timers[id];
        item.id = timerId;
        item.ms = param.ms;
        item.func = param.func;
        item.ptr = param.ptr;
        item.used = true;

        // set alarm
        struct itimerspec its;
        long nano_intv = param.ms * ONE_MSEC_TO_NSEC;
        its.it_value.tv_sec = nano_intv / ONE_SEC_TO_NSEC;
        its.it_value.tv_nsec = nano_intv % ONE_SEC_TO_NSEC;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;
        if (timer_settime(timerId, 0, &its, NULL)) {
            return 4;
        }

        return 0;
    }

    virtual int DeleteTimer(const TimerIdEx& id) override
    {
        size_t index = (size_t)id;
        if (m_timers.size() <= index)
            return 1;

        ParamTimer& item = m_timers[index];

        timer_delete((timer_t)item.id);
        item.used = false;

        return 0;
    }
};
#endif

//////////////////////////////////////////////////////////////////////////
// class CTimerInstance
//////////////////////////////////////////////////////////////////////////

class CTimerInstance
{
public:
    enum ActiveType
    {
        TIMER_NONE      = 0,
        WINDOWS_MMTIMER = 1,
        LINUX_SIGNAL    = 10,
    };

private:
    std::unique_ptr<CTimerImpl>     m_impl;
    ActiveType                      m_type;

private:
    CTimerInstance()
    {
    }
    ~CTimerInstance()
    {
        Finalize();
    }

public:
    static CTimerInstance& GetInstance()
    {
        static CTimerInstance manager;
        return manager;
    }

    int Initialize(ActiveType type)
    {
        int ret = 0;
        if (nullptr == m_impl)
        {
            switch (type)
            {
            case CTimerInstance::WINDOWS_MMTIMER:
#ifdef WIN32
                m_impl.reset(new CTimerWinmm);
#endif
                break;
            case CTimerInstance::LINUX_SIGNAL:
#ifdef __linux
                m_impl.reset(new CTimerLinuxSignal);
#endif
                break;
            case CTimerInstance::TIMER_NONE:
            default:
                break;
            }

            m_type = type;

            ret = m_impl->Initialize();
        }

        if (nullptr == m_impl)
            return 100;

        return ret;
    }

    int Finalize()
    {
        return m_impl->Finalize();
    }

    int CreateTimer(TimerIdEx& id, const CTimerImpl::ParamTimer& param)
    {
        return m_impl->CreateTimer(id, param);
    }

    int DeleteTimer(const TimerIdEx& id)
    {
        return m_impl->DeleteTimer(id);
    }

    void CallBack(TimerIdEx id)
    {
        m_impl->CallBack(id);
    }
};

//////////////////////////////////////////////////////////////////////////
// API
//////////////////////////////////////////////////////////////////////////

namespace timer_ex
{
    int InitializeTimer()
    {
        CTimerInstance& instance = CTimerInstance::GetInstance();

#ifdef WIN32
        CTimerInstance::ActiveType type = CTimerInstance::WINDOWS_MMTIMER;
#else
        CTimerInstance::ActiveType type = CTimerInstance::LINUX_SIGNAL;
#endif

        return instance.Initialize(type);
    }

    int FinalizeTimer()
    {
        CTimerInstance& instance = CTimerInstance::GetInstance();
        return instance.Finalize();
    }

    int CreateTimer(TimerIdEx& id, int ms, std::function<void(TimerIdEx id, void* ptr)> func, void* ptr)
    {
        CTimerInstance& instance = CTimerInstance::GetInstance();

        CTimerImpl::ParamTimer param;
        param.ms   = ms;
        param.func = func;
        param.ptr  = ptr;

        return instance.CreateTimer(id, param);
    }

    int DeleteTimer(const TimerIdEx& id)
    {
        CTimerInstance& instance = CTimerInstance::GetInstance();

        return instance.DeleteTimer(id);
    }
}; // namespace timer_ex
