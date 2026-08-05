#pragma once

#include <QSettings>

#include <string_view>

namespace pcm {

class AppLockService {
 public:
  explicit AppLockService(QSettings *settings = nullptr);

  bool configure(std::string_view credential);
  bool verify(std::string_view credential) const;
  bool isConfigured() const;
  void disable();

 private:
  bool isValidCredential(std::string_view credential) const;

  QSettings mDefaultSettings;
  QSettings *mSettings = nullptr;
};

} // namespace pcm
