#ifndef TIMER_HXX_
#define TIMER_HXX_

namespace PT {

	class [[nodiscard]] Timer {
	private:
		std::chrono::time_point<std::chrono::steady_clock> m_start_time;
		std::chrono::duration<double>                      m_elapsed_time = std::chrono::duration<double>::zero();

	public:
		inline void start(void) {
			m_start_time = std::chrono::steady_clock::now();
		}

		inline void stop(void) {
			m_elapsed_time = std::chrono::steady_clock::now() - m_start_time;
		}

		inline [[nodiscard]] std::chrono::milliseconds::rep ms(void) const {
			return (std::chrono::duration_cast<std::chrono::milliseconds>(m_elapsed_time)).count();
		}

	};

};

#endif // TIMER_HXX_