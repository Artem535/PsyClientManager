#pragma once

// === Qt ===
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QVariant>
#include <QWidget>

// === STL ===
#include <memory>
#include <optional>

// === Local ===
#include "database.h"
#include "qclient_delegate.h"
#include "qclient_model.h"

namespace Ui {
class ClientInfo;
}

class ClientInfo final : public QWidget {
  Q_OBJECT

public:
  explicit ClientInfo(std::shared_ptr<QClientModel> model,
                      QWidget *parent = nullptr);
  ~ClientInfo() override;

signals:
  void displayButtonClicked(const std::optional<ObxClient> &client);

private:
  std::unique_ptr<Ui::ClientInfo> mUi;
  std::shared_ptr<QClientModel> mClientModel;

  void connectSignals(const QClientDelegate *delegate);
};
