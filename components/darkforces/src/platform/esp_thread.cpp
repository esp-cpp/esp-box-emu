// ESP32 (esp-box-emu) thread implementation for The Force Engine (FreeRTOS).
#include <TFE_System/system.h>
#include <TFE_System/Threads/thread.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class ThreadEsp : public Thread
{
public:
	ThreadEsp(const char* name, ThreadFunc func, void* userData);
	virtual ~ThreadEsp();

	virtual bool run();
	virtual void pause();
	virtual void resume();
	virtual void waitOnExit(void);

	ThreadFunc getFunc() { return m_func; }
	void* getUserData() { return m_userData; }

protected:
	static void threadEntry(void* arg);

	TaskHandle_t m_task = nullptr;
	SemaphoreHandle_t m_done = nullptr;
};

ThreadEsp::ThreadEsp(const char* name, ThreadFunc func, void* userData) : Thread(name, func, userData)
{
	m_done = xSemaphoreCreateBinary();
}

ThreadEsp::~ThreadEsp()
{
	if (m_isRunning)
	{
		waitOnExit();
	}
	if (m_done)
	{
		vSemaphoreDelete(m_done);
		m_done = nullptr;
	}
}

void ThreadEsp::threadEntry(void* arg)
{
	ThreadEsp* thread = (ThreadEsp*)arg;
	thread->getFunc()(thread->getUserData());
	thread->m_isRunning = false;
	xSemaphoreGive(thread->m_done);
	vTaskDelete(nullptr);
}

bool ThreadEsp::run(void)
{
	if (m_isRunning) { return true; }
	m_isRunning = true;
	// Engine threads (the MIDI/iMuse update thread) run on core 1 alongside the audio task,
	// leaving core 0 to the game and display.
	BaseType_t ok = xTaskCreatePinnedToCore(threadEntry, m_name, 8 * 1024, this, 8, &m_task, 1);
	if (ok != pdPASS)
	{
		m_isRunning = false;
		m_task = nullptr;
		TFE_System::logWrite(LOG_ERROR, "Thread", "Cannot create thread '%s'.", m_name);
		return false;
	}
	return true;
}

void ThreadEsp::pause(void)
{
	if (m_task && m_isRunning && !m_isPaused)
	{
		vTaskSuspend(m_task);
		m_isPaused = true;
	}
}

void ThreadEsp::resume(void)
{
	if (m_task && m_isRunning && m_isPaused)
	{
		vTaskResume(m_task);
		m_isPaused = false;
	}
}

void ThreadEsp::waitOnExit(void)
{
	if (m_task && m_isRunning)
	{
		if (m_isPaused) { resume(); }
		xSemaphoreTake(m_done, portMAX_DELAY);
	}
	m_task = nullptr;
	m_isRunning = false;
}

//factory
Thread* Thread::create(const char* name, ThreadFunc func, void* userData)
{
	return new ThreadEsp(name, func, userData);
}
