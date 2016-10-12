#include <unistd.h>
#include "myalloc.h"

int main() {
    int size = getpagesize() / 3;
    void *a = (void*)myalloc(size);
    void *b = (void*)myalloc(size);
    void *c = (void*)myalloc(size * 10);
    void *d = (void*)myalloc(size);
    void *e = (void*)myalloc(size);
    myfree(d);
    myfree(c);
    myfree(e);
    myfree(b);
    myfree(a);
    //freeing c should result in a coalesce and a deallocation
    //at this point there should be no memory allocated, try mallocing again...
    void *tst = (void*)myalloc(size);
    void *tst2 = (void*)myalloc(size);
    myfree(tst2);
    myfree(tst);
}
