#ifndef BLOCKING_QUEUE_
#define BLOCKING_QUEUE_

#include <deque>
#include <mutex>
#include <condition_variable>
#include <algorithm>

template<typename T>
class BlockingQueue {
public:
    void put(T elem) {
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            backingQueue.push_back(elem);
        }
        waitObj.notify_all();
    }

    bool empty() {
        return size() == 0;
    }

    bool swapToFrontIf(std::function<bool(T)> func) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto pos = std::find_if(backingQueue.begin(), backingQueue.end(), func);
        if (pos != backingQueue.begin() && pos != backingQueue.end()) {
            std::iter_swap(pos, backingQueue.begin());
            return true;
        }
        return false;
    }

    bool removeIf(std::function<bool(T)> func) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto it = std::remove_if(backingQueue.begin(), backingQueue.end(), func);
        bool removed = it != backingQueue.end();
        backingQueue.erase(it, backingQueue.end());
        return removed;
    }

    void remove(T elem) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        backingQueue.erase(std::remove(backingQueue.begin(), backingQueue.end(), elem), backingQueue.end());
    }

    size_t size() {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        return backingQueue.size();
    }

    T take() {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        waitObj.wait(lock, [this] { return this->size() > 0; });
        auto front = backingQueue.front();
        backingQueue.pop_front();
        return front;
    }

    std::deque<T> clear() {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        std::deque<T> returnQueue = backingQueue;
        backingQueue.clear();
        return returnQueue;
    }

private:
    std::deque<T> backingQueue{};
    std::recursive_mutex mutex{};
    std::condition_variable_any waitObj{};
};

#endif
