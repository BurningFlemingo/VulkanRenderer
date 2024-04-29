#include "DeletionQueue.h"

DeletionQueue::Handle DeletionQueue::pushDeleter(std::function<void()> deleter
) {
	Handle handle{ generateHandle() };

	size_t index{ m_Deleters.size() };

	m_Deleters.emplace_back(deleter);

	m_HandleToIndex.insert({ handle, index });

	m_IndexToHandle.insert({ index, handle });

	return handle;
}

void DeletionQueue::removeDeleter(const Handle handleToDelete) {
	if (m_HandleToIndex.count(handleToDelete) == 0 || m_Deleters.size() == 0) {
		return;
	}

	size_t deletedIndex{ m_HandleToIndex[handleToDelete] };
	size_t lastIndex{ m_Deleters.size() - 1 };

	if (deletedIndex != lastIndex) {
		Handle lastHandle{ m_IndexToHandle[lastIndex] };

		m_HandleToIndex[lastHandle] = deletedIndex;
		m_IndexToHandle[deletedIndex] = lastHandle;

		m_Deleters[deletedIndex] = m_Deleters[lastIndex];
	}
	m_Deleters.pop_back();

	m_HandleToIndex.erase(handleToDelete);
	m_IndexToHandle.erase(lastIndex);

	freeHandle(handleToDelete);
}

void DeletionQueue::flush() {
	for (auto itt{ m_Deleters.rbegin() }; itt != m_Deleters.rend(); itt++) {
		(*itt)();
	}

	m_HandleToIndex.clear();
	m_IndexToHandle.clear();
	m_Deleters.clear();
	m_FreeHandles.clear();
	m_HandleCounter = 0;
}

DeletionQueue::Handle DeletionQueue::generateHandle() {
	DeletionQueue::Handle handle{};
	if (m_FreeHandles.empty()) {
		handle = m_HandleCounter;
		m_HandleCounter++;
	} else {
		handle = m_FreeHandles.back();
		m_FreeHandles.pop_back();
	}

	return handle;
}

void DeletionQueue::freeHandle(const DeletionQueue::Handle handle) {
	m_FreeHandles.emplace_back(handle);
}
