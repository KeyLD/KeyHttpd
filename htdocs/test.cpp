/* ***********************************************************************
    > File Name: test.cpp
    > Author: Key
    > Mail: keyld777@gmail.com
    > Created Time: Wed 11 Apr 2018 09:22:51 PM CST
*********************************************************************** */
#include <unistd.h>
#include <iostream>
#define each(i, n) for (int(i) = 0; (i) < (n); (i)++)
#define reach(i, n) for (int(i) = n - 1; (i) >= 0; (i)--)
#define range(i, st, en) for (int(i) = (st); (i) <= (en); (i)++)
#define rrange(i, st, en) for (int(i) = (en); (i) >= (st); (i)--)
#define fill(ary, num) memset((ary), (num), sizeof(ary))

typedef long long ll;

int main()
{
    execl("/bin/ruby", "ruby", "test.rb", 0);
    //execl("test.rb", "test.rb", 0);
    printf("aaaa\n");
    return 0;
}
