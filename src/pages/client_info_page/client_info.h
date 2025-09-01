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
  void end_edit();

private slots:
  void onClientSelected(const QModelIndex &index);
  void onEditModeToggled(bool checked);
  void onUpdateClicked();
  void onAddClicked();
  void onClearClicked();
  void onDateChanged(const QDate &date);
  void updateButtonState();

private:
  void connectSignals();
  void updateUiFromClient(const ObxClient &client);
  void clearUi();
  bool validateInput();
  [[nodiscard]] ObxClient getClientFromUi() const;
  static int countAge(const QDate &birthDate);

  std::unique_ptr<Ui::ClientInfo> mUi;
  std::shared_ptr<QClientModel> mClientModel;
  std::optional<ObxClient> mEditingClient;
  bool mInEditMode = false;
};
