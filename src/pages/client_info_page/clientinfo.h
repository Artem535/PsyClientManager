#pragma once

// === Qt ===
#include <QCheckBox>
#include <QDateTime>
#include <QLineEdit>
#include <QModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

// === STL ===
#include <memory>
#include <optional>

// === Local ===
#include "clientmodel.h"
#include "database.h"

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
  ObxClient getClientFromUi() const;
  int countAge(const QDate &birthDate) const;

  std::unique_ptr<Ui::ClientInfo> mUi;
  std::shared_ptr<QClientModel> mClientModel;
  std::optional<ObxClient> mEditingClient;
  bool mInEditMode = false;
};
