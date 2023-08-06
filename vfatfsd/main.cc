#include <deque>
#include <cstdio>
#include <string>
#include <unistd.h>

const std::deque<std::string> v = {"Hello",
                                  "from",
                                  "vfatfsd!"};

int main() {
    for(auto i : v) {
        printf("%s ", i.c_str());
    }
    printf("\n");

    pid_t pid = fork();
    if(pid != 0) {
        printf("I'm the child!\n");
    } else {
        printf("I'm the parent!\n");
    }

    return 0;
}