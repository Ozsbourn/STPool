#include <queue>
#include <vector>
#include <unordered_map>
#include <string>

#include <mutex>
#include <thread>

#include <condition_variable>
#include <atomic>

#include "timer.h"

#ifndef THREAD_POOL_HXX_
#define THREAD_POOL_HXX_

namespace TP {

    using task_id = std::uint64_t;

    class Task {
    private:
        friend class ThreadPool;

    // Protected data
    protected:
        PT::Task::TaskStatus m_status;
        std::string          m_taskDesc;

        PT::task_id          m_taskId;

        PT::ThreadPool*      m_threadPool;

    // Protected member methods
    protected:
    	// Thread-running method
        void one_thread_pre_method(void);

    // Data types definitions for Task class
    public:
        enum class TaskStatus {
            awaiting,
            completed
        };

    // Public member methods
    public:
        Task(const std::string& desc);


        void send_signal(void);

        void virtual one_thread_method(void) = 0;


        virtual ~Task(void);

    };



    struct Thread {
        std::thread       thread;
        std::atomic<bool> is_working;
    };



    class ThreadPool {
    private:
        friend void PT::Task::send_signal(void);

    // Private data
    private:
        std::mutex m_task_queue_mutex;
        std::mutex m_completed_tasks_mutex;
        std::mutex m_signal_queue_mutex;

        std::mutex m_logger_mutex;

        std::mutex m_wait_mutex;

        std::condition_variable m_tasks_access;
        std::condition_variable m_wait_access;

        std::vector<PT::Thread*> m_threads;


        std::queue<std::shared_ptr<Task>> m_task_queue;
        PT::task_id                       m_last_task_id;

        std::unordered_map<PT::task_id, std::shared_ptr<Task>> m_completed_tasks;
        std::uint64_t                                          m_completed_task_count;

        std::queue<PT::task_id> m_signal_queue;


        std::atomic<bool> isStopped;

        std::atomic<bool> isPaused;
        std::atomic<bool> isIgnoreSignals;

        std::atomic<bool> m_logger_flag;

        PT::Timer         m_timer;

    // Private methods
    private:
        // Main function that initializes each thread
        void run(PT::Thread* thread);

        // Pause processing with signal emission
        void receive_signal(PT::task_id id);

        // Permission to start the next thread
        bool run_allowed(void) const;

        // Checking the execution of all tasks from the queue
        bool is_comleted(void) const;

        // Checking if at least one thread is busy
        bool is_standby(void)  const;

    public:
        ThreadPool(int count_of_threads);



        // Template function for adding a task to the queue
        template <typename TaskChild>
        PT::task_id add_task(const TaskChild& task) {
            std::lock_guard<std::mutex> lock(m_task_queue_mutex);
            m_task_queue.push(std::make_shared<TaskChild>(task));

            // assign a unique id to a new task
            // the minimum value of id is 1
            m_task_queue.back()->m_taskId = ++m_last_task_id;

            // associate a task with the current pool
            m_task_queue.back()->m_threadPool = this;
            m_tasks_access.notify_one();

            return m_last_task_id;
        }

        // Waiting for the current task queue to be completely processed or suspended,
        //	returns the id of the task that first signaled and 0 otherwise
        PT::task_id wait_signal(void);

        // Wait for the current task queue to be fully processed,
        //	ignoring any pause signals
        void wait(void);

        // Pause processing
        void stop(void);

        // Resumption of processing
        void start(void);


        // Get result by id
        template <typename TaskChild>
        std::shared_ptr<TaskChild> get_result(MT::task_id id) {
            auto el = m_completed_tasks.find(id);

            if (el != m_completed_tasks.end()) {
                return std::reinterpret_pointer_cast<TaskChild>(el->second);
            } else {
                return nullptr;
            }
        }


        // Cleaning completed tasks
        void clear_completed(void);

        // Setting the logging flag
        void set_logger_flag(bool flag);



        ~ThreadPool(void);

    };



    struct MassivePart {
    	std::int64_t begin;
    	std::int64_t size;
    };

    void separate_massive(std::int64_t full_size, std::int64_t part_size, int thread_count, std::vector<MassivePart>& parts);

};

#endif // THREAD_POOL_HXX_