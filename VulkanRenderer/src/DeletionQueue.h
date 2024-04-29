#pragma once

#include <functional>
#include <stdint.h>
#include <unordered_map>
#include <deque>

class DeletionQueue {
   public:
	using Handle = size_t;

	Handle pushDeleter(std::function<void()> deleter);
	void removeDeleter(const Handle handle);

	void flush();

   private:
	Handle generateHandle();
	void freeHandle(const Handle handle);

	std::unordered_map<Handle, size_t> m_HandleToIndex;
	std::unordered_map<size_t, Handle> m_IndexToHandle;
	std::deque<std::function<void()>> m_Deleters;

	std::vector<DeletionQueue::Handle> m_FreeHandles;
	size_t m_HandleCounter{};
};
