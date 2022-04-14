#pragma once

#include <type_traits>
#include <mutex>
#include <map>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
 
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
 
    struct Access {
        private:
        std::lock_guard<std::mutex> guard;  // class fields are initialized in declaration order

        public:
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {
        }

        Access(const Access&) = delete;
        Access& operator=(const Access&) = delete;
    };
 
    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    ConcurrentMap(const ConcurrentMap&) = delete;
    ConcurrentMap& operator=(const ConcurrentMap&) = delete;
 
    Access operator[](const Key& key) {
        auto& bucket = GetBucket(key);
        return {key, bucket};
    }

    size_t erase(const Key& key) {
        Bucket& bucket = GetBucket(key);
        std::lock_guard guard(bucket.mutex);
        return bucket.map.erase(key);
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }
 
private:
    std::vector<Bucket> buckets_;

    Bucket& GetBucket(const Key& key) {
        return buckets_[static_cast<uint64_t>(key) % buckets_.size()];
    }
};
