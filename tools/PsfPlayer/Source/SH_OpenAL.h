#ifndef _CSH_OPENAL_H_
#define _CSH_OPENAL_H_

#include <deque>
#include "SoundHandler.h"
#include "openal/Device.h"
#include "openal/Context.h"
#include "openal/Source.h"
#include "openal/Buffer.h"

class CSH_OpenAL : public CSoundHandler
{
public:
							CSH_OpenAL();
	virtual					~CSH_OpenAL();

	static CSoundHandler*	HandlerFactory();

	void					Reset();
	void					Write(int16*, unsigned int, unsigned int);
	bool					HasFreeBuffers();
	void					RecycleBuffers();

private:
	typedef std::deque<ALuint> BufferList;

	enum
	{
		MAX_BUFFERS = 25,
	};

	OpenAl::CDevice			m_device;
	OpenAl::CContext		m_context;
	OpenAl::CSource			m_source;

	BufferList				m_availableBuffers;
	uint64					m_lastUpdateTime;
	bool					m_mustSync;
	ALuint					m_bufferNames[MAX_BUFFERS];
};

#endif
