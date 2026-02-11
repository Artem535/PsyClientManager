#pragma once

#include <QAbstractListModel>
#include <QLoggingCategory>
#include <QMetaType>
#include <QString>
#include <QThread>
#include <QVariant>

#include <memory>
#include <vector>

#include "database.h"

Q_DECLARE_METATYPE(ObxClient)
Q_DECLARE_METATYPE(std::vector<ObxClient>)

class QClientModel final : public QAbstractListModel {
  Q_OBJECT

public:
  enum ClientRoles { Id = Qt::UserRole + 1, Full_object };
  explicit QClientModel(std::shared_ptr<pcm::database::Database> db,
                        QObject *parent = nullptr);
  ~QClientModel() override;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;

  void add_new_client(const ObxClient &new_client);
  void reload();
  bool isLoading() const;

signals:
  void loadingStateChanged(bool isLoading);
  void loadFailed(const QString &message);
  void requestLoad();

private:
  void setLoading(bool isLoading);

  std::shared_ptr<pcm::database::Database> m_db;
  QThread *m_loaderThread{nullptr};
  std::vector<ObxClient> m_clients;
  std::vector<int64_t> m_client_ids;
  bool m_isLoading{false};
};
