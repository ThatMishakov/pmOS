// TODO: This file should be somewhere else

#pragma once

template<typename RandomIterator, class T, class Compare>
RandomIterator lower_bound(RandomIterator begin, RandomIterator end, const T &value, Compare comp)
{
    while (begin < end) {
        RandomIterator mid = begin + ((end - begin) / 2);

        if (comp(*mid, value)) {
            begin = mid + 1;
        } else {
            end = mid;
        }
    }
    return begin;
}
