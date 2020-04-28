#ifndef THREADSAFE_UNORDERED_MAP_HPP__
#define THREADSAFE_UNORDERED_MAP_HPP__

#include <mutex>
#include <vector>
#include <unordered_map>

template<typename Key, typename Value>
class ThreadsafeUnorderedMap
{
public:
    bool find(Key k)
    {
        std::unique_lock<std::mutex> lck(mtx);
        return map.find(k) != map.end();
    }

    void insert(Key k, Value v)
    {
        std::unique_lock<std::mutex> lck(mtx);
        map[k] = v;
    }

    Value get(Key k)
    {
        std::unique_lock<std::mutex> lck(mtx);
        return map[k];
    }

    void erase(Key k)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if(map.find(k) != map.end())
        {
            map.erase(k);
        }
    }

    std::vector<Key> getAllKey()
    {
        std::unique_lock<std::mutex> lck(mtx);
        std::vector<Key> res;
        for(const auto & p : map)
        {
            res.push_back(p.first);
        }
        return res;
    }

    std::vector<Value> getAllValue()
    {
        std::unique_lock<std::mutex> lck(mtx);
        std::vector<Value> res;
        for(const auto & p : map)
        {
            res.push_back(p.second);
        }
        return res;
    }

private:
    std::mutex mtx;
    std::unordered_map<Key, Value> map;
};

#endif