#include "Kaleido3D.h"
#include "Timer.h"

#include "LogUtil.h"

#ifdef K3DPLATFORM_OS_WIN
#include <Windows.h>
#include <intrin.h>
#elif defined(K3DPLATFORM_OS_LINUX) && !defined(K3DPLATFORM_OS_ANDROID)
#include <time.h>
///
/// \brief rdtsc for x64 machine
/// \return
///
static __inline__ unsigned long long __rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static int64 Lfrequency = 0;
static int64 QueryFrequency()
{
    timespec sp;
    Lfrequency = ::clock_getres(CLOCK_MONOTONIC,&sp);
}

#endif

static int64 gFrequency = 0;

#ifdef K3DPLATFORM_OS_WIN 

static int64 GetPerformanceFrequency()
{
  static int64 counterFrequency = 0;
  LARGE_INTEGER frequency;
  if ( !QueryPerformanceFrequency( &frequency ) ) {
    counterFrequency = 0;
  }
  else {
    counterFrequency = frequency.QuadPart;
  }
  return counterFrequency;
}

static inline int64 GetPerformanceCounter()
{
  if ( gFrequency > 0 ) {
    LARGE_INTEGER counter;

    if ( QueryPerformanceCounter( &counter ) ) {
      return counter.QuadPart;
    }
    else {
      KLOG(Error, "Timer", "QueryPerformanceCounter failed, although QueryPerformanceFrequency succeeded." );
      return 0;
    }
  }
  else
    return 0;
}
#endif

static inline int64 ticksToNanoseconds( int64 ticks )
{
  if ( gFrequency > 0 ) {
    // QueryPerformanceCounter uses an arbitrary frequency
    int64 seconds = ticks / gFrequency;
    int64 nanoSeconds = (ticks - seconds * gFrequency) * 1000000000 / gFrequency;
    return seconds * 1000000000 + nanoSeconds;
  }
  else {
    // GetTickCount(64) return milliseconds
    return ticks * 1000000;
  }
}

namespace k3d {

	Timer::Timer(Precision precision)
		: m_Precision(precision)
		, m_BaseTime(0)
		, m_CurrentTime(0)
		, m_FrameRate(0)
		, m_Enabled(false)
	{
#ifdef K3DPLATFORM_OS_WIN
		gFrequency = GetPerformanceFrequency();
#elif defined(K3DPLATFORM_OS_LINUX) && !defined(K3DPLATFORM_OS_ANDROID)
		frequency = QueryFrequency();
#endif
	}


	Timer::~Timer()
	{
	}

	void Timer::ResetTimer()
	{
#ifdef K3DPLATFORM_OS_WIN
		m_CurrentTime = GetPerformanceCounter();
#endif
	}

	int64 Timer::MicrosecElapsed()
	{
#ifdef K3DPLATFORM_OS_WIN
		m_LastTime = m_CurrentTime;
		m_CurrentTime = GetPerformanceCounter();
#endif
		return ticksToNanoseconds(m_CurrentTime - m_LastTime) / 1000000;
	}

	void Timer::BeginTimer()
	{
#if defined(K3DPLATFORM_OS_WIN)
		m_BaseTime = __rdtsc();
#endif
	}

	int64 Timer::EndTimer()
	{
#if defined(K3DPLATFORM_OS_WIN)
		m_OffSetTime = __rdtsc() - m_BaseTime;
#endif
		return m_OffSetTime;
	}

	void Timer::Update()
	{
		m_FrameRate = 1.0f*m_OffSetTime / gFrequency;
		//SetTimer( 0, 0, 0, TimerFunction );
	}

	Timer* Timer::CreateNewTimer()
	{
		return new Timer;
	}

	void Timer::EnableTimer(bool enable)
	{
		m_Enabled = enable;
	}

}