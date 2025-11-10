#pragma once

#include <memory>
#include <mutex>
#include <functional>
#include <map>


template <typename T>
class TemplateManager
{
public:
    static T* GetInstance(void)
    {
        if (m_pInstance == nullptr)
        {
            std::lock_guard<std::mutex> lock(*m_pInstanceMutex);
            m_pInstance = std::shared_ptr<T>(new T());
        }
        return m_pInstance.get();
    }

    void AddDependentManager(unsigned int nManagerId, TemplateManager* pManager) { m_mapDependentMgr[nManagerId] = pManager; }
    bool WaitDependentManager(unsigned int nTimeOut, unsigned int nWaitInterval = 200);

    int GetCurrentJobQueueSize() const;
    int GetActivatedWorkerCount() const;
    int GetTotalWorkerCount() const;
    bool GetCurrentJobPoolInfo(_Out_ int& nActivatedWorkerCount, _Out_ int& nTotalWorkerCount, _Out_ int& nQueueSize);

protected:
    TemplateManager(int nClassIdx = -1)
        : m_bStarted(false)
        , m_bTerminateLoopThread(false)
        , m_nClassIdx(nClassIdx) {}
    virtual ~TemplateManager() {}

    bool m_bStarted;
    bool m_bTerminateLoopThread;
    int m_nClassIdx;

    std::map<int, TemplateManager*> m_mapDependentMgr; // m_mapDependentMgr[manager index] -> TemplateManager pointer

private:
    static std::shared_ptr<T> m_pInstance;
    static std::shared_ptr<std::mutex> m_pInstanceMutex;
};

template <typename T> std::shared_ptr<T> TemplateManager<T>::m_pInstance = nullptr;
template <typename T> std::shared_ptr<std::mutex> TemplateManager<T>::m_pInstanceMutex = std::make_shared<std::mutex>();


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
bool TemplateManager<T>::WaitDependentManager(unsigned int nTimeOut, unsigned int nWaitInterval /*= 200*/)
{
    bool bMgrStart = false;
    for each (auto it in m_mapDependentMgr)
    {
        bMgrStart = false;
        unsigned int nWaitTime = 0;
        TemplateManager* pMgr = it.second;
        if (pMgr)
        {
            while (nWaitTime <= nTimeOut)
            {
                if (!pMgr->IsStarted())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(nWaitInterval));
                    nWaitTime += nWaitInterval;
                }
                else
                {
                    bMgrStart = true;
                    break;
                }
            }

            if (!bMgrStart)
                break;
        }
    }
    return bMgrStart;
}

template<typename T>
int TemplateManager<T>::GetCurrentJobQueueSize() const
{
    return (m_pJobPool != nullptr) ? m_pJobPool->getCurrentQueueSize() : -1;
}

template<typename T>
int TemplateManager<T>::GetActivatedWorkerCount() const
{
    return (m_pJobPool != nullptr) ? m_pJobPool->getActivatedWorkerCount() : -1;
}

template<typename T>
int TemplateManager<T>::GetTotalWorkerCount() const
{
    return (m_pJobPool != nullptr) ? m_pJobPool->getTotalWorkerCount() : -1;
}

template<typename T>
bool TemplateManager<T>::GetCurrentJobPoolInfo(_Out_ int& nActivatedWorkerCount, _Out_ int& nTotalWorkerCount, _Out_ int& nQueueSize)
{
    if (!m_pJobPool)
        return false;
    else
    {
        nActivatedWorkerCount = m_pJobPool->getActivatedWorkerCount();
        nTotalWorkerCount = m_pJobPool->getTotalWorkerCount();
        nQueueSize = m_pJobPool->getCurrentQueueSize();
        return true;
    }
}
