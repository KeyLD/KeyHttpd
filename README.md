# KeyHttpd
A Tiny Httpd By NodeKey

一个基于epoll的简单HTTP服务器，使用Ruby作cgi，支持GET与POST方法

如若不更改cgi条件下，本机需安装Ruby

## 使用

```shell
git clone https://github.com/KeyLD/KeyHttpd.git
cd KeyHttpd
make
./server
```
然后打开浏览器输入`localhost:8080`即可。

## 服务器工作流程
1. 服务器启动，建立套接字，绑定端口，监听
2. 创建epoll，将服务器套接字加入epoll
3. 受到一个连接请求时，将客户端加入epoll并监听EPOLLIN事件（这时实际上是立即触发的
4. 对于EPOLLIN事件，先解析其头部，头部中最重要的是首行，获取请求方法，路径，版本。
5. 对路径uri解析，主要目的是判断是否可以直接加载静态文件。如果是GET方法并且没有参数，则可以直接加载静态文件，否则不可以。并将get方法的参数记录下来。
6. 有选择地对其他头部进行筛选（对我这个简单的服务器来说一般没什么用）。对于body部分，如果是get方法就不用读了，如果是post方法则将其记录下来作为参数。请求解析好后，将事件修改为EPOLLOUT（这时也是立即触发的
7. 这里我也参用管道的方式，建立两个管道实现全双工管道，之后进行fork。 先将子进程的输入输出重定位，并让子进程去执行cgi脚本，这样就让子进程输出到管道上。
8. 从管道上读取数据并返回给客户端。
9. 关闭连接，完成一次HTTP的请求与回应，并将该事件修改为EPOLLIN，等待下一次请求。

## 致谢
1. 架构设计上借鉴该[博客](https://blog.csdn.net/fangjian1204/article/details/34415651)
2. 实现上借鉴[TinyHttpd](https://github.com/EZLippi/Tinyhttpd)
