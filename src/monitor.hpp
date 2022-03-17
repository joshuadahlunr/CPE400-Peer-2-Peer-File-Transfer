#ifndef ___MONITOR_HPP___
#define ___MONITOR_HPP___

#include <mutex>
#include <shared_mutex>

#include "include_everywhere.hpp"

/**
 * @brief Adapter class that provides some thread safe wrappers around other types.
 * 	The data held by the monitor is stack allocated, while it returns pointer like types it itself does not model a pointer to the data
 * @note Modified from https://stackoverflow.com/questions/12647217/making-a-c-class-a-monitor-in-the-concurrent-sense/48408987#48408987
 *
 * @tparam T - The type this monitor guards
 * @tparam Mutex - The mutex the locks the data
 */
template<class T, typename Mutex = std::shared_mutex>
class monitor {
	/**
	 * @brief Base class that provides pointer type access to locked data
	 */
	struct lock_base {
		// The monitor that created this wrapper
		monitor *const creator;

		lock_base(const monitor* creator) : creator((monitor*) creator) {}
		// Arrow operator
		T* operator->() { return &creator->data; }
		const T* operator->() const { return &creator->data; }
		// Dereference operator
		T& operator*() { return creator->data; }
		const T& operator*() const { return creator->data; }
		// Subscript operator
		template<typename _T> auto& operator[](_T&& index) { return creator->data[index]; }
		template<typename _T> const auto& operator[](_T&& index) const { return creator->data[index]; }

		// Functions which check that the lock contains valid data
		bool has_lock() const { return creator; }
		operator bool() const { return creator; }
	};

private:
	// The wrapped data
	T data;
	// Data mutex
	mutable Mutex mutex;
public:
	// Construction is forwarded to the wrapped type
	template<typename ...Args>
	monitor(Args&&... args) : data(std::forward<Args>(args)...) { }

	/**
	 * @brief A pointer type wrapper with a unique (write) lock
	 */
	struct lock_unique : public lock_base {
		lock_unique(const monitor* creator) : lock_base(creator) { if(creator) lock = std::unique_lock(creator->mutex); }
		lock_unique(lock_unique&& move) : lock_base(move.creator), lock(std::move(move.lock)) {}
		template<typename Duration> lock_unique(const monitor* creator, Duration d) : lock_base(creator) { if(creator) lock = std::unique_lock(creator->mutex, d); }

		// The lock this "pointer" holds
		std::unique_lock<Mutex> lock;
	};

	/**
	 * @brief A pointer type wrapper with a shared (read) lock
	 */
	struct lock_shared : public lock_base {
		lock_shared(const monitor* creator) : lock_base(creator) { if(creator) lock = std::shared_lock(creator->mutex); }
		lock_shared(lock_shared&& move) : lock_base(move.creator), lock(std::move(move.lock)) {}
		template<typename Duration> lock_shared(const monitor* creator,  Duration d) : lock_base(creator) { if(creator) lock = std::shared_lock(creator->mutex, d); }

		// The lock this "pointer" holds
		std::shared_lock<Mutex> lock;
	};

	// Return a write locked pointer to the underlying data
	lock_unique write_lock() { return lock_unique(this); }
	const lock_unique write_lock() const { return lock_unique(this); }
	lock_unique try_write_lock() {
		auto lock = lock_unique(nullptr);
		lock.lock = std::unique_lock(mutex, std::defer_lock_t{});
		if(lock.lock.try_lock()) reference_cast<monitor*>(lock.creator) = this;
		return lock;
	}
	const lock_unique try_write_lock() const {
		auto lock = lock_unique(nullptr);
		lock.lock = std::unique_lock(mutex, std::defer_lock_t{});
		if(lock.lock.try_lock()) reference_cast<monitor*>(lock.creator) = (monitor*) this;
		return lock;
	}
	// Return a read locked pointer to the underlying data
	lock_shared read_lock() { return lock_shared(this); }
	const lock_shared read_lock() const { return lock_shared(this); }
	lock_shared try_read_lock() {
		auto lock = lock_shared(nullptr);
		lock.lock = std::shared_lock(mutex, std::defer_lock_t{});
		if(lock.lock.try_lock()) reference_cast<monitor*>(lock.creator) = this;
		return lock;
	}
	const lock_shared try_read_lock() const {
		auto lock = lock_shared(nullptr);
		lock.lock = std::shared_lock(mutex, std::defer_lock_t{});
		if(lock.lock.try_lock()) reference_cast<monitor*>(lock.creator) = (monitor*) this;
		return lock;
	}
	// Return an unsafe (no lock) reference to the underlying data
	T& unsafe() { return data; }
	const T& unsafe() const { return data; }

	// Call a function or access a member of the base type directly
	lock_unique operator->() { return write_lock(); }				// If we might change the data return a write lock
	const lock_shared operator->() const { return read_lock(); }	// If we can't change the data return a read lock
};

#endif // ___MONITOR_HPP___
