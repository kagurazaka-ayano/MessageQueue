/**
 * @file ManagerTest.cpp
 * @author ayano
 * @date 1/26/24
 * @brief
*/

#include "kawaiiMQ.h"
#include "gtest/gtest.h"
#include <future>

TEST(ManagerTest, SingletonMiltiThreadTest) {
    auto result1 = std::async(std::launch::async, [](){
        return KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();
    });
    auto result2 = std::async(std::launch::async, [](){
        return KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();
    });
    EXPECT_EQ(result1.get(), result2.get());
}

TEST(ManagerTest, RelateAndUnrelate) {
    auto topic1 = KawaiiMQ::Topic("topic1");
    auto topic2 = KawaiiMQ::Topic("topic2");
    auto queue11 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test11");
    auto queue12 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test12");
    auto queue21 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test21");
    auto queue22 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test22");

    auto manager = KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();
    manager->relate(topic1, queue11);
    manager->relate(topic1, queue12);
    manager->relate(topic2, queue21);
    manager->relate(topic2, queue22);
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 2);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 2);
    manager->unrelate(topic1, queue11);
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 1);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 2);
    manager->unrelate(topic1, queue12);
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 0);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 2);
    manager->unrelate(topic2, queue21);
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 0);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 1);
    manager->unrelate(topic2, queue22);
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 0);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 0);
}

TEST(ManagerTest, RelateAndUnrelateMultiThread) {
    auto topic1 = KawaiiMQ::Topic("topic1");
    auto topic2 = KawaiiMQ::Topic("topic2");
    auto queue11 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test11");
    auto queue12 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test12");
    auto queue21 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test21");
    auto queue22 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test22");

    auto manager = KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();
    auto result1 = std::async(std::launch::async, [&](){
        manager->relate(topic1, queue11);
        manager->relate(topic1, queue12);
        manager->relate(topic2, queue21);
        manager->relate(topic2, queue22);
    });
    auto result2 = std::async(std::launch::async, [&](){
        manager->unrelate(topic1, queue11);
        manager->unrelate(topic1, queue12);
        manager->unrelate(topic2, queue21);
        manager->unrelate(topic2, queue22);
    });
    result1.get();
    result2.get();
    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 0);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 0);
}

TEST(ManagerTest, ConcurrentAccess) {
    auto topic1 = KawaiiMQ::Topic("topic1");
    auto topic2 = KawaiiMQ::Topic("topic2");
    auto queue1 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test1");
    auto queue2 = KawaiiMQ::Queue<KawaiiMQ::IntMessage>("test2");

    auto manager = KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();

    std::thread t1([&]() {
        manager->relate(topic1, queue1);
        manager->relate(topic2, queue2);
    });

    std::thread t2([&]() {
        manager->unrelate(topic1, queue1);
        manager->unrelate(topic2, queue2);
    });

    t1.join();
    t2.join();

    EXPECT_EQ(manager->getAllRelatedQueue(topic1).size(), 0);
    EXPECT_EQ(manager->getAllRelatedQueue(topic2).size(), 0);
}

TEST(ProducerConsumerTest, HighDataFlowWithSingleProducerMultipleConsumers) {
    KawaiiMQ::Producer<KawaiiMQ::IntMessage> producer;
    KawaiiMQ::Consumer<KawaiiMQ::IntMessage> consumer1, consumer2, consumer3;
    KawaiiMQ::Topic topic1{"topic1"};
    KawaiiMQ::Queue<KawaiiMQ::IntMessage> queue;
    auto manager = KawaiiMQ::MessageQueueManager<KawaiiMQ::IntMessage>::Instance();
    manager->flush();
    manager->relate(topic1, queue);
    producer.subscribe(topic1);
    consumer1.subscribe(topic1);
    consumer2.subscribe(topic1);
    consumer3.subscribe(topic1);

    const int dataFlow = 10000;
    int ans = 0;
    for (int i = 0; i < dataFlow; ++i) {
        producer.publishMessage(topic1, KawaiiMQ::IntMessage(i));
        ans += i;
    }

    auto t1 = std::async(([&]() {
        int ret = 0;
        for (int i = 0; i < dataFlow / 3; ++i)
            ret += consumer1.fetchSingleTopic(topic1)[0]->getContent();
        return ret;
    }));
    auto t2 = std::async(([&]() {
        int ret = 0;
        for (int i = 0; i < dataFlow / 3; ++i)
            ret += consumer2.fetchSingleTopic(topic1)[0]->getContent();
        return ret;
    }));
    auto t3 = std::async(([&]() {
        int ret = 0;
        for (int i = 0; i < dataFlow - (dataFlow / 3) * 2; ++i)
            ret += consumer3.fetchSingleTopic(topic1)[0]->getContent();
        return ret;
    }));

    ASSERT_EQ(t1.get() + t2.get() + t3.get(), ans);
    manager->unrelate(topic1, queue);
}
