#pragma once

#include "database.h"
#include <QAbstractListModel>
#include <QMetaType>
#include <QVariant>
#include <memory>
#include <qnamespace.h>
#include <qvariant.h>

Q_DECLARE_METATYPE(Client)

class ClientModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum ClientRoles { Id = Qt::UserRole + 1, Full_object };
  explicit ClientModel(std::shared_ptr<pcm::database::Database> db,
                       QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;

  void add_new_client(const Client &new_client);

private:
  std::shared_ptr<pcm::database::Database> m_db;
  std::vector<obx_id> m_client_ids;
};
