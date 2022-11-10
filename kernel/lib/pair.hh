#pragma once

namespace klib {

template<class T1, class T2>
struct pair {
    T1 first;
    T2 second;

    bool operator<(const pair<T1,T2>& p)
    {
        if (p.first == this->first) return this->second < p.second;
        return this->first < p.first;
    }
};

}