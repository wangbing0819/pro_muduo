#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * =》 TcpConnection 设置回调 =》 Channel =》 Poller =》 Channel的回调操作
 * 
 */ 
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, 
                const std::string &name, 
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
private:
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_;           // 这里绝对不是baseLoop，因为TcpConnection都是在subLoop里面管理的
    const std::string name_;    // 保存已连接套接字文件描述符
    std::atomic_int state_;     // 封装已经建立连接的文件描述符以及各种事件发生时对应的回调函数
    bool reading_;

    // 这里和Acceptor类似   Acceptor=》mainLoop    TcpConenction=》subLoop
    std::unique_ptr<Socket> socket_;                   
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;                     //当前主机的ip和端口号
    const InetAddress peerAddr_;                      //远端地址

    // 这些回调函数都是用户自己定义的
    ConnectionCallback connectionCallback_;          // 有新连接时的回调
    MessageCallback messageCallback_;                // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;    // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;  // 接收数据的缓冲区 => 接收用户发过来的数据
    Buffer outputBuffer_; // 发送数据的缓冲区 => 用来保存暂时发生不出去的数据
    /*
    TCP的发送缓冲区也是有大小限制的，如果此时无法将数据一次性拷贝到TCP缓冲区当中，
    那么剩余的数据可以暂时保存在我们自己定义的缓冲区当中并将给文件描述对应的写事件注册到对应的Poller当中，
    等到写事件就绪了，在调用回调方法将剩余的数据发送给客户端。
    */
};