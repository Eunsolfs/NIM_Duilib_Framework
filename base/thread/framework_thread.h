// Thread with framework(message loop)

#ifndef BASE_THREAD_FRAMEWORK_THREAD_H_
#define BASE_THREAD_FRAMEWORK_THREAD_H_

#include "thread.h"
#include "thread_local.h"
#include "base/framework/message_loop.h"
#include "base/synchronization/waitable_event.h"

namespace nbase
{
class FrameworkThread;

struct FrameworkThreadTlsData
{
	FrameworkThread *self;		// ָ���̱߳����ָ��
	bool quit_properly;			// �߳���ȷ�˳�
	int managed;				// ���ü�����������߳����̹߳���������
	int managed_thread_id;		// ���һ���̱߳������⽫�����Ĺ����� ID�����ܲ����߳� ID��
	void *custom_data;			// Ϊ Framework Thread �������ౣ��
};

// һ���򵥵��̳߳��������߳��Ͻ�����Ϣѭ����
// ������ʹ���̵߳���Ϣѭ��ʹ����ִ��
// �̡߳����˶�������ʱ���߳̽���ֹ��ȫ��
// ���̵߳���Ϣѭ�����ŶӵĴ����������������
// ���߳���ֹ֮ǰ��
//
// �߳�ֹͣ������˳��Ϊ��
//
//  (1) FrameworkThread::CleanUp()
//  (2) MessageLoop::~MessageLoop
//  (3.b)    MessageLoop::DestructionObserver::WillDestroyCurrentMessageLoop
class BASE_EXPORT FrameworkThread : public virtual nbase::SupportWeakCallback, public nbase::Thread
{
	friend class ThreadMap;
	friend class ThreadManager;
public:
	// custom message loop
	class CustomMessageLoop : public MessageLoop
	{
	public:
		virtual ~CustomMessageLoop() {}
	};
	// custom message loop factory
	class CustomMessageLoopFactory
	{
	public:
		virtual CustomMessageLoop* CreateMessageLoop() = 0;
	};

	explicit FrameworkThread(const char* name);

	virtual ~FrameworkThread();

	// �����̡߳�����߳��ѳɹ��������򷵻� true��
	// ���򣬷��� false���ɹ����غ���Ϣѭ������
	// getter �����طǿ�ֵ��
	//
	// ע�⣺�˺��������ڳ��м��������� Windows �ϵ��ã�
	// ���� Dll Main��ȫ�ֶ�����������ڼ䣬atexit()
	// �������
	bool Start();

	// �����̡߳���������
	// ����Ĭ�ϵ�ѭ�����͡�
	//
	// ע�⣺�˺��������ڳ��м��������� Windows �ϵ��ã�
	// ���� Dll Main��ȫ�ֶ�����������ڼ䣬atexit()
	// �������
#if defined(OS_WIN)
	bool StartWithLoop(const MessageLoop::Type type, Dispatcher *dispatcher = NULL);
	bool StartWithLoop(CustomMessageLoopFactory *factory, Dispatcher *dispatcher = NULL);
#else
	bool StartWithLoop(const MessageLoop::Type type);
	bool StartWithLoop(CustomMessageLoopFactory *factory);
#endif // OS_WIN

	// ����һ����Ϣѭ�����ڵ�ǰ�߳�������
	// ��� OS = Windows����ȷ����ǰ�߳��� crt �̣߳��� beginthreadex �ȴ�����
#if defined(OS_WIN)
	void RunOnCurrentThreadWithLoop(const MessageLoop::Type type, Dispatcher *dispatcher = NULL);
#else
	void RunOnCurrentThreadWithLoop(const MessageLoop::Type type);
#endif // OS_WIN

	// ָʾ�߳��˳������߳��˳��󷵻ء���
	// �˷������أ�Thread ��������ȫ���ò�����ʹ��
	// �ͺ��������¹���ģ����������ٴε��� Start����
	//
	// Stop ���ܻᱻ��ε��ã�����߳��������У����ᱻ�򵥵غ���
	// �Ѿ�ֹͣ�ˡ�
	//
	// ע�⣺�˷����ǿ�ѡ�ġ��ϸ���˵û�б�Ҫ�������
	// ������Ϊ�̵߳���������������ֹͣ�̣߳����
	// ��Ҫ�ġ�
	//
	void Stop();

	// Signals the thread to exit in the near future.
	//
	// WARNING: This function is not meant to be commonly used. Use at your own
	// risk. Calling this function will cause message_loop() to become invalid in
	// the near future. This function was created to workaround a specific
	// deadlock on Windows with printer worker thread. In any other case, Stop()
	// should be used.
	//
	// StopSoon should not be called multiple times as it is risky to do so. It
	// could cause a timing issue in message_loop() access. Call Stop() to reset
	// the thread object once it is known that the thread has quit.
	void StopSoon();

	// Returns the message loop for this thread.  Use the MessageLoop's
	// PostTask methods to execute code on the thread.  This only returns
	// non-null after a successful call to Start.  After Stop has been called,
	// this will return NULL.
	//
	// NOTE: You must not call this MessageLoop's Quit method directly.  Use
	// the Thread's Stop method instead.
	//
	MessageLoop* message_loop() const { return message_loop_; }

	// Set the name of this thread (for display in debugger too).
	const std::string &thread_name() { return name_; }

	// Returns true if the thread has been started, and not yet stopped.
	// When a thread is running, |thread_id_| is a valid id.
	bool IsRunning() const { return (Thread::thread_id() != kInvalidThreadId) ? true : false; }

	// Returns true if the thread's stopping flag is set
	bool IsStopping() const { return stopping_; } 

	// Get the current thread
	// must be called in Run or in its children methods
	static FrameworkThread* current();

	// Get the managed thread id of current thread
	// return -1 means current thread is not managed by ThreadManager
	static int GetManagedThreadId();

	// Get custom tls data
	static void* GetCustomTlsData();
	// Set custom tls data
	static void SetCustomTlsData(void *data);

protected:
	// Called just prior to starting the message loop
	virtual void Init() {}

	// Called to start the message loop
	virtual void Run();

	// Called just after the message loop ends
	virtual void Cleanup() {}

	// Called after the message loop has been deleted. In general clients
	// should prefer to use CleanUp(). This method is used when code needs to
	// be run after all of the MessageLoop::DestructionObservers have completed.
	virtual void CleanUpAfterMessageLoopDestruction() {}

	// Initilize the tls data, must be called before any tls associated calls on current thread
	static void InitTlsData(FrameworkThread *self);
	// Free the tls data, must be called after any tls associated calls on current thread
	static void FreeTlsData();
	// Retrieve the tls data
	static FrameworkThreadTlsData* GetTlsData();
	static void SetThreadWasQuitProperly(bool flag);
	static bool GetThreadWasQuitProperly();

	void set_message_loop(MessageLoop* message_loop)
	{
		message_loop_ = message_loop;
	}

private:
	bool thread_was_started() const { return started_; }
	void DoStopSoon();

	// Whether we successfully started the thread.
	bool started_;

	// If true, we're in the middle of stopping, and shouldn't access
	// |message_loop_|. It may non-NULL and invalid.
	bool stopping_;

	// The thread's message loop.  Valid only while the thread is alive.  Set
	// by the created thread.
	MessageLoop::Type loop_type_;
	MessageLoop *message_loop_;
	std::shared_ptr<CustomMessageLoopFactory> factory_;
	
#if defined(OS_WIN)
	// the dispatcher
	Dispatcher *dispatcher_;
#endif // OS_WIN

	// For ensure message_loop_ has instance after thread create 
	WaitableEvent event_;

	// The name of the thread.  Used for debugging purposes.
	std::string name_;
};

}

#endif  // BASE_THREAD_FRAMEWORK_THREAD_H_