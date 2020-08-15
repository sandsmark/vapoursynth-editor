#ifndef JOB_EDIT_DIALOG_H_INCLUDED
#define JOB_EDIT_DIALOG_H_INCLUDED

#include <ui_job_edit_dialog.h>
#include <QDebug>

#include "../../../common-src/settings/settings_definitions_core.h"

class SettingsManager;
class VSScriptLibrary;

class JobEditDialog : public QDialog
{
    Q_OBJECT

public:

    JobEditDialog(SettingsManager *a_pSettingsManager,
                  VSScriptLibrary *a_pVSScriptLibrary, QWidget *a_pParent = nullptr);

    virtual ~JobEditDialog();

    JobProperties jobProperties() const;

public slots:

    int call(const QString &a_title, const JobProperties &a_jobProperties);

private slots:

    void slotJobTypeChanged(int a_index);
    void slotEncodingScriptBrowseButtonClicked();
    void slotEncodingPresetComboBoxActivated(const QString &a_text);
    void slotEncodingPresetSaveButtonClicked();
    void slotEncodingPresetDeleteButton();
    void slotEncodingExecutableBrowseButtonClicked();
    void slotEncodingFramesFromVideoButtonClicked();
    void slotEncodingArgumentsHelpButtonClicked();
    void slotProcessExecutableBrowseButtonClicked();

private:

    void setUpEncodingPresets();

    QString chooseExecutable(const QString &a_dialogTitle,
                             const QString &a_initialPath = QString());

    Ui::JobEditDialog m_ui;

    SettingsManager *m_pSettingsManager;
    VSScriptLibrary *m_pVSScriptLibrary;

    QVector<EncodingPreset> m_encodingPresets;
};

#endif // JOB_EDIT_DIALOG_H_INCLUDED
