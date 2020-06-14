#ifndef BENCHMARK_DIALOG_H_INCLUDED
#define BENCHMARK_DIALOG_H_INCLUDED

#include <ui_benchmark_dialog.h>

#include "../vapoursynth/vs_script_processor_dialog.h"
#include "../../../common-src/chrono.h"

#ifdef Q_OS_WIN
class QWinTaskbarButton;
class QWinTaskbarProgress;
#endif

class ScriptBenchmarkDialog : public VSScriptProcessorDialog
{
    Q_OBJECT

public:

    ScriptBenchmarkDialog(SettingsManager *a_pSettingsManager,
                          VSScriptLibrary *a_pVSScriptLibrary,
                          QWidget *a_pParent = nullptr);
    virtual ~ScriptBenchmarkDialog();

    virtual bool initialize(const QString &a_script,
                            const QString &a_scriptName) override;

    void resetSavedRange();

public slots:

    void call();

protected slots:

    virtual void slotWriteLogMessage(int a_messageType,
                                     const QString &a_message) override;

    virtual void slotReceiveFrame(int a_frameNumber, int a_outputIndex,
                                  const VSFrameRef *a_cpOutputFrameRef,
                                  const VSFrameRef *a_cpPreviewFrameRef) override;

    virtual void slotFrameRequestDiscarded(int a_frameNumber,
                                           int a_outputIndex, const QString &a_reason) override;

    void slotWholeVideoButtonPressed();

    void slotStartStopBenchmarkButtonPressed();

protected:

    virtual void stopAndCleanUp() override;

    void stopProcessing();

    void updateMetrics();

    Ui::ScriptBenchmarkDialog m_ui;

    bool m_processing;

    int m_framesTotal;
    int m_framesProcessed;
    int m_framesFailed;

    hr_time_point m_benchmarkStartTime;

    int m_lastFromFrame;
    int m_lastToFrame;

#ifdef Q_OS_WIN
    QWinTaskbarButton *m_pWinTaskbarButton;
    QWinTaskbarProgress *m_pWinTaskbarProgress;
#endif
};

#endif // BENCHMARK_DIALOG_H_INCLUDED
