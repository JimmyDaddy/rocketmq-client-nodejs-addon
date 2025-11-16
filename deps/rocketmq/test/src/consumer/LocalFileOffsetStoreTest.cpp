#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <stdexcept>

#include "MQMessageQueue.h"
#include "OffsetStore.h"
#include "consumer/LocalFileOffsetStore.h"

namespace {

std::string CreateTempDir() {
  std::string pattern = "/tmp/rmq_offset_store_testXXXXXX";
  std::vector<char> path(pattern.begin(), pattern.end());
  path.push_back('\0');
  char* dir = mkdtemp(path.data());
  if (dir == nullptr) {
    throw std::runtime_error("mkdtemp failed");
  }
  return std::string(dir);
}

void RemovePath(const std::string& path) {
  DIR* dir = opendir(path.c_str());
  if (!dir) {
    std::remove(path.c_str());
    return;
  }
  dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    std::string child = path + FILE_SEPARATOR + entry->d_name;
    RemovePath(child);
  }
  closedir(dir);
  rmdir(path.c_str());
}

std::string OffsetsFilePath(const std::string& storeDir) {
  return storeDir + FILE_SEPARATOR + "offsets.json";
}

}  // namespace

class LocalFileOffsetStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base_dir_ = CreateTempDir();
    override_dir_ = base_dir_ + FILE_SEPARATOR + "store";
  }

  void TearDown() override { RemovePath(base_dir_); }

  std::string base_dir_;
  std::string override_dir_;
};

TEST_F(LocalFileOffsetStoreTest, PersistAndLoadOffsets) {
  rocketmq::LocalFileOffsetStore store(nullptr, "GID_test", override_dir_);
  rocketmq::MQMessageQueue mq("TestTopic", "brokerA", 3);
  store.updateOffset(mq, 123456, false);
  store.persist(mq);

  rocketmq::LocalFileOffsetStore reloaded(nullptr, "GID_test", override_dir_);
  reloaded.load();
  EXPECT_EQ(123456, reloaded.readOffset(mq, rocketmq::READ_FROM_MEMORY));
}

TEST_F(LocalFileOffsetStoreTest, FallsBackToBackupFileWhenMainMissing) {
  rocketmq::LocalFileOffsetStore store(nullptr, "GID_test", override_dir_);
  rocketmq::MQMessageQueue mq("TestTopic", "brokerA", 0);
  store.updateOffset(mq, 10, false);
  store.persist(mq);
  store.updateOffset(mq, 20, false);
  store.persist(mq);

  std::string offsets_file = OffsetsFilePath(override_dir_);
  ASSERT_EQ(0, std::remove(offsets_file.c_str())) << std::strerror(errno);

  rocketmq::LocalFileOffsetStore reader(nullptr, "GID_test", override_dir_);
  EXPECT_EQ(10, reader.readOffset(mq, rocketmq::READ_FROM_STORE));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LocalFileOffsetStoreTest.*";
  return RUN_ALL_TESTS();
}
