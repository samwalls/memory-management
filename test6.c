#include <unistd.h>
#include "myalloc.h"

int main() {
    int size = getpagesize() / 3;
    void *a = (void*)myalloc(size);
    void *b = (void*)myalloc(size);
    void *c = (void*)myalloc(size * 10);
    void *d = (void*)myalloc(size);
    void *e = (void*)myalloc(size);
    myfree(e);
    myfree(d);
    myfree(c);
    myfree(b);
    myfree(a);
    //should result in two deallocations, leaving the root page behind
}
