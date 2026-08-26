//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// multithread.h


#ifndef _MULTITHREAD_H
#  define _MULTITHREAD_H

#  include "log_level.h"
#  include "log_profile.h"

///
class SyncObject
{
public:
	///
	virtual void acquire() = 0;
	/// begin writing a message of the given severity
	virtual void acquire(LogLevel) {
		acquire();
	}
	///
	virtual void release() = 0;
};

///
class Acquire
{
	SyncObject* m_so;	///
	/** Times the wait and the hold.  Declared after m_so so that it is
	    constructed - and takes its first timestamp - before the constructor
	    body calls acquire().  Empty unless NYAMY_LOG_PROFILE is defined. */
	AcquireProfile m_profile;

public:
	///
	Acquire(SyncObject* i_so) : m_so(i_so) {
		m_so->acquire();
		m_profile.acquired();
	}
	///
	Acquire(SyncObject* i_so, LogLevel i_level) : m_so(i_so) {
		m_so->acquire(i_level);
		m_profile.acquired();
	}
	///
	~Acquire() {
		m_so->release();
		m_profile.released();
	}
};

#endif // !_MULTITHREAD_H
