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
	FrameworkThread *self;		// 指向线程本身的指针
	bool quit_properly;			// 线程正确退出
	int managed;				// 引用计数器，框架线程由线程管理器管理
	int managed_thread_id;		// 如果一个线程被管理，这将是它的管理器 ID（可能不是线程 ID）
	void *custom_data;			// 为 Framework Thread 的派生类保留
};

// 一个简单的线程抽象，在新线程上建立消息循环。
// 消费者使用线程的消息循环使代码执行
// 线程。当此对象被销毁时，线程将终止。全部
// 在线程的消息循环中排队的待处理任务将运行完成
// 在线程终止之前。
//
// 线程停止后，销毁顺序为：
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

	// 启动线程。如果线程已成功启动，则返回 true；
	// 否则，返回 false。成功返回后，消息循环（）
	// getter 将返回非空值。
	//
	// 注意：此函数不能在持有加载器锁的 Windows 上调用；
	// 即在 Dll Main、全局对象构造或销毁期间，atexit()
	// 打回来。
	bool Start();

	// 启动线程。除了允许
	// 覆盖默认的循环类型。
	//
	// 注意：此函数不能在持有加载器锁的 Windows 上调用；
	// 即在 Dll Main、全局对象构造或销毁期间，atexit()
	// 打回来。
#if defined(OS_WIN)
	bool StartWithLoop(const MessageLoop::Type type, Dispatcher *dispatcher = NULL);
	bool StartWithLoop(CustomMessageLoopFactory *factory, Dispatcher *dispatcher = NULL);
#else
	bool StartWithLoop(const MessageLoop::Type type);
	bool StartWithLoop(CustomMessageLoopFactory *factory);
#endif // OS_WIN

	// 创建一个消息循环并在当前线程上运行
	// 如果 OS = Windows，请确保当前线程是 crt 线程（由 beginthreadex 等创建）
#if defined(OS_WIN)
	void RunOnCurrentThreadWithLoop(const MessageLoop::Type type, Dispatcher *dispatcher = NULL);
#else
	void RunOnCurrentThreadWithLoop(const MessageLoop::Type type);
#endif // OS_WIN

	// 指示线程退出并在线程退出后返回。后
	// 此方法返回，Thread 对象已完全重置并可以使用
	// 就好像它是新构造的（即，可以再次调用 Start）。
	//
	// Stop 可能会被多次调用，如果线程正在运行，它会被简单地忽略
	// 已经停止了。
	//
	// 注意：此方法是可选的。严格来说没有必要调用这个
	// 方法作为线程的析构函数将负责停止线程，如果
	// 必要的。
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