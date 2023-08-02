#include <deque>
#include <cstdio>
#include <string>

const std::deque<std::string> v = {"Hello",
                                  "from",
                                  "vfatfsd!"};

int main() {
    for(auto i : v) {
        printf("%s ", i.c_str());
    }
    printf("\n");

    return 0;
}