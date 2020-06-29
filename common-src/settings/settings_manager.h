#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include "settings_manager_core.h"
#include "settings_definitions.h"

#include <QByteArray>
#include <QTextCharFormat>
#include <QColor>
#include <QAction>

//==============================================================================

class SettingsManager : public SettingsManagerCore
{
public:

    SettingsManager(QObject *a_pParent);
    virtual ~SettingsManager();

    //----------------------------------------------------------------------

    QAction *createStandardAction(const QString &a_actionID,
                                  QObject *a_pParent);

    std::vector<StandardAction> getStandardActions() const;

    QKeySequence getDefaultHotkey(const QString &a_actionID) const;

    QKeySequence getHotkey(const QString &a_actionID) const;

    bool setHotkey(const QString &a_actionID,
                   const QKeySequence &a_hotkey);

    //----------------------------------------------------------------------

    QTextCharFormat getDefaultTextFormat(const QString &a_textFormatID)
    const;

    QTextCharFormat getTextFormat(const QString &a_textFormatID) const;

    bool setTextFormat(const QString &a_textFormatID,
                       const QTextCharFormat &a_format);

    //----------------------------------------------------------------------

    QColor getDefaultColor(const QString &a_colorID) const;

    QColor getColor(const QString &a_colorID) const;

    bool setColor(const QString &a_colorID, const QColor &a_color);

    //----------------------------------------------------------------------

    QString getLastUsedPath() const;

    bool setLastUsedPath(const QString &a_lastUsedPath);

    QByteArray getMainWindowGeometry() const;

    bool setMainWindowGeometry(const QByteArray &a_mainWindowGeometry);

    bool getMainWindowMaximized() const;

    bool setMainWindowMaximized(bool a_mainWindowMaximized);

    QByteArray getPreviewDialogGeometry() const;

    bool setPreviewDialogGeometry(const QByteArray &a_previewDialogGeometry);

    bool getPreviewDialogMaximized() const;

    bool setPreviewDialogMaximized(bool a_previewDialogMaximized);

    QByteArray getJobServerWatcherGeometry() const;

    bool setJobServerWatcherGeometry(const QByteArray &a_geometry);

    bool getJobServerWatcherMaximized() const;

    bool setJobServerWatcherMaximized(bool a_maximized);

    QByteArray getJobsHeaderState() const;

    bool setJobsHeaderState(const QByteArray &a_headerState);

    bool getAutoLoadLastScript() const;

    bool setAutoLoadLastScript(bool a_autoLoadLastScript);

    bool getZoomPanelVisible() const;

    bool setZoomPanelVisible(bool a_zoomPanelVisible);

    ZoomMode getZoomMode() const;

    bool setZoomMode(ZoomMode a_zoomMode);

    double getZoomRatio() const;

    bool setZoomRatio(double a_zoomRatio);

    Qt::TransformationMode getScaleMode() const;

    bool setScaleMode(Qt::TransformationMode a_scaleMode);

    CropMode getCropMode() const;

    bool setCropMode(CropMode a_cropMode);

    int getCropZoomRatio() const;

    bool setCropZoomRatio(int a_cropZoomRatio);

    bool getPromptToSaveChanges() const;

    bool setPromptToSaveChanges(bool a_prompt);

    QStringList getRecentFilesList() const;

    bool addToRecentFilesList(const QString &a_filePath);

    unsigned int getMaxRecentFilesNumber() const;

    bool setMaxRecentFilesNumber(unsigned int a_maxRecentFilesNumber);

    QStringList getVapourSynthDocumentationPaths() const;

    bool setVapourSynthDocumentationPaths(const QStringList &a_pathsList);

    int getCharactersTypedToStartCompletion() const;

    bool setCharactersTypedToStartCompletion(int a_charactersNumber);

    TimeLineSlider::DisplayMode getTimeLineMode() const;

    bool setTimeLineMode(TimeLineSlider::DisplayMode a_timeLineMode);

    double getTimeStep() const;

    bool setTimeStep(double a_timeStep);

    bool getColorPickerVisible() const;

    bool setColorPickerVisible(bool a_colorPickerVisible);

    PlayFPSLimitMode getPlayFPSLimitMode() const;

    bool setPlayFPSLimitMode(PlayFPSLimitMode a_mode);

    double getPlayFPSLimit() const;

    bool setPlayFPSLimit(double a_limit);

    bool getUseSpacesAsTab() const;

    bool setUseSpacesAsTab(bool a_value);

    int getSpacesInTab() const;

    bool setSpacesInTab(int a_spacesNumber);

    QString getTabText() const;

    bool getRememberLastPreviewFrame() const;

    bool setRememberLastPreviewFrame(bool a_remember);

    int getLastPreviewFrame() const;

    bool setLastPreviewFrame(int a_frameNumber);

    QString getDefaultNewScriptTemplate();

    QString getNewScriptTemplate();

    bool setNewScriptTemplate(const QString &a_text);

    std::vector<CodeSnippet> getAllCodeSnippets() const;

    CodeSnippet getCodeSnippet(const QString &a_name) const;

    bool saveCodeSnippet(const CodeSnippet &a_snippet);

    bool deleteCodeSnippet(const QString &a_name);

    std::vector<DropFileCategory> getAllDropFileTemplates() const;

    bool setDropFileTemplates(
        const std::vector<DropFileCategory> &a_categories);

    QString getDropFileTemplate(const QString &a_filePath) const;

    bool getHighlightSelectionMatches() const;

    bool setHighlightSelectionMatches(bool a_highlight);

    int getHighlightSelectionMatchesMinLength() const;

    bool setHighlightSelectionMatchesMinLength(int a_length);

    bool getTimeLinePanelVisible() const;

    bool setTimeLinePanelVisible(bool a_visible);

    bool getAlwaysKeepCurrentFrame() const;

    bool setAlwaysKeepCurrentFrame(bool a_keep);

    std::vector<TextBlockStyle> getLogStyles(const QString &a_logName) const;

    bool getUseDarkMode() const;

    bool setUseDarkMode(bool a_use);

    bool setLogStyles(const QString &a_logName,
                      const std::vector<TextBlockStyle> a_styles);

    QString getLastSnapshotExtension() const;

    bool setLastSnapshotExtension(const QString &a_extension);

private:

    void initializeStandardActions();

    std::vector<StandardAction> m_standardActions;
};

//==============================================================================

#endif // SETTINGSMANAGER_H
