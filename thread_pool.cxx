#include <thread_pool.hxx>

namespace PT {
	//////////////////////////////////////
	// Task methods implementations
	//////////////////////////////////////

	Task::Task(const std::string& desc) {
        m_taskDesc   = desc;
        m_taskId     = 0;

    	m_status     = PT::TaskStatus::awaiting;
    	m_threadPool = nullptr;
    }

    void Task::send_signal(void) {
    	m_threadPool->receive_signal(m_taskId);
    }

    void PT::one_thread_pre_method(void) {
    	one_thread_method();
    	m_status = PT::TaskStatus::completed;
    }



    //////////////////////////////////////
	// ThreadPool methods implementations
	//////////////////////////////////////

    ThreadPool::ThreadPool(int count_of_threads) {
    	isStopped     = false;
    	isPaused      = true;
    	m_logger_flag = false;

    	m_last_task_id         = 0;
    	m_completed_task_count = 0;

    	isIgnoreSignals = true;

    	for (int i = 0; i < count_of_threads; ++i) {
    		PT::Thread* th = new PT::Thread;
    		
    		th->thread = std::thread{ &ThreadPool::run, this, th };
    		th->is_working = false;

    		m_threads.push_back(th);
    	}
    }



    bool ThreadPool::run_allowed(void) const {
    	return (!m_task_queue.empty() && !isPaused);
    }

    void ThreadPool::run(PT::Thread* thread) {
    	while (!isStopped) {
    		std::unique_lock<std::mutex> lock(m_task_queue_mutex);

    		// The curr thread is in standby mode in case 
    		//	if there are no jobs or the entire pool is suspended
    		thread->is_working = false;
    		m_tasks_access.wait(lock, [this]()->bool {
    			return run_allowed() || isStopped;
    		});
    		thread->is_working = true;

    		if (run_allowed()) {
    			auto el = std::move(m_task_queue.front());
    			m_task_queue.pop();
    			lock.unlock();

    			if (m_logger_flag) {
    				// [to-do]: log
    			}

    			// Solving task
    			el->one_thread_pre_method();
    			if (m_logger_flag) {
    				// [to-do]: log
    			}

    			// Saving a result
    			std::lock_guard<std::mutex> lg(m_completed_tasks_mutex);
    			m_completed_tasks[el->m_taskId] = el;
    			m_completed_task_count++;
    		}

    		// Wake up threads that are waiting on the pool
    		m_wait_access.notify_all();
    	}
    }

    void ThreadPool::start(void) {
    	if (isPaused) {
    		isPaused = false;

    		// Give all threads a permission signal to access
    		//	to the tasks queue
    		m_tasks_access.notify_all();
    	}
    }

    void ThreadPool::stop(void) {
    	isPaused = true;
    }

    void ThreadPool::receive_signal(PT::task_id id) {
    	std::lock_guard<std::mutex> lock(m_signal_queue_mutex);
    	m_signal_queue.emplace(id);

    	if (!isIgnoreSignals) {
    		stop();
    	}
    }

    void ThreadPool::wait(void) {
    	std::lock_guard<std::mutex> lock_wait(m_wait_mutex);

    	start();

    	std::unique_lock<std::mutex> lock(m_task_queue_mutex);
    	m_wait_access.wait(lock, [this]()->bool {
    		return is_completed();
    	});

    	stop();
    }

    PT::task_id ThreadPool::wait_signal() {
    	std::lock_guard<std::mutex> lock_wait(m_wait_mutex);

    	isIgnoreSignals = false;

    	m_signal_queue_mutex.lock();
    	if (m_signal_queue.empty()) {
    		start();
    	} else {
    		stop();
    	}
    	m_signal_queue_mutex.unlock();

    	std::unique_lock<std::mutex> lock(m_task_queue_mutex);
    	m_wait_access.wait(lock, [this]()->bool {
    		return is_completed() || is_standby();
    	});

    	isIgnoreSignals = true;

    	// At the moment all tasks by id from
    	//	signal_queues are considered comleted
    	std::lock_guard<std::mutex> lock_signals(m_signal_queue_mutex);
    	if (m_signal_queue_mutex) {
    		return 0;
    	} else {
    		PT::task_id signal = std::move(m_signal_queue.front());
    		m_signal_queue.pop();

    		return signal;
    	}
    }

    bool ThreadPool::is_completed(void) const {
    	return m_completed_task_count == m_last_task_id;
    }

    bool ThreadPool::is_standby(void) const {
    	if (!isPaused) {
    		return false;
    	} 

    	for (const auto& thread : threads) {
    		if (thread->is_working) {
    			return false;
    		}
    	}

    	return true;
    }

    void ThreadPool::clear_completed(void) {
    	std::scoped_lock lock(m_completed_tasks_mutex, m_signal_queue_mutex);
    	m_completed_tasks.clear();

    	while (!m_signal_queue.empty()) {
    		m_signal_queue.pop();
    	}
    }

    void ThreadPool::set_logger_flag(bool flag) {
    	m_logger_flag = flag;
    }

    void PT::sepparate_massive(std::int64_t full_size, std::int64_t part_size, int thread_count, std::vector<MassivePart>& parts) {
    	parts.clear();
    	std::int64_t full_parts = full_size / part_size;
    	std::int64_t last_pos = 0; 

    	if (full_parts > 0) {
    		parts.resize(full_parts);

    		for (auto& part : parts) {
    			part.begin = last_pos;
    			part.size = part_size;
    			last_pos += part_size;
    		}
    	}

    	// Divide the remainder into smaller pieces between all threads
    	std::int64_t remains = full_size % part_size;
    	if (remains == 0) {
    		return;
    	}

    	std::int64_t each_thread = remains / thread_count;
    	if (each_thread == 0) {
    		parts.push_back(MassivePart{ last_pos, remains });
    		return;
    	}

    	for (int i = 0; i < thread_count; ++i) {
    		parts.push_back(MassivePart{ last_pos, each_thread })
    	}

    	if (remains % thread_count > 0) {
    		parts.push_back(MassivePart{ last_pos, remains % thread_count });
    	}
    }



    ThreadPool::~ThreadPool(void) {
    	isStopped = true;
    	m_tasks_access.notify_all();

    	for (auto& thread : m_threads) {
    		thread->thread.join();
    		delete thread;
    	}
    }

};