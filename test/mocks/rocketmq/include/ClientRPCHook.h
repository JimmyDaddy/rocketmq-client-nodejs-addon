#ifndef ROCKETMQ_STUB_CLIENTRPCHOOK_H
#define ROCKETMQ_STUB_CLIENTRPCHOOK_H

#include <string>

namespace rocketmq {

class SessionCredentials {
 public:
  SessionCredentials(const std::string& access_key,
                     const std::string& secret_key,
                     const std::string& ons_channel);

  const std::string& access_key() const;
  const std::string& secret_key() const;
  const std::string& ons_channel() const;

 private:
  std::string access_key_;
  std::string secret_key_;
  std::string ons_channel_;
};

class ClientRPCHook {
 public:
  explicit ClientRPCHook(const SessionCredentials& credentials);
  const SessionCredentials& credentials() const;

 private:
  SessionCredentials credentials_;
};

}

#endif
