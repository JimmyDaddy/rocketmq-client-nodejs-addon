#include <gtest/gtest.h>

#include "TransactionListener.h"
#include "producer/TransactionMQProducerConfigImpl.hpp"

namespace {

class DummyTransactionListener : public rocketmq::TransactionListener {
 public:
  rocketmq::LocalTransactionState executeLocalTransaction(const rocketmq::MQMessage&, void*) override {
    return rocketmq::LocalTransactionState::COMMIT_MESSAGE;
  }

  rocketmq::LocalTransactionState checkLocalTransaction(const rocketmq::MQMessageExt&) override {
    return rocketmq::LocalTransactionState::UNKNOWN;
  }
};

}  // namespace

using rocketmq::DefaultMQProducerConfigImpl;
using rocketmq::TransactionListener;
using rocketmq::TransactionMQProducerConfigImpl;

TEST(TransactionMQProducerConfigImplTest, Defaults) {
  TransactionMQProducerConfigImpl config;
  EXPECT_EQ(nullptr, config.getTransactionListener());

  // Ensure base producer defaults still accessible
  EXPECT_EQ(4 * 1024 * 1024, config.max_message_size());
}

TEST(TransactionMQProducerConfigImplTest, StoresProvidedListenerPointer) {
  TransactionMQProducerConfigImpl config;
  DummyTransactionListener listener;

  config.setTransactionListener(&listener);
  EXPECT_EQ(&listener, config.getTransactionListener());

  // Confirm setters inherited from DefaultMQProducerConfigImpl remain functional
  config.set_max_message_size(1024);
  EXPECT_EQ(1024, config.max_message_size());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TransactionMQProducerConfigImplTest.*";
  return RUN_ALL_TESTS();
}
