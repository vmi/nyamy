//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// multithread.h


#ifndef _MULTITHREAD_H
#  define _MULTITHREAD_H

#  include "log_level.h"

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

public:
	///
	Acquire(SyncObject* i_so) : m_so(i_so) {
		m_so->acquire();
	}
	///
	Acquire(SyncObject* i_so, LogLevel i_level) : m_so(i_so) {
		m_so->acquire(i_level);
	}
	///
	~Acquire() {
		m_so->release();
	}
};

#endif // !_MULTITHREAD_H
