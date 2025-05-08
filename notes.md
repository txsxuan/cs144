# check0

## 配置环境
谁能想到，谁能想到，这一步是耗费我时间最长的……为了配置这个工具链，我先是弄坏了我的llvm工具链，导致ubuntu的图形界面直接崩溃，后来折腾了快一天，之后更是抽象，直接连grub引导都坏了。也算是长了一点知识，比如进入tty的时候可以重新安装桌面管理器gdm，比如在grub坏掉的时候可以插入启动盘，然后进入试用ubuntu，将主要的几个磁盘挂载起来再重新安装grub，还是有办法的嘛……
最后选择在wsl中，使用llvm16工具链，主要用到了clang-16,clang++-16,clang-tidy-16,clang-format-16
## 为wsl设置代理
这也折腾死我了，最后想了个办法，直接下一个xray，写写配置文件就行，easy
## 阅读代码
按照文档要求，需要阅读utils目录下的代码
* file_descriptor.hh<br />
    这就是一个封装好的文件描述符，基本上是把原来的c系统调用包装成了c++类，其中最值得我学习的是以下几点
    * fcntl函数<br />
    file control，一般用来获取与文件相关的信息或者设置相关信息。原型是     int fcntl(int fd, int cmd, ... /* arg */ );
        * fd: linux的文件描述符
        * cmd:命令，可以设置文件或者获取文件相关的信息，支持的操作有很多，在这个文件里主要使用的是fcntl( fd, F_GETFL )，意思是获取文件相关的标志位。同理，将F_GETFL 改为F_SETFL就是设置相关的标志位，比如可以让文件是阻塞模式，也可以不是。 

    * 一种神奇的写法<br />
        想起之前阅读一个sdl的项目时发现的，一些大佬们喜欢在一个类里面方一个智能指针私有变量,这个智能指针可以用来实现RAII，又可以保证接口的灵活性，十分实用。

* socket.hh<br />
同样的是对c系统调用的包装。看代码会发现socket继承于FileDescriptor类
    * extern int socket (int __domain, int __type, int __protocol)<br />
    这是glibc中对linux系统调用的封装，主要接收三个参数。第一个用来指示协议族,例如AF_INET（IPv4）、AF_INET6（IPv6）、AF_UNIX（本地 Unix 套接字）<br />
    第二个用来标识套接字类型，可以是SOCK_STREAM（TCP 流式套接字）或者SOCK_DGRAM（UDP 数据报套接字）。<br />
    第三个参数用来表示协议类型，通常为0，默认匹配。
    * extern int shutdown (int __fd, int __how) __THROW;<br />

    | `__how` | 宏定义 | 作用 |
    |------------|----------------|-------------------------------|
    | `0` | `SHUT_RD` | 关闭**读取**方向，不能再接收数据，但仍可发送数据 |
    | `1` | `SHUT_WR` | 关闭**写入**方向，不能再发送数据，但仍可接收数据 |
    | `2` | `SHUT_RDWR` | 关闭**读写**方向，完全断开连接 |
    * 
## 编写webget
* http文件头要求结尾必须是"\r\n"
* 需要使用address，而框架代码支持一种用主机+端口的方式传入，由于会调用getaddrinfo，它要求第二个参数也就是端口号必须是字符串类型，这里比较坑
* 在最后一行的时候必须用两次"\r\n"，因为第二个\r\n用来表示http头的结束
* 通过
![图片](./imgs/check0webget.png)
## in-memory reliable byte stream
过程相比之前有了一些痛苦的地方，总的来说还是比较简单的也涨了一点芝士
* queue<br />
    很显然需要一个先进先出的数据结构，所以采用队列实现（不需要随机访问，而需要快速的从队首pop元素和从队尾存入一个元素。
* string_view<br />
    这是一个不需要复制字符串但是能访问字符串的类，它甚至可以接受一个c字符串，根据GPT所言，const std::string &是做不到这种的。需要注意的是，它并不会复制一个字符串，因此如果用一个临时变量来构造它会出现很危险的事情。比如string_view{queue.front().substr}，这里的substr只会返回一个临时的字符串
* peek<br />
    这是这个问题的真正关键，也是决定了速度。
    这里涉及了一个string_view::remove_prefix的概念，记录队首被删掉的字符，如果整个字符被删掉再pop出来

# check1
## 使用socket发送udp报文<br />
这个任务非常的简单，唯一掌握的新知识就是报文要一个一个字节的写，同时，checksum为0可以认为是不需要检验，否则都需要手动计算，还有length字段一定不能错！

![图片](./imgs/udplen.png)
## ressembler
目前写的最红温的一个部分，写了一大堆代码，遇到各种问题（其实大部分是因为没有充分理解需求导致的），打算之后看看大佬的实现，这里测试过了，先不管了。

![图片](./imgs/check1test.png)
最大的收获是阅读了一部分测试的代码，有空的话需要学习这部分测试代码

> 2025-04-13 13:40<br />

换成了ubuntu真机，本周开始继续努力
成功修改了部分代码，简洁了其实现——原来的版本是当有合适的数据报到来时先将数据写入，再去判断缓存中的数据是否需要修改。现在略加改进，每次到来一个新的数据报时，将其与缓存中的数据合并，再决定是否需要上传。

> 2025-05-07 09:22 by konakona<br>

清明之后成功摆烂了一个月，什么都没干（），感觉多少有点幽默了。现在重新捡起来，今天目标完成check2.
# check2
## Translating between 64-bit indexes and 32-bit seqnos
在check1中我实现了用于处理编号从0开始的以64位二进制存储的数据报的ressembler（似乎是复用和解复用？）。如果在实际场景中，数据报也用64位二进制存储的话，基本可以认为是永远不会重复的。`A 64-bit index is
big enough that we can treat it as never overflowing`
但是，为了节省空间，我们实际上只能用32位的二进制来存储，这意味者我至少需要解决以下问题
* `Your implementation needs to plan for 32-bit integers to wrap around.`
    32位的seqno必然会循环，所以$2^{32}-1$之后的数据流的index应该是$0$，也就是说我需要对真实的64位的absseqno取模。同时，为了防止一些不可预料的事情发生，或许需要控制发送窗口的大小。
* ` TCP sequence numbers start at a random value`
    为了足够的区分，比如让每一次建立连接都看起来不同，TCP的seqno选择从一个随机数开始，按照之前上课所学，TCP的三次握手本质上就是在确认二者的seqno起始，相当于各自维护一边，从而维护整个连接的。数据流从zero point开始，而这个zero point是$2^{32}-1$以内的一个随机数，又被成为SYN或者ISN(Initial SequenceNumber)，之后数据流中的seqno就是$(SYN+1) \ mod\ 2^{32},(SYN+2) \ mod\ 2^{32}...$
* `The logical beginning and ending each occupy one sequence number`
    为了确保一个流被完整地接收,需要额外维护数据流的开始和结束，分别给他们一个seqno，即使他们不包含任何有效的数据。数据流的开始就是SYN，结束则是FIN.` Keep
in mind that SYN and FIN aren’t part of the stream itself and aren’t “bytes”——they represent the beginning and ending of the byte stream itself.`
基于上述讨论，实际上已经可以确定接下来需要做的事，那就是写一个Wrapper32，它至少需要满足上述三个要求，除此之外，Wrapper32还需要提供seqno，absolute seqno，和stream index之间的转换。
例如

|*elment*|*SYN*|c|a|t|FIN|
|:-:|:-:|:-:|:-:|:-:|:-:|
|**seqno**|$2^{32}-2$|$2^{32}-1$|$0$|$1$|$2$|
|**absolute seqno**|$0$|$1$|$2$|$3$|$4$|
|**stream index**||0|1|2||

关于absolute seqno和stream index的转换其实很简单，无非就是加上或减去1，但是涉及到seqno就比较麻烦了，不但要考虑到SYN，还要考虑一个seqno可能对应很多的abs seqno，因此必须提供相关的接口。
学了这么久，猛地发现自己白学了（，我在今天才意识到，类的公有私有和类的实例是无关的，也就是说即使是私有的

> 2025-05-08 10:02 by konakona <br />

昨晚头昏脑涨想了很久，最后还是参考了别人的代码才写成，总的来说我对这部分的代码的了解还是太少了，写的很差，很多函数的功能都想不到。
但是仍然有一些比较疑惑待解决的点
- [ ] 为什么checkpoint可以直接用ackno，感觉太草率了，有运气的嫌疑
- [ ] 这个测试代码是怎么实现多态的，模板套模板真的给我看傻了

