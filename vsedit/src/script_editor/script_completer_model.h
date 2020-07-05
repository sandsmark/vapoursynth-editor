#ifndef SCRIPTCOMPLETERMODEL_H
#define SCRIPTCOMPLETERMODEL_H

#include "../vapoursynth/vs_plugin_data.h"

#include <QStandardItemModel>

class ScriptCompleterModel : public QStandardItemModel
{
    Q_OBJECT

public:

    ScriptCompleterModel(QObject *a_pParent = nullptr);

    virtual ~ScriptCompleterModel();

    void setPluginsList(const VSPluginsList &a_pluginsList);

    void setCoreName(const QString &a_coreName);

public slots:
    void onDefinesChanged(const QMap<QString, QString> &defines);

private:
    QMap<QString, QString> m_oldDefines;
    QString m_coreName;
};

#endif // SCRIPTCOMPLETERMODEL_H
