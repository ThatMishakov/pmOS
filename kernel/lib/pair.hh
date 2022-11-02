#pragma once

namespace klib {

template<class T1, class T2>
struct Pair {
    T1 first;
    T2 second;

    bool operator<(const Pair<T1,T2>& p)
    {
        if (p.first == this->first) return this->second < p.second;
        return this->first < p.first;
    }
};

}