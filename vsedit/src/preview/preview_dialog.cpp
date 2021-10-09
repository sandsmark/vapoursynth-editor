#include "preview_dialog.h"

#include "../../../common-src/helpers.h"
#include "../../../common-src/vapoursynth/vapoursynth_script_processor.h"
#include "../../../common-src/settings/settings_manager.h"
#include "../settings/settings_dialog.h"
#include "scroll_navigator.h"
#include "../../../common-src/timeline_slider/timeline_slider.h"
#include "preview_advanced_settings_dialog.h"

#include <vapoursynth/VapourSynth.h>

#include <QEvent>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QStatusBar>
#include <QLabel>
#include <QToolTip>
#include <QCursor>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QPoint>
#include <QMenu>
#include <QActionGroup>
#include <QAction>
#include <QByteArray>
#include <QClipboard>
#include <QTimer>
#include <QImageWriter>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

//==============================================================================

#define BEGIN_CROP_VALUES_CHANGE \
	if(m_changingCropValues) \
		return; \
	m_changingCropValues = true;

#define END_CROP_VALUES_CHANGE \
	m_changingCropValues = false;

//==============================================================================

const char TIMELINE_BOOKMARKS_FILE_SUFFIX[] = ".bookmarks";

//==============================================================================

PreviewDialog::PreviewDialog(SettingsManager *a_pSettingsManager,
                             VSScriptLibrary *a_pVSScriptLibrary, QWidget *a_pParent) :
    VSScriptProcessorDialog(a_pSettingsManager, a_pVSScriptLibrary, a_pParent)
    , m_pAdvancedSettingsDialog(nullptr)
    , m_frameExpected(0)
    , m_frameShown(-1)
    , m_lastFrameRequestedForPlay(-1)
    , m_bigFrameStep(10)
    , m_cpFrameRef(nullptr)
    , m_changingCropValues(false)
    , m_pPreviewContextMenu(nullptr)
    , m_pActionFrameToClipboard(nullptr)
    , m_pActionSaveSnapshot(nullptr)
    , m_pActionToggleZoomPanel(nullptr)
    , m_pMenuZoomModes(nullptr)
    , m_pActionGroupZoomModes(nullptr)
    , m_pActionSetZoomModeNoZoom(nullptr)
    , m_pActionSetZoomModeFixedRatio(nullptr)
    , m_pActionSetZoomModeFitToFrame(nullptr)
    , m_pMenuZoomScaleModes(nullptr)
    , m_pActionGroupZoomScaleModes(nullptr)
    , m_pActionSetZoomScaleModeNearest(nullptr)
    , m_pActionSetZoomScaleModeBilinear(nullptr)
    , m_pActionToggleCropPanel(nullptr)
    , m_pActionToggleTimeLinePanel(nullptr)
    , m_pMenuTimeLineModes(nullptr)
    , m_pActionGroupTimeLineModes(nullptr)
    , m_pActionSetTimeLineModeTime(nullptr)
    , m_pActionSetTimeLineModeFrames(nullptr)
    , m_pActionTimeStepForward(nullptr)
    , m_pActionTimeStepBack(nullptr)
    , m_pActionPasteCropSnippetIntoScript(nullptr)
    , m_pActionAdvancedSettingsDialog(nullptr)
    , m_pActionToggleColorPicker(nullptr)
    , m_pActionPlay(nullptr)
    , m_pActionLoadChapters(nullptr)
    , m_pActionClearBookmarks(nullptr)
    , m_pActionBookmarkCurrentFrame(nullptr)
    , m_pActionUnbookmarkCurrentFrame(nullptr)
    , m_pActionGoToPreviousBookmark(nullptr)
    , m_pActionGoToNextBookmark(nullptr)
    , m_pActionPasteShownFrameNumberIntoScript(nullptr)
    , m_playing(false)
    , m_processingPlayQueue(false)
    , m_secondsBetweenFrames(0)
    , m_pPlayTimer(nullptr)
    , m_alwaysKeepCurrentFrame(DEFAULT_ALWAYS_KEEP_CURRENT_FRAME)
    , m_pGeometrySaveTimer(nullptr)
{
    m_ui.setupUi(this);
    setWindowIcon(QIcon(":preview.png"));

    m_iconPlay = QIcon(":play.png");
    m_iconPause = QIcon(":pause.png");

    m_pAdvancedSettingsDialog = new PreviewAdvancedSettingsDialog(
        m_pSettingsManager, this);

    m_pPlayTimer = new QTimer(this);
    m_pPlayTimer->setTimerType(Qt::PreciseTimer);
    m_pPlayTimer->setSingleShot(true);

    createActionsAndMenus();

    createStatusBar();
    m_pStatusBarWidget->setColorPickerVisible(
        m_pSettingsManager->getColorPickerVisible());

    m_ui.frameNumberSlider->setBigStep(m_bigFrameStep);
    m_ui.frameNumberSlider->setDisplayMode(
        m_pSettingsManager->getTimeLineMode());

    m_ui.frameToClipboardButton->setDefaultAction(m_pActionFrameToClipboard);
    m_ui.saveSnapshotButton->setDefaultAction(m_pActionSaveSnapshot);
    m_ui.advancedSettingsButton->setDefaultAction(
        m_pActionAdvancedSettingsDialog);

    setUpZoomPanel();
    setUpCropPanel();
    setUpTimeLinePanel();

    m_ui.colorPickerButton->setDefaultAction(m_pActionToggleColorPicker);

    m_pGeometrySaveTimer = new QTimer(this);
    m_pGeometrySaveTimer->setInterval(DEFAULT_WINDOW_GEOMETRY_SAVE_DELAY);
    connect(m_pGeometrySaveTimer, &QTimer::timeout,
            this, &PreviewDialog::slotSaveGeometry);

    m_windowGeometry = m_pSettingsManager->getPreviewDialogGeometry();

    if (!m_windowGeometry.isEmpty()) {
        restoreGeometry(m_windowGeometry);
    }

    connect(m_pAdvancedSettingsDialog, SIGNAL(signalSettingsChanged()),
            this, SLOT(slotAdvancedSettingsChanged()));
    connect(m_ui.frameNumberSlider, SIGNAL(signalFrameChanged(int)),
            this, SLOT(slotShowFrame(int)));
    connect(m_ui.frameNumberSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotShowFrame(int)));
    connect(m_ui.previewArea, SIGNAL(signalSizeChanged()),
            this, SLOT(slotPreviewAreaSizeChanged()));
    connect(m_ui.previewArea, SIGNAL(signalCtrlWheel(QPoint)),
            this, SLOT(slotPreviewAreaCtrlWheel(QPoint)));
    connect(m_ui.previewArea, SIGNAL(signalMouseMiddleButtonReleased()),
            this, SLOT(slotPreviewAreaMouseMiddleButtonReleased()));
    connect(m_ui.previewArea, SIGNAL(signalMouseRightButtonReleased()),
            this, SLOT(slotPreviewAreaMouseRightButtonReleased()));
    connect(m_ui.previewArea, SIGNAL(signalMouseOverPoint(float, float)),
            this, SLOT(slotPreviewAreaMouseOverPoint(float, float)));
    connect(m_pPlayTimer, SIGNAL(timeout()),
            this, SLOT(slotProcessPlayQueue()));

    slotSettingsChanged();

    bool rememberLastPreviewFrame =
        m_pSettingsManager->getRememberLastPreviewFrame();

    if (rememberLastPreviewFrame) {
        m_frameExpected = m_pSettingsManager->getLastPreviewFrame();
        setScriptName(m_pSettingsManager->getLastUsedPath());
    }
}

// END OF PreviewDialog::PreviewDialog(SettingsManager * a_pSettingsManager,
//		VSScriptLibrary * a_pVSScriptLibrary, QWidget * a_pParent)
//==============================================================================

PreviewDialog::~PreviewDialog()
{
    if (m_pGeometrySaveTimer->isActive()) {
        m_pGeometrySaveTimer->stop();
        slotSaveGeometry();
    }
}

// END OF PreviewDialog::~PreviewDialog()
//==============================================================================

void PreviewDialog::setScriptName(const QString &a_scriptName)
{
    VSScriptProcessorDialog::setScriptName(a_scriptName);
    setTitle();
}

// END OF void PreviewDialog::setScriptName(const QString & a_scriptName)
//==============================================================================

void PreviewDialog::previewScript(const QString &a_script,
                                  const QString &a_scriptName)
{
    QString previousScript = script();
    QString previousScriptName = scriptName();

    stopAndCleanUp();

    bool initialized = initialize(a_script, a_scriptName);

    if (!initialized) {
        return;
    }

    setTitle();

    int lastFrameNumber = m_cpVideoInfo->numFrames - 1;
    m_ui.frameNumberSpinBox->setMaximum(lastFrameNumber);
    m_ui.frameNumberSlider->setFramesNumber(m_cpVideoInfo->numFrames);

    if (m_cpVideoInfo->fpsDen == 0) {
        m_ui.frameNumberSlider->setFPS(0.0);
    } else {
        m_ui.frameNumberSlider->setFPS((double)m_cpVideoInfo->fpsNum /
                                       (double)m_cpVideoInfo->fpsDen);
    }

    bool scriptChanged = ((previousScript != a_script) &&
                          (previousScriptName != a_scriptName));

    if (scriptChanged && (!m_alwaysKeepCurrentFrame)) {
        m_frameExpected = 0;
        m_ui.previewArea->setPixmap(QImage());
    }

    if (m_frameExpected > lastFrameNumber) {
        m_frameExpected = lastFrameNumber;
    }

    resetCropSpinBoxes();

    slotSetPlayFPSLimit();

    setScriptName(a_scriptName);

    loadTimelineBookmarks();

    if (m_pSettingsManager->getPreviewDialogMaximized()) {
        showMaximized();
    } else {
        showNormal();
    }

    slotShowFrame(m_frameExpected);
}

// END OF void PreviewDialog::previewScript(const QString& a_script,
//		const QString& a_scriptName)
//==============================================================================

void PreviewDialog::stopAndCleanUp()
{
    slotPlay(false);

    if (m_ui.cropCheckButton->isChecked()) {
        m_ui.cropCheckButton->click();
    }

    bool rememberLastPreviewFrame =
        m_pSettingsManager->getRememberLastPreviewFrame();

    if (rememberLastPreviewFrame && (!scriptName().isEmpty()) &&
            (m_frameShown > -1)) {
        m_pSettingsManager->setLastPreviewFrame(m_frameShown);
    }

    m_frameShown = -1;
    m_framePixmap = QImage();
    // Replace shown image with a blank one of the same dimension:
    // -helps to keep the scrolling position when refreshing the script;
    // -leaves the image blank on sudden error;
    // -creates a blinking effect indicating the script is being refreshed.
    QImage blackPixmap(m_ui.previewArea->pixmapSize(), QImage::Format_Mono);
    blackPixmap.fill(Qt::black);
    m_ui.previewArea->setPixmap(blackPixmap);

    if (m_cpFrameRef) {
        Q_ASSERT(m_cpVSAPI);
        m_cpVSAPI->freeFrame(m_cpFrameRef);
        m_cpFrameRef = nullptr;
    }

    VSScriptProcessorDialog::stopAndCleanUp();
}

// END OF void PreviewDialog::stopAndCleanUp()
//==============================================================================

void PreviewDialog::moveEvent(QMoveEvent *a_pEvent)
{
    QDialog::moveEvent(a_pEvent);
    saveGeometryDelayed();
}

// END OF void PreviewDialog::moveEvent(QMoveEvent * a_pEvent)
//==============================================================================

void PreviewDialog::resizeEvent(QResizeEvent *a_pEvent)
{
    QDialog::resizeEvent(a_pEvent);
    saveGeometryDelayed();
}

// END OF void PreviewDialog::resizeEvent(QResizeEvent * a_pEvent)
//==============================================================================

void PreviewDialog::changeEvent(QEvent *a_pEvent)
{
    QDialog::changeEvent(a_pEvent);

    if (a_pEvent->type() == QEvent::WindowStateChange) {
        if (isMaximized()) {
            m_pSettingsManager->setPreviewDialogMaximized(true);
        } else {
            m_pSettingsManager->setPreviewDialogMaximized(false);
        }
    }
}

// END OF void PreviewDialog::changeEvent(QEvent * a_pEvent)
//==============================================================================

void PreviewDialog::keyPressEvent(QKeyEvent *a_pEvent)
{
    Qt::KeyboardModifiers modifiers = a_pEvent->modifiers();

    if (modifiers != Qt::NoModifier) {
        QDialog::keyPressEvent(a_pEvent);
        return;
    }

    if (!m_pVapourSynthScriptProcessor->isInitialized()) {
        QDialog::keyPressEvent(a_pEvent);
        return;
    }

    Q_ASSERT(m_cpVideoInfo);

    int key = a_pEvent->key();

    if (((key == Qt::Key_Left) || (key == Qt::Key_Down)) &&
            (m_frameExpected > 0)) {
        slotShowFrame(m_frameExpected - 1);
    } else if (((key == Qt::Key_Right) || (key == Qt::Key_Up)) &&
               (m_frameExpected < (m_cpVideoInfo->numFrames - 1))) {
        slotShowFrame(m_frameExpected + 1);
    } else if ((key == Qt::Key_PageDown) && (m_frameExpected > 0)) {
        slotShowFrame(std::max(0, m_frameExpected - m_bigFrameStep));
    } else if ((key == Qt::Key_PageUp) &&
               (m_frameExpected < (m_cpVideoInfo->numFrames - 1))) {
        slotShowFrame(std::min(m_cpVideoInfo->numFrames - 1,
                               m_frameExpected + m_bigFrameStep));
    } else if (key == Qt::Key_Home) {
        slotShowFrame(0);
    } else if (key == Qt::Key_End) {
        slotShowFrame(m_cpVideoInfo->numFrames - 1);
    } else if (key == Qt::Key_Escape) {
        close();
    } else {
        QDialog::keyPressEvent(a_pEvent);
    }
}

// END OF void PreviewDialog::keyPressEvent(QKeyEvent * a_pEvent)
//==============================================================================

void PreviewDialog::slotReceiveFrame(int a_frameNumber, int a_outputIndex,
                                     const VSFrameRef *a_cpOutputFrameRef,
                                     const VSFrameRef *a_cpPreviewFrameRef)
{
    if (!a_cpOutputFrameRef) {
        return;
    }

    Q_ASSERT(m_cpVSAPI);
    const VSFrameRef *cpOutputFrameRef =
        m_cpVSAPI->cloneFrameRef(a_cpOutputFrameRef);
    const VSFrameRef *cpPreviewFrameRef =
        m_cpVSAPI->cloneFrameRef(a_cpPreviewFrameRef);

    if (m_playing) {
        Frame newFrame(a_frameNumber, a_outputIndex,
                       cpOutputFrameRef, cpPreviewFrameRef);
        m_framesCache.push_back(newFrame);
        slotProcessPlayQueue();
    } else {
        setCurrentFrame(cpOutputFrameRef, cpPreviewFrameRef);
        m_frameShown = a_frameNumber;

        if (m_frameShown == m_frameExpected) {
            m_ui.frameStatusLabel->setPixmap(m_readyPixmap);
        }
    }
}

// END OF void PreviewDialog::slotReceiveFrame(int a_frameNumber,
//		int a_outputIndex, const VSFrameRef * a_cpOutputFrameRef,
//		const VSFrameRef * a_cpPreviewFrameRef)
//==============================================================================

void PreviewDialog::slotFrameRequestDiscarded(int a_frameNumber,
        int a_outputIndex, const QString &a_reason)
{
    (void)a_outputIndex;
    (void)a_reason;

    if (m_playing) {
        slotPlay(false);
    } else {
        if (a_frameNumber != m_frameExpected) {
            return;
        }

        if (m_frameShown == -1) {
            if (m_frameExpected == 0) {
                // Nowhere to roll back
                m_ui.frameNumberSlider->setFrame(0);
                m_ui.frameNumberSpinBox->setValue(0);
                m_ui.frameStatusLabel->setPixmap(m_errorPixmap);
            } else {
                slotShowFrame(0);
            }

            return;
        }

        m_frameExpected = m_frameShown;
        m_ui.frameNumberSlider->setFrame(m_frameShown);
        m_ui.frameNumberSpinBox->setValue(m_frameShown);
        m_ui.frameStatusLabel->setPixmap(m_readyPixmap);
    }
}

// END OF void PreviewDialog::slotFrameRequestDiscarded(int a_frameNumber,
//		int a_outputIndex, const QString & a_reason)
//==============================================================================

void PreviewDialog::slotShowFrame(int a_frameNumber)
{
    if ((m_frameShown == a_frameNumber) && (!m_framePixmap.isNull())) {
        return;
    }

    if (m_playing) {
        return;
    }

    static bool requestingFrame = false;

    if (requestingFrame) {
        return;
    }

    requestingFrame = true;

    m_ui.frameNumberSpinBox->setValue(a_frameNumber);
    m_ui.frameNumberSlider->setFrame(a_frameNumber);

    bool requested = requestShowFrame(a_frameNumber);

    if (requested) {
        m_frameExpected = a_frameNumber;
        m_ui.frameStatusLabel->setPixmap(m_busyPixmap);
    } else {
        m_ui.frameNumberSpinBox->setValue(m_frameExpected);
        m_ui.frameNumberSlider->setFrame(m_frameExpected);
    }

    requestingFrame = false;
}
// END OF void PreviewDialog::slotShowFrame(int a_frameNumber)
//==============================================================================

void PreviewDialog::slotSaveSnapshot()
{
    if ((m_frameShown < 0) || m_framePixmap.isNull()) {
        return;
    }

    QHash<QString, QString> extensionToFilterMap = {
        {"png", tr("PNG image (*.png)")},
    };

    QString fileExtension = m_pSettingsManager->getLastSnapshotExtension();

    QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
    bool webpSupported = (supportedFormats.indexOf("webp") > -1);

    if (webpSupported) {
        extensionToFilterMap["webp"] = tr("WebP image (*.webp)");
    }

    QString snapshotFilePath = scriptName();

    if (snapshotFilePath.isEmpty()) {
        snapshotFilePath =
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        snapshotFilePath += QString("/%1.").arg(m_frameShown);
    } else {
        snapshotFilePath += QString(" - %1.").arg(m_frameShown);
    }

    snapshotFilePath += fileExtension;

    QStringList saveFormatsList = extensionToFilterMap.values();

    QString selectedFilter = extensionToFilterMap[fileExtension];

    snapshotFilePath = QFileDialog::getSaveFileName(this,
                       tr("Save frame as image"), snapshotFilePath,
                       saveFormatsList.join(";;"), &selectedFilter);

    QFileInfo fileInfo(snapshotFilePath);
    QString suffix = fileInfo.suffix().toLower();

    QByteArray format("png");
    int quality = -1;

    if ((suffix == "webp") && webpSupported) {
        format = "webp";
        quality = 100;
    }

    if (!snapshotFilePath.isEmpty()) {
        bool success = m_framePixmap.save(snapshotFilePath, format, quality);

        if (success) {
            m_pSettingsManager->setLastSnapshotExtension(suffix);
        } else {
            QMessageBox::critical(this, tr("Image save error"),
                                  tr("Error while saving image ") + snapshotFilePath);
        }
    }
}

// END OF void PreviewDialog::slotSaveSnapshot()
//==============================================================================

void PreviewDialog::slotToggleZoomPanelVisible(bool a_zoomPanelVisible)
{
    m_ui.zoomPanel->setVisible(a_zoomPanelVisible);
    m_pSettingsManager->setZoomPanelVisible(a_zoomPanelVisible);
}

// END OF void PreviewDialog::slotToggleZoomPanelVisible(
//		bool a_zoomPanelVisible)
//==============================================================================

void PreviewDialog::slotZoomModeChanged()
{
    static bool changingZoomMode = false;

    if (changingZoomMode) {
        return;
    }

    changingZoomMode = true;

    ZoomMode zoomMode = (ZoomMode)m_ui.zoomModeComboBox->currentData().toInt();

    QObject *pSender = sender();

    if (pSender == m_ui.zoomModeComboBox) {

        for (QAction *pAction : m_pActionGroupZoomModes->actions()) {
            ZoomMode actionZoomMode =
                m_actionIDToZoomModeMap[pAction->data().toString()];

            if (actionZoomMode == zoomMode) {
                pAction->setChecked(true);
                break;
            }
        }
    } else {
        // If signal wasn't sent by combo box - presume it was sent by action.
        QAction *pSenderAction = qobject_cast<QAction *>(pSender);
        zoomMode = m_actionIDToZoomModeMap[pSenderAction->data().toString()];
        int zoomModeIndex = m_ui.zoomModeComboBox->findData((int)zoomMode);
        m_ui.zoomModeComboBox->setCurrentIndex(zoomModeIndex);
    }

    setPreviewPixmap();
    bool fixedRatio(zoomMode == ZoomMode::FixedRatio);
    m_ui.zoomRatioSpinBox->setEnabled(fixedRatio);
    bool noZoom = (zoomMode == ZoomMode::NoZoom);
    m_ui.scaleModeComboBox->setEnabled(!noZoom);
    m_pMenuZoomScaleModes->setEnabled(!noZoom);
    m_pSettingsManager->setZoomMode(zoomMode);

    changingZoomMode = false;
}

// END OF void PreviewDialog::slotZoomModeChanged()
//==============================================================================

void PreviewDialog::slotZoomRatioChanged(double a_zoomRatio)
{
    setPreviewPixmap();
    m_pSettingsManager->setZoomRatio(a_zoomRatio);
}

// END OF void PreviewDialog::slotZoomRatioChanged(double a_zoomRatio)
//==============================================================================

void PreviewDialog::slotScaleModeChanged()
{
    static bool changingScaleMode = false;

    if (changingScaleMode) {
        return;
    }

    changingScaleMode = true;

    Qt::TransformationMode scaleMode = (Qt::TransformationMode)
                                       m_ui.scaleModeComboBox->currentData().toInt();

    QObject *pSender = sender();

    if (pSender == m_ui.scaleModeComboBox) {
        for (QAction *pAction : m_pActionGroupZoomScaleModes->actions()) {
            Qt::TransformationMode actionScaleMode =
                m_actionIDToZoomScaleModeMap[pAction->data().toString()];

            if (actionScaleMode == scaleMode) {
                pAction->setChecked(true);
                break;
            }
        }
    } else {
        // If signal wasn't sent by combo box - presume it was sent by action.
        QAction *pSenderAction = qobject_cast<QAction *>(pSender);
        scaleMode =
            m_actionIDToZoomScaleModeMap[pSenderAction->data().toString()];
        int scaleModeIndex = m_ui.scaleModeComboBox->findData((int)scaleMode);
        m_ui.scaleModeComboBox->setCurrentIndex(scaleModeIndex);
    }

    setPreviewPixmap();
    m_pSettingsManager->setScaleMode(scaleMode);

    changingScaleMode = false;
}

// END OF void PreviewDialog::slotScaleModeChanged()
//==============================================================================

void PreviewDialog::slotToggleCropPanelVisible(bool a_cropPanelVisible)
{
    m_ui.cropPanel->setVisible(a_cropPanelVisible);
    setPreviewPixmap();
}

// END OF void PreviewDialog::slotToggleCropPanelVisible(
//		bool a_cropPanelVisible)
//==============================================================================

void PreviewDialog::slotCropModeChanged()
{
    CropMode cropMode = (CropMode)m_ui.cropModeComboBox->currentData().toInt();

    if (cropMode == CropMode::Absolute) {
        m_ui.cropWidthSpinBox->setEnabled(true);
        m_ui.cropHeightSpinBox->setEnabled(true);
        m_ui.cropRightSpinBox->setEnabled(false);
        m_ui.cropBottomSpinBox->setEnabled(false);
    } else {
        m_ui.cropWidthSpinBox->setEnabled(false);
        m_ui.cropHeightSpinBox->setEnabled(false);
        m_ui.cropRightSpinBox->setEnabled(true);
        m_ui.cropBottomSpinBox->setEnabled(true);
    }

    m_pSettingsManager->setCropMode(cropMode);
}

// END OF void PreviewDialog::slotCropModeChanged()
//==============================================================================

void PreviewDialog::slotCropLeftValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    int remainder = m_cpVideoInfo->width - a_value;
    m_ui.cropWidthSpinBox->setMaximum(remainder);
    m_ui.cropRightSpinBox->setMaximum(remainder - 1);

    CropMode cropMode = (CropMode)m_ui.cropModeComboBox->currentData().toInt();

    if (cropMode == CropMode::Absolute) {
        if (m_ui.cropWidthSpinBox->value() > remainder) {
            m_ui.cropWidthSpinBox->setValue(remainder);
        }

        m_ui.cropRightSpinBox->setValue(remainder -
                                        m_ui.cropWidthSpinBox->value());
    } else {
        if (m_ui.cropRightSpinBox->value() > remainder - 1) {
            m_ui.cropRightSpinBox->setValue(remainder - 1);
        }

        m_ui.cropWidthSpinBox->setValue(remainder -
                                        m_ui.cropRightSpinBox->value());
    }

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollLeft();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropLeftValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropTopValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    int remainder = m_cpVideoInfo->height - a_value;
    m_ui.cropHeightSpinBox->setMaximum(remainder);
    m_ui.cropBottomSpinBox->setMaximum(remainder - 1);

    CropMode cropMode = (CropMode)m_ui.cropModeComboBox->currentData().toInt();

    if (cropMode == CropMode::Absolute) {
        if (m_ui.cropHeightSpinBox->value() > remainder) {
            m_ui.cropHeightSpinBox->setValue(remainder);
        }

        m_ui.cropBottomSpinBox->setValue(remainder -
                                         m_ui.cropHeightSpinBox->value());
    } else {
        if (m_ui.cropBottomSpinBox->value() > remainder - 1) {
            m_ui.cropBottomSpinBox->setValue(remainder - 1);
        }

        m_ui.cropHeightSpinBox->setValue(remainder -
                                         m_ui.cropBottomSpinBox->value());
    }

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollTop();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropTopValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropWidthValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    m_ui.cropRightSpinBox->setValue(m_cpVideoInfo->width -
                                    m_ui.cropLeftSpinBox->value() - a_value);

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollRight();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropWidthValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropHeightValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    m_ui.cropBottomSpinBox->setValue(m_cpVideoInfo->height -
                                     m_ui.cropTopSpinBox->value() - a_value);

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollBottom();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropHeightValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropRightValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    m_ui.cropWidthSpinBox->setValue(m_cpVideoInfo->width -
                                    m_ui.cropLeftSpinBox->value() - a_value);

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollRight();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropRightValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropBottomValueChanged(int a_value)
{
    BEGIN_CROP_VALUES_CHANGE

    m_ui.cropHeightSpinBox->setValue(m_cpVideoInfo->height -
                                     m_ui.cropTopSpinBox->value() - a_value);

    recalculateCropMods();

    setPreviewPixmap();

    m_ui.previewArea->slotScrollBottom();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::slotCropBottomValueChanged(int a_value)
//==============================================================================

void PreviewDialog::slotCropZoomRatioValueChanged(int a_cropZoomRatio)
{
    m_pSettingsManager->setCropZoomRatio(a_cropZoomRatio);
    setPreviewPixmap();
}

// END OF void PreviewDialog::slotCropZoomRatioValueChanged(int a_cropZoomRatio)
//==============================================================================

void PreviewDialog::slotPasteCropSnippetIntoScript()
{
    if (!m_ui.cropPanel->isVisible()) {
        return;
    }

    CropMode cropMode = (CropMode)m_ui.cropModeComboBox->currentData().toInt();
    QString cropString;

    if (cropMode == CropMode::Absolute) {
        cropString = QString("***CLIP*** = core.std.CropAbs"
                             "(***CLIP***, x=%1, y=%2, width=%3, height=%4)")
                     .arg(m_ui.cropLeftSpinBox->value())
                     .arg(m_ui.cropTopSpinBox->value())
                     .arg(m_ui.cropWidthSpinBox->value())
                     .arg(m_ui.cropHeightSpinBox->value());
    } else {
        cropString = QString("***CLIP*** = core.std.CropRel"
                             "(***CLIP***, left=%1, top=%2, right=%3, bottom=%4)")
                     .arg(m_ui.cropLeftSpinBox->value())
                     .arg(m_ui.cropTopSpinBox->value())
                     .arg(m_ui.cropRightSpinBox->value())
                     .arg(m_ui.cropBottomSpinBox->value());
    }

    emit signalPasteIntoScriptAtNewLine(cropString);
}

// END OF void PreviewDialog::slotPasteCropSnippetIntoScript()
//==============================================================================

void PreviewDialog::slotCallAdvancedSettingsDialog()
{
    m_pAdvancedSettingsDialog->slotCall();
}

// END OF void PreviewDialog::slotCallAdvancedSettingsDialog()
//==============================================================================

void PreviewDialog::slotToggleTimeLinePanelVisible(bool a_timeLinePanelVisible)
{
    m_ui.timeLinePanel->setVisible(a_timeLinePanelVisible);
    m_pSettingsManager->setTimeLinePanelVisible(a_timeLinePanelVisible);
}

// END OF void PreviewDialog::slotToggleTimeLinePanelVisible(
//		bool a_timeLinePanelVisible)
//==============================================================================

void PreviewDialog::slotTimeLineModeChanged()
{
    static bool changingTimeLineMode = false;

    if (changingTimeLineMode) {
        return;
    }

    changingTimeLineMode = true;

    TimeLineSlider::DisplayMode timeLineMode = (TimeLineSlider::DisplayMode)
            m_ui.timeLineModeComboBox->currentData().toInt();

    QObject *pSender = sender();

    if (pSender == m_ui.timeLineModeComboBox) {
        for (QAction *pAction : m_pActionGroupTimeLineModes->actions()) {
            TimeLineSlider::DisplayMode actionTimeLineMode =
                m_actionIDToTimeLineModeMap[pAction->data().toString()];

            if (actionTimeLineMode == timeLineMode) {
                pAction->setChecked(true);
                break;
            }
        }
    } else {
        // If signal wasn't sent by combo box - presume it was sent by action.
        QAction *pSenderAction = qobject_cast<QAction *>(pSender);
        timeLineMode =
            m_actionIDToTimeLineModeMap[pSenderAction->data().toString()];
        int timeLineModeIndex = m_ui.timeLineModeComboBox->findData(
                                    (int)timeLineMode);
        m_ui.timeLineModeComboBox->setCurrentIndex(timeLineModeIndex);
    }

    m_ui.frameNumberSlider->setDisplayMode(timeLineMode);
    m_pSettingsManager->setTimeLineMode(timeLineMode);

    changingTimeLineMode = false;
}

// END OF void PreviewDialog::slotTimeLineModeChanged()
//==============================================================================

void PreviewDialog::slotTimeStepChanged(const QTime &a_time)
{
    m_pSettingsManager->setTimeStep(vsedit::qtimeToSeconds(a_time));
}

// END OF void PreviewDialog::slotTimeStepChanged(const QTime & a_time)
//==============================================================================

void PreviewDialog::slotTimeStepForward()
{
    double step = vsedit::qtimeToSeconds(m_ui.timeStepEdit->time());
    m_ui.frameNumberSlider->slotStepBySeconds(step);
}

// END OF void PreviewDialog::slotTimeStepForward()
//==============================================================================

void PreviewDialog::slotTimeStepBack()
{
    double step = vsedit::qtimeToSeconds(m_ui.timeStepEdit->time());
    m_ui.frameNumberSlider->slotStepBySeconds(-step);
}

// END OF void PreviewDialog::slotTimeStepBack()
//==============================================================================

void PreviewDialog::slotSettingsChanged()
{
    QKeySequence hotkey;

    for (QAction *pAction : m_settableActionsList) {
        hotkey = m_pSettingsManager->getHotkey(pAction->data().toString());
        pAction->setShortcut(hotkey);
    }

    m_alwaysKeepCurrentFrame = m_pSettingsManager->getAlwaysKeepCurrentFrame();

    m_ui.frameNumberSlider->setUpdatesEnabled(false);

    QFont sliderLabelsFont =
        m_pSettingsManager->getTextFormat(TEXT_FORMAT_ID_TIMELINE).font();
    m_ui.frameNumberSlider->setLabelsFont(sliderLabelsFont);

    QColor bookmarksColor =
        m_pSettingsManager->getColor(COLOR_ID_TIMELINE_BOOKMARKS);
    m_ui.frameNumberSlider->setColor(TimeLineSlider::Bookmark, bookmarksColor);

    m_ui.frameNumberSlider->setUpdatesEnabled(true);
}

// END OF void PreviewDialog::slotSettingsChanged()
//==============================================================================

void PreviewDialog::slotPreviewAreaSizeChanged()
{
    ZoomMode zoomMode = (ZoomMode)m_ui.zoomModeComboBox->currentData().toInt();

    if (zoomMode == ZoomMode::FitToFrame) {
        setPreviewPixmap();
    }
}

// END OF void PreviewDialog::slotPreviewAreaSizeChanged()
//==============================================================================

void PreviewDialog::slotPreviewAreaCtrlWheel(QPoint a_angleDelta)
{
    ZoomMode zoomMode = (ZoomMode)m_ui.zoomModeComboBox->currentData().toInt();
    int deltaY = a_angleDelta.y();

    if (m_ui.cropCheckButton->isChecked()) {
        if (deltaY > 0) {
            m_ui.cropZoomRatioSpinBox->stepBy(1);
        } else if (deltaY < 0) {
            m_ui.cropZoomRatioSpinBox->stepBy(-1);
        }
    } else if (zoomMode == ZoomMode::FixedRatio) {
        if (deltaY > 0) {
            m_ui.zoomRatioSpinBox->stepBy(1);
        } else if (deltaY < 0) {
            m_ui.zoomRatioSpinBox->stepBy(-1);
        }
    }
}

// END OF void PreviewDialog::slotPreviewAreaCtrlWheel(QPoint a_angleDelta)
//==============================================================================

void PreviewDialog::slotPreviewAreaMouseMiddleButtonReleased()
{
    if (m_ui.cropCheckButton->isChecked()) {
        return;
    }

    int zoomModeIndex = m_ui.zoomModeComboBox->currentIndex();
    zoomModeIndex++;

    if (zoomModeIndex >= m_ui.zoomModeComboBox->count()) {
        zoomModeIndex = 0;
    }

    m_ui.zoomModeComboBox->setCurrentIndex(zoomModeIndex);
}

// END OF void PreviewDialog::slotPreviewAreaMouseMiddleButtonReleased()
//==============================================================================

void PreviewDialog::slotPreviewAreaMouseRightButtonReleased()
{
    m_pPreviewContextMenu->popup(QCursor::pos());
}

// END OF void PreviewDialog::slotPreviewAreaMouseRightButtonReleased()
//==============================================================================

void PreviewDialog::slotPreviewAreaMouseOverPoint(float a_normX, float a_normY)
{
    if (!m_cpFrameRef) {
        return;
    }

    if (!m_pStatusBarWidget->colorPickerVisible()) {
        return;
    }

    double value1 = 0.0;
    double value2 = 0.0;
    double value3 = 0.0;

    size_t frameX = 0;
    size_t frameY = 0;

    frameX = (size_t)((float)m_framePixmap.width() * a_normX);
    frameY = (size_t)((float)m_framePixmap.height() * a_normY);

    int width = m_cpVSAPI->getFrameWidth(m_cpFrameRef, 0);
    int height = m_cpVSAPI->getFrameHeight(m_cpFrameRef, 0);
    const VSFormat *cpFormat = m_cpVSAPI->getFrameFormat(m_cpFrameRef);

    if ((frameX >= (size_t)width) || (frameY >= (size_t)height)) {
        return;
    }

    if (cpFormat->id == pfCompatBGR32) {
        const uint8_t *cpData = m_cpVSAPI->getReadPtr(m_cpFrameRef, 0);
        int stride = m_cpVSAPI->getStride(m_cpFrameRef, 0);
        const uint32_t *cpLine = (const uint32_t *)(cpData + frameY * stride);
        uint32_t packedValue = cpLine[frameX];
        value3 = (double)(packedValue & 0xFF);
        value2 = (double)((packedValue >> 8) & 0xFF);
        value1 = (double)((packedValue >> 16) & 0xFF);
    } else if (cpFormat->id == pfCompatYUY2) {
        size_t x = frameX >> 1;
        size_t rem = frameX & 0x1;
        const uint8_t *cpData = m_cpVSAPI->getReadPtr(m_cpFrameRef, 0);
        int stride = m_cpVSAPI->getStride(m_cpFrameRef, 0);
        const uint32_t *cpLine = (const uint32_t *)(cpData + frameY * stride);
        uint32_t packedValue = cpLine[x];

        if (rem == 0) {
            value1 = (double)(packedValue & 0xFF);
        } else {
            value1 = (double)((packedValue >> 16) & 0xFF);
        }

        value2 = (double)((packedValue >> 8) & 0xFF);
        value3 = (double)((packedValue >> 24) & 0xFF);
    } else {
        value1 = valueAtPoint(frameX, frameY, 0);

        if (cpFormat->numPlanes > 1) {
            value2 = valueAtPoint(frameX, frameY, 1);
        }

        if (cpFormat->numPlanes > 2) {
            value3 = valueAtPoint(frameX, frameY, 2);
        }
    }

    QString l1("1");
    QString l2("2");
    QString l3("3");

    int colorFamily = m_cpVideoInfo->format->colorFamily;
    int formatID = m_cpVideoInfo->format->id;

    if ((colorFamily == cmYUV) || (formatID == pfCompatYUY2)) {
        l1 = "Y";
        l2 = "U";
        l3 = "V";
    } else if ((colorFamily == cmRGB) || (formatID == pfCompatBGR32)) {
        l1 = "R";
        l2 = "G";
        l3 = "B";
    } else if (colorFamily == cmYCoCg) {
        l1 = "Y";
        l2 = "Co";
        l3 = "Cg";
    }

    QString colorString = QString("%1:%2|%3:%4|%5:%6")
                          .arg(l1).arg(value1).arg(l2).arg(value2).arg(l3).arg(value3);

    if (colorFamily == cmGray) {
        colorString = QString("G:%1").arg(value1);
    }

    m_pStatusBarWidget->setColorPickerString(colorString);
}

// END OF void PreviewDialog::slotPreviewAreaMouseOverPoint(float a_normX,
//		float a_normY)
//==============================================================================

void PreviewDialog::slotFrameToClipboard()
{
    if (m_framePixmap.isNull()) {
        return;
    }

    QClipboard *pClipboard = QApplication::clipboard();
    pClipboard->setPixmap(QPixmap::fromImage(m_framePixmap));
}

// END OF void PreviewDialog::slotFrameToClipboard()
//==============================================================================

void PreviewDialog::slotAdvancedSettingsChanged()
{
    m_pVapourSynthScriptProcessor->slotResetSettings();

    if (!m_playing) {
        requestShowFrame(m_frameExpected);
    }
}

// END OF void PreviewDialog::slotAdvancedSettingsChanged()
//==============================================================================

void PreviewDialog::slotToggleColorPicker(bool a_colorPickerVisible)
{
    m_pStatusBarWidget->setColorPickerVisible(a_colorPickerVisible);
    m_pSettingsManager->setColorPickerVisible(a_colorPickerVisible);
}

// END OF void PreviewDialog::slotToggleColorPicker(bool a_colorPickerVisible)
//==============================================================================

void PreviewDialog::slotSetPlayFPSLimit()
{
    double limit = m_ui.playFpsLimitSpinBox->value();

    PlayFPSLimitMode mode =
        (PlayFPSLimitMode)m_ui.playFpsLimitModeComboBox->currentData().toInt();

    if (mode == PlayFPSLimitMode::NoLimit) {
        m_secondsBetweenFrames = 0.0;
    } else if (mode == PlayFPSLimitMode::Custom) {
        m_secondsBetweenFrames = 1.0 / limit;
    } else if (mode == PlayFPSLimitMode::FromVideo) {
        if (!m_cpVideoInfo) {
            m_secondsBetweenFrames = 0.0;
        } else if (m_cpVideoInfo->fpsNum == 0ll) {
            m_secondsBetweenFrames = 0.0;
        } else {
            m_secondsBetweenFrames =
                (double)m_cpVideoInfo->fpsDen / (double)m_cpVideoInfo->fpsNum;
        }
    } else {
        Q_ASSERT(false);
    }

    m_pSettingsManager->setPlayFPSLimitMode(mode);
    m_pSettingsManager->setPlayFPSLimit(limit);
}

// END OF void PreviewDialog::void slotSetPlayFPSLimit()
//==============================================================================

void PreviewDialog::slotPlay(bool a_play)
{
    if (m_playing == a_play) {
        return;
    }

    m_playing = a_play;
    m_pActionPlay->setChecked(m_playing);

    if (m_playing) {
        m_pActionPlay->setIcon(m_iconPause);
        m_lastFrameRequestedForPlay = m_frameShown;
        slotProcessPlayQueue();
    } else {
        clearFramesCache();
        m_pVapourSynthScriptProcessor->flushFrameTicketsQueue();
        m_pActionPlay->setIcon(m_iconPlay);
    }
}

// END OF void PreviewDialog::slotPlay(bool a_play)
//==============================================================================

void PreviewDialog::slotProcessPlayQueue()
{
    if (!m_playing) {
        return;
    }

    if (m_processingPlayQueue) {
        return;
    }

    m_processingPlayQueue = true;

    int nextFrame = (m_frameShown + 1) % m_cpVideoInfo->numFrames;
    Frame referenceFrame(nextFrame, 0, nullptr);

    while (!m_framesCache.empty()) {
        QList<Frame>::iterator it =
            std::find(m_framesCache.begin(), m_framesCache.end(),
                      referenceFrame);

        if (it == m_framesCache.end()) {
            break;
        }

        hr_time_point now = hr_clock::now();
        double passed = duration_to_double(now - m_lastFrameShowTime);
        double secondsToNextFrame = m_secondsBetweenFrames - passed;

        if (secondsToNextFrame > 0) {
            int millisecondsToNextFrame = std::ceil(secondsToNextFrame * 1000);
            m_pPlayTimer->start(millisecondsToNextFrame);
            break;
        }

        setCurrentFrame(it->cpOutputFrameRef, it->cpPreviewFrameRef);
        m_lastFrameShowTime = hr_clock::now();

        m_frameShown = nextFrame;
        m_frameExpected = m_frameShown;
        m_ui.frameNumberSpinBox->setValue(m_frameExpected);
        m_ui.frameNumberSlider->setFrame(m_frameExpected);
        m_framesCache.erase(it);
        nextFrame = (m_frameShown + 1) % m_cpVideoInfo->numFrames;
        referenceFrame.number = nextFrame;
    }

    nextFrame = (m_lastFrameRequestedForPlay + 1) %
                m_cpVideoInfo->numFrames;

    while (((m_framesInQueue + m_framesInProcess) < m_maxThreads) &&
            (m_framesCache.size() <= m_cachedFramesLimit)) {
        m_pVapourSynthScriptProcessor->requestFrameAsync(nextFrame, 0, true);
        m_lastFrameRequestedForPlay = nextFrame;
        nextFrame = (nextFrame + 1) % m_cpVideoInfo->numFrames;
    }

    m_processingPlayQueue = false;
}

// END OF void PreviewDialog::slotProcessPlayQueue()
//==============================================================================

void PreviewDialog::slotLoadChapters()
{
    if (m_playing) {
        return;
    }

    const QString lastUsedPath = m_pSettingsManager->getLastUsedPath();
    const QString filePath = QFileDialog::getOpenFileName(this,
                             tr("Load chapters"), lastUsedPath,
                             tr("Chapters file (*.txt;*.xml);;All files (*)"));
    QFile chaptersFile(filePath);

    if (!chaptersFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const VSVideoInfo *cpVideoInfo =
        m_pVapourSynthScriptProcessor->videoInfo();

    if (cpVideoInfo->fpsDen == 0) {
        QString infoString =
            tr("Warning: Load chapters requires clip having constant frame rate. Skipped");
        emit signalWriteLogMessage(mtWarning, infoString);
        return;
    }
    const double fps = (double)cpVideoInfo->fpsNum /
                       (double)cpVideoInfo->fpsDen;

    static const QRegExp regExp(R"((\d{2}):(\d{2}):(\d{2})[\.:](\d{3})?)");

    while (!chaptersFile.atEnd()) {
        const QByteArray line = chaptersFile.readLine();

        if (regExp.indexIn(line) < 0) {
            continue;
        }

        const QStringList timecodes = regExp.capturedTexts();

        const double timecode = timecodes.at(1).toDouble() * 3600.0 +
                                timecodes.at(2).toDouble() * 60.0 + timecodes.at(3).toDouble() +
                                timecodes.at(4).toDouble() / 1000;
        const int frameIndex  = round(timecode * fps);
        m_ui.frameNumberSlider->addBookmark(frameIndex);
    }

    saveTimelineBookmarks();
}

// END OF void PreviewDialog::slotLoadBookmarks()
//==============================================================================

void PreviewDialog::slotClearBookmarks()
{
    if (m_playing) {
        return;
    }

    QMessageBox::StandardButton result = QMessageBox::question(this,
                                         tr("Clear bookmards"), tr("Do you really want to clear "
                                                 "timeline bookmarks?"),
                                         QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
                                         QMessageBox::No);

    if (result == QMessageBox::No) {
        return;
    }

    m_ui.frameNumberSlider->clearBookmarks();
    saveTimelineBookmarks();
}

// END OF void PreviewDialog::slotClearBookmarks()
//==============================================================================

void PreviewDialog::slotBookmarkCurrentFrame()
{
    if (m_playing) {
        return;
    }

    m_ui.frameNumberSlider->slotBookmarkCurrentFrame();
    saveTimelineBookmarks();
}

// END OF void PreviewDialog::slotBookmarkCurrentFrame()
//==============================================================================

void PreviewDialog::slotUnbookmarkCurrentFrame()
{
    if (m_playing) {
        return;
    }

    m_ui.frameNumberSlider->slotUnbookmarkCurrentFrame();
    saveTimelineBookmarks();
}

// END OF void PreviewDialog::slotUnbookmarkCurrentFrame()
//==============================================================================

void PreviewDialog::slotGoToPreviousBookmark()
{
    if (m_playing) {
        return;
    }

    m_ui.frameNumberSlider->slotGoToPreviousBookmark();
}

// END OF void PreviewDialog::slotGoToPreviousBookmark()
//==============================================================================

void PreviewDialog::slotGoToNextBookmark()
{
    if (m_playing) {
        return;
    }

    m_ui.frameNumberSlider->slotGoToNextBookmark();
}

// END OF void PreviewDialog::slotGoToNextBookmark()
//==============================================================================

void PreviewDialog::slotPasteShownFrameNumberIntoScript()
{
    emit signalPasteIntoScriptAtCursor(QString::number(m_frameShown));
}

// END OF void PreviewDialog::slotPasteShownFrameNumberIntoScript()
//==============================================================================

void PreviewDialog::slotSaveGeometry()
{
    m_pGeometrySaveTimer->stop();
    m_pSettingsManager->setPreviewDialogGeometry(m_windowGeometry);
}

// END OF void PreviewDialog::slotSaveGeometry()
//==============================================================================

void PreviewDialog::createActionsAndMenus()
{
    struct ActionToCreate {
        QAction **ppAction;
        const char *id;
        bool checkable;
        const char *slotToConnect;
    };

    ActionToCreate actionsToCreate[] = {
        {
            &m_pActionFrameToClipboard, ACTION_ID_FRAME_TO_CLIPBOARD,
            false, SLOT(slotFrameToClipboard())
        },
        {
            &m_pActionSaveSnapshot, ACTION_ID_SAVE_SNAPSHOT,
            false, SLOT(slotSaveSnapshot())
        },
        {
            &m_pActionToggleZoomPanel, ACTION_ID_TOGGLE_ZOOM_PANEL,
            true, SLOT(slotToggleZoomPanelVisible(bool))
        },
        {
            &m_pActionSetZoomModeNoZoom, ACTION_ID_SET_ZOOM_MODE_NO_ZOOM,
            true, SLOT(slotZoomModeChanged())
        },
        {
            &m_pActionSetZoomModeFixedRatio, ACTION_ID_SET_ZOOM_MODE_FIXED_RATIO,
            true, SLOT(slotZoomModeChanged())
        },
        {
            &m_pActionSetZoomModeFitToFrame, ACTION_ID_SET_ZOOM_MODE_FIT_TO_FRAME,
            true, SLOT(slotZoomModeChanged())
        },
        {
            &m_pActionSetZoomScaleModeNearest,
            ACTION_ID_SET_ZOOM_SCALE_MODE_NEAREST,
            true, SLOT(slotScaleModeChanged())
        },
        {
            &m_pActionSetZoomScaleModeBilinear,
            ACTION_ID_SET_ZOOM_SCALE_MODE_BILINEAR,
            true, SLOT(slotScaleModeChanged())
        },
        {
            &m_pActionToggleCropPanel, ACTION_ID_TOGGLE_CROP_PANEL,
            true, SLOT(slotToggleCropPanelVisible(bool))
        },
        {
            &m_pActionToggleTimeLinePanel, ACTION_ID_TOGGLE_TIMELINE_PANEL,
            true, SLOT(slotToggleTimeLinePanelVisible(bool))
        },
        {
            &m_pActionSetTimeLineModeTime, ACTION_ID_SET_TIMELINE_MODE_TIME,
            true, SLOT(slotTimeLineModeChanged())
        },
        {
            &m_pActionSetTimeLineModeFrames, ACTION_ID_SET_TIMELINE_MODE_FRAMES,
            true, SLOT(slotTimeLineModeChanged())
        },
        {
            &m_pActionTimeStepForward, ACTION_ID_TIME_STEP_FORWARD,
            false, SLOT(slotTimeStepForward())
        },
        {
            &m_pActionTimeStepBack, ACTION_ID_TIME_STEP_BACK,
            false, SLOT(slotTimeStepBack())
        },
        {
            &m_pActionPasteCropSnippetIntoScript,
            ACTION_ID_PASTE_CROP_SNIPPET_INTO_SCRIPT,
            false, SLOT(slotPasteCropSnippetIntoScript())
        },
        {
            &m_pActionAdvancedSettingsDialog, ACTION_ID_ADVANCED_PREVIEW_SETTINGS,
            false, SLOT(slotCallAdvancedSettingsDialog())
        },
        {
            &m_pActionToggleColorPicker, ACTION_ID_TOGGLE_COLOR_PICKER,
            true, SLOT(slotToggleColorPicker(bool))
        },
        {
            &m_pActionPlay, ACTION_ID_PLAY,
            true, SLOT(slotPlay(bool))
        },
        {
            &m_pActionLoadChapters, ACTION_ID_TIMELINE_LOAD_CHAPTERS,
            false, SLOT(slotLoadChapters())
        },
        {
            &m_pActionClearBookmarks, ACTION_ID_TIMELINE_CLEAR_BOOKMARKS,
            false, SLOT(slotClearBookmarks())
        },
        {
            &m_pActionBookmarkCurrentFrame,
            ACTION_ID_TIMELINE_BOOKMARK_CURRENT_FRAME,
            false, SLOT(slotBookmarkCurrentFrame())
        },
        {
            &m_pActionUnbookmarkCurrentFrame,
            ACTION_ID_TIMELINE_UNBOOKMARK_CURRENT_FRAME,
            false, SLOT(slotUnbookmarkCurrentFrame())
        },
        {
            &m_pActionGoToPreviousBookmark,
            ACTION_ID_TIMELINE_GO_TO_PREVIOUS_BOOKMARK,
            false, SLOT(slotGoToPreviousBookmark())
        },
        {
            &m_pActionGoToNextBookmark, ACTION_ID_TIMELINE_GO_TO_NEXT_BOOKMARK,
            false, SLOT(slotGoToNextBookmark())
        },
        {
            &m_pActionPasteShownFrameNumberIntoScript,
            ACTION_ID_PASTE_SHOWN_FRAME_NUMBER_INTO_SCRIPT,
            false, SLOT(slotPasteShownFrameNumberIntoScript())
        },
    };

    for (ActionToCreate &item : actionsToCreate) {
        QAction *pAction =
            m_pSettingsManager->createStandardAction(item.id, this);
        *item.ppAction = pAction;
        pAction->setCheckable(item.checkable);
        m_settableActionsList.push_back(pAction);
    }

//------------------------------------------------------------------------------

    m_pPreviewContextMenu = new QMenu(this);
    m_pPreviewContextMenu->addAction(m_pActionFrameToClipboard);
    m_pPreviewContextMenu->addAction(m_pActionSaveSnapshot);
    m_pActionToggleZoomPanel->setChecked(
        m_pSettingsManager->getZoomPanelVisible());
    m_pPreviewContextMenu->addAction(m_pActionToggleZoomPanel);

//------------------------------------------------------------------------------

    m_pMenuZoomModes = new QMenu(m_pPreviewContextMenu);
    m_pMenuZoomModes->setTitle(tr("Zoom mode"));
    m_pPreviewContextMenu->addMenu(m_pMenuZoomModes);

    m_pActionGroupZoomModes = new QActionGroup(this);

    ZoomMode zoomMode = m_pSettingsManager->getZoomMode();

    struct ZoomModeAction {
        QAction *pAction;
        ZoomMode zoomMode;
    };

    ZoomModeAction zoomModeActions[] = {
        {m_pActionSetZoomModeNoZoom, ZoomMode::NoZoom},
        {m_pActionSetZoomModeFixedRatio, ZoomMode::FixedRatio},
        {m_pActionSetZoomModeFitToFrame, ZoomMode::FitToFrame},
    };

    for (ZoomModeAction &action : zoomModeActions) {
        QString id = action.pAction->data().toString();
        action.pAction->setActionGroup(m_pActionGroupZoomModes);
        m_pMenuZoomModes->addAction(action.pAction);
        m_actionIDToZoomModeMap[id] = action.zoomMode;
        addAction(action.pAction);

        if (zoomMode == action.zoomMode) {
            action.pAction->setChecked(true);
        }
    }

//------------------------------------------------------------------------------

    m_pMenuZoomScaleModes = new QMenu(m_pPreviewContextMenu);
    m_pMenuZoomScaleModes->setTitle(tr("Zoom scale mode"));
    m_pPreviewContextMenu->addMenu(m_pMenuZoomScaleModes);

    m_pActionGroupZoomScaleModes = new QActionGroup(this);

    Qt::TransformationMode scaleMode = m_pSettingsManager->getScaleMode();

    struct ScaleModeAction {
        QAction *pAction;
        Qt::TransformationMode scaleMode;
    };

    ScaleModeAction scaleModeActions[] = {
        {m_pActionSetZoomScaleModeNearest, Qt::FastTransformation},
        {m_pActionSetZoomScaleModeBilinear, Qt::SmoothTransformation},
    };

    for (ScaleModeAction &action : scaleModeActions) {
        QString id = action.pAction->data().toString();
        action.pAction->setActionGroup(m_pActionGroupZoomScaleModes);
        m_pMenuZoomScaleModes->addAction(action.pAction);
        m_actionIDToZoomScaleModeMap[id] = action.scaleMode;
        addAction(action.pAction);

        if (scaleMode == action.scaleMode) {
            action.pAction->setChecked(true);
        }
    }

    bool noZoom = (zoomMode == ZoomMode::NoZoom);
    m_pMenuZoomScaleModes->setEnabled(!noZoom);

//------------------------------------------------------------------------------

    m_pPreviewContextMenu->addAction(m_pActionToggleCropPanel);
    m_pPreviewContextMenu->addAction(m_pActionToggleTimeLinePanel);
    m_pActionToggleTimeLinePanel->setChecked(
        m_pSettingsManager->getTimeLinePanelVisible());

//------------------------------------------------------------------------------

    m_pMenuTimeLineModes = new QMenu(m_pPreviewContextMenu);
    m_pMenuTimeLineModes->setTitle(tr("Timeline display mode"));
    m_pPreviewContextMenu->addMenu(m_pMenuTimeLineModes);

    m_pActionGroupTimeLineModes = new QActionGroup(this);

    TimeLineSlider::DisplayMode timeLineMode =
        m_pSettingsManager->getTimeLineMode();

    struct TimeLineModeAction {
        QAction *pAction;
        TimeLineSlider::DisplayMode timeLineMode;
    };

    TimeLineModeAction timeLineModeAction[] = {
        {m_pActionSetTimeLineModeTime, TimeLineSlider::DisplayMode::Time},
        {m_pActionSetTimeLineModeFrames, TimeLineSlider::DisplayMode::Frames},
    };

    for (TimeLineModeAction &action : timeLineModeAction) {
        QString id = action.pAction->data().toString();
        action.pAction->setActionGroup(m_pActionGroupTimeLineModes);
        m_pMenuTimeLineModes->addAction(action.pAction);
        m_actionIDToTimeLineModeMap[id] = action.timeLineMode;
        addAction(action.pAction);

        if (timeLineMode == action.timeLineMode) {
            action.pAction->setChecked(true);
        }
    }

//------------------------------------------------------------------------------

    addAction(m_pActionTimeStepForward);
    addAction(m_pActionTimeStepBack);

    m_pActionToggleColorPicker->setChecked(
        m_pSettingsManager->getColorPickerVisible());
    m_pPreviewContextMenu->addAction(m_pActionToggleColorPicker);

    m_pActionPlay->setChecked(false);
    addAction(m_pActionPlay);

    addAction(m_pActionLoadChapters);
    addAction(m_pActionClearBookmarks);
    addAction(m_pActionBookmarkCurrentFrame);
    addAction(m_pActionUnbookmarkCurrentFrame);
    addAction(m_pActionGoToPreviousBookmark);
    addAction(m_pActionGoToNextBookmark);

    addAction(m_pActionPasteShownFrameNumberIntoScript);
    m_pPreviewContextMenu->addAction(m_pActionPasteShownFrameNumberIntoScript);

//------------------------------------------------------------------------------

    for (ActionToCreate &item : actionsToCreate) {
        const char *signal =
            item.checkable ? SIGNAL(toggled(bool)) : SIGNAL(triggered());
        connect(*item.ppAction, signal, this, item.slotToConnect);
    }
}

// END OF void PreviewDialog::createActionsAndMenus()
//==============================================================================

void PreviewDialog::setUpZoomPanel()
{
    m_ui.zoomPanel->setVisible(m_pSettingsManager->getZoomPanelVisible());

    m_ui.zoomRatioSpinBox->setLocale(QLocale("C"));

    m_ui.zoomCheckButton->setDefaultAction(m_pActionToggleZoomPanel);

    m_ui.zoomModeComboBox->addItem(QIcon(":zoom_no_zoom.png"),
                                   tr("No zoom"), (int)ZoomMode::NoZoom);
    m_ui.zoomModeComboBox->addItem(QIcon(":zoom_fixed_ratio.png"),
                                   tr("Fixed ratio"), (int)ZoomMode::FixedRatio);
    m_ui.zoomModeComboBox->addItem(QIcon(":zoom_fit_to_frame.png"),
                                   tr("Fit to frame"), (int)ZoomMode::FitToFrame);

    ZoomMode zoomMode = m_pSettingsManager->getZoomMode();
    int comboIndex = m_ui.zoomModeComboBox->findData((int)zoomMode);

    if (comboIndex != -1) {
        m_ui.zoomModeComboBox->setCurrentIndex(comboIndex);
    }

    bool fixedRatio(zoomMode == ZoomMode::FixedRatio);
    m_ui.zoomRatioSpinBox->setEnabled(fixedRatio);

    double zoomRatio = m_pSettingsManager->getZoomRatio();
    m_ui.zoomRatioSpinBox->setValue(zoomRatio);

    m_ui.scaleModeComboBox->addItem(tr("Nearest"),
                                    (int)Qt::FastTransformation);
    m_ui.scaleModeComboBox->addItem(tr("Bilinear"),
                                    (int)Qt::SmoothTransformation);
    bool noZoom = (zoomMode == ZoomMode::NoZoom);
    m_ui.scaleModeComboBox->setEnabled(!noZoom);

    Qt::TransformationMode scaleMode = m_pSettingsManager->getScaleMode();
    comboIndex = m_ui.scaleModeComboBox->findData((int)scaleMode);

    if (comboIndex != -1) {
        m_ui.scaleModeComboBox->setCurrentIndex(comboIndex);
    }

    connect(m_ui.zoomModeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotZoomModeChanged()));
    connect(m_ui.zoomRatioSpinBox, SIGNAL(valueChanged(double)),
            this, SLOT(slotZoomRatioChanged(double)));
    connect(m_ui.scaleModeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotScaleModeChanged()));
}

// END OF void PreviewDialog::setUpZoomPanel()
//==============================================================================

void PreviewDialog::setUpTimeLinePanel()
{
    m_ui.timeLinePanel->setVisible(
        m_pSettingsManager->getTimeLinePanelVisible());

    m_ui.playButton->setDefaultAction(m_pActionPlay);
    m_ui.timeLineCheckButton->setDefaultAction(m_pActionToggleTimeLinePanel);
    m_ui.timeStepForwardButton->setDefaultAction(m_pActionTimeStepForward);
    m_ui.timeStepBackButton->setDefaultAction(m_pActionTimeStepBack);

    m_ui.playFpsLimitModeComboBox->addItem(tr("From video"),
                                           (int)PlayFPSLimitMode::FromVideo);
    m_ui.playFpsLimitModeComboBox->addItem(tr("No limit"),
                                           (int)PlayFPSLimitMode::NoLimit);
    m_ui.playFpsLimitModeComboBox->addItem(tr("Custom"),
                                           (int)PlayFPSLimitMode::Custom);

    PlayFPSLimitMode playFpsLimitMode =
        m_pSettingsManager->getPlayFPSLimitMode();
    int comboIndex = m_ui.playFpsLimitModeComboBox->findData(
                         (int)playFpsLimitMode);

    if (comboIndex != -1) {
        m_ui.playFpsLimitModeComboBox->setCurrentIndex(comboIndex);
    }

    m_ui.playFpsLimitSpinBox->setLocale(QLocale("C"));
    double customFPS = m_pSettingsManager->getPlayFPSLimit();
    m_ui.playFpsLimitSpinBox->setValue(customFPS);

    slotSetPlayFPSLimit();

    m_ui.loadChaptersButton->setDefaultAction(m_pActionLoadChapters);
    m_ui.clearBookmarksButton->setDefaultAction(m_pActionClearBookmarks);
    m_ui.bookmarkCurrentFrameButton->setDefaultAction(
        m_pActionBookmarkCurrentFrame);
    m_ui.unbookmarkCurrentFrameButton->setDefaultAction(
        m_pActionUnbookmarkCurrentFrame);
    m_ui.goToPreviousBookmarkButton->setDefaultAction(
        m_pActionGoToPreviousBookmark);
    m_ui.goToNextBookmarkButton->setDefaultAction(
        m_pActionGoToNextBookmark);

    double timeStep = m_pSettingsManager->getTimeStep();
    m_ui.timeStepEdit->setTime(vsedit::secondsToQTime(timeStep));

    m_ui.timeLineModeComboBox->addItem(QIcon(":timeline.png"), tr("Time"),
                                       (int)TimeLineSlider::DisplayMode::Time);
    m_ui.timeLineModeComboBox->addItem(QIcon(":timeline_frames.png"),
                                       tr("Frames"), (int)TimeLineSlider::DisplayMode::Frames);

    TimeLineSlider::DisplayMode timeLineMode =
        m_pSettingsManager->getTimeLineMode();
    comboIndex = m_ui.timeLineModeComboBox->findData((int)timeLineMode);

    if (comboIndex != -1) {
        m_ui.timeLineModeComboBox->setCurrentIndex(comboIndex);
    }

    connect(m_ui.timeStepEdit, SIGNAL(timeChanged(const QTime &)),
            this, SLOT(slotTimeStepChanged(const QTime &)));
    connect(m_ui.timeLineModeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotTimeLineModeChanged()));
    connect(m_ui.playFpsLimitModeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSetPlayFPSLimit()));
    connect(m_ui.playFpsLimitSpinBox, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetPlayFPSLimit()));
}

// END OF void PreviewDialog::setUpTimeLinePanel()
//==============================================================================

void PreviewDialog::setUpCropPanel()
{
    m_ui.cropCheckButton->setDefaultAction(m_pActionToggleCropPanel);

    m_ui.cropModeComboBox->addItem(tr("Absolute"), (int)CropMode::Absolute);
    m_ui.cropModeComboBox->addItem(tr("Relative"), (int)CropMode::Relative);
    CropMode cropMode = m_pSettingsManager->getCropMode();
    int cropModeIndex = m_ui.cropModeComboBox->findData((int)cropMode);
    m_ui.cropModeComboBox->setCurrentIndex(cropModeIndex);
    slotCropModeChanged();

    m_ui.cropZoomRatioSpinBox->setValue(m_pSettingsManager->getCropZoomRatio());
    m_ui.cropPasteToScriptButton->setDefaultAction(
        m_pActionPasteCropSnippetIntoScript);
    m_ui.cropPanel->setVisible(false);

    connect(m_ui.cropModeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotCropModeChanged()));
    connect(m_ui.cropLeftSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropLeftValueChanged(int)));
    connect(m_ui.cropTopSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropTopValueChanged(int)));
    connect(m_ui.cropWidthSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropWidthValueChanged(int)));
    connect(m_ui.cropHeightSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropHeightValueChanged(int)));
    connect(m_ui.cropRightSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropRightValueChanged(int)));
    connect(m_ui.cropBottomSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropBottomValueChanged(int)));
    connect(m_ui.cropZoomRatioSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotCropZoomRatioValueChanged(int)));
}

// END OF void PreviewDialog::setUpCropPanel()
//==============================================================================

bool PreviewDialog::requestShowFrame(int a_frameNumber)
{
    if (!m_pVapourSynthScriptProcessor->isInitialized()) {
        return false;
    }

    if ((m_frameShown != -1) && (m_frameShown != m_frameExpected)) {
        return false;
    }

    m_pVapourSynthScriptProcessor->requestFrameAsync(a_frameNumber, 0, true);
    return true;
}

// END OF bool PreviewDialog::requestShowFrame(int a_frameNumber)
//==============================================================================

void PreviewDialog::setPreviewPixmap()
{
    if (m_ui.cropPanel->isVisible()) {
        int cropLeft = m_ui.cropLeftSpinBox->value();
        int cropTop = m_ui.cropTopSpinBox->value();
        int cropWidth = m_ui.cropWidthSpinBox->value();
        int cropHeight = m_ui.cropHeightSpinBox->value();
        QImage croppedImage = m_framePixmap.copy(cropLeft, cropTop,
                                cropWidth, cropHeight);
        int ratio = m_ui.cropZoomRatioSpinBox->value();

        if (ratio == 1) {
            m_ui.previewArea->setPixmap(croppedImage);
            return;
        }

        QImage zoomedImage = croppedImage.scaled(
                                   croppedImage.width() * ratio, croppedImage.height() * ratio,
                                   Qt::KeepAspectRatio, Qt::FastTransformation);
        m_ui.previewArea->setPixmap(croppedImage);
        return;
    }

    ZoomMode zoomMode = (ZoomMode)m_ui.zoomModeComboBox->currentData().toInt();

    if (zoomMode == ZoomMode::NoZoom) {
        m_ui.previewArea->setPixmap(m_framePixmap);
        return;
    }

    QImage previewPixmap;
    int frameWidth = 0;
    int frameHeight = 0;
    Qt::TransformationMode scaleMode = (Qt::TransformationMode)
                                       m_ui.scaleModeComboBox->currentData().toInt();

    if (zoomMode == ZoomMode::FixedRatio) {
        double ratio = m_ui.zoomRatioSpinBox->value();
        frameWidth = m_framePixmap.width() * ratio;
        frameHeight = m_framePixmap.height() * ratio;
    } else {
        QRect previewRect = m_ui.previewArea->geometry();
        int cropSize = m_ui.previewArea->frameWidth() * 2;
        frameWidth = previewRect.width() - cropSize;
        frameHeight = previewRect.height() - cropSize;
    }

    previewPixmap = m_framePixmap.scaled(frameWidth, frameHeight,
                                         Qt::KeepAspectRatio, scaleMode);
    m_ui.previewArea->setPixmap(previewPixmap);
}

// END OF bool void PreviewDialog::setPreviewPixmap()
//==============================================================================

void PreviewDialog::recalculateCropMods()
{
    QSpinBox *cropSpinBoxes[] = {m_ui.cropLeftSpinBox, m_ui.cropTopSpinBox,
                                 m_ui.cropWidthSpinBox, m_ui.cropHeightSpinBox, m_ui.cropRightSpinBox,
                                 m_ui.cropBottomSpinBox
                                };

    for (QSpinBox *pSpinBox : cropSpinBoxes) {
        int value = pSpinBox->value();

        if (value == 0) {
            pSpinBox->setSuffix("");
        } else {
            int sizeMod = vsedit::mod(value);
            pSpinBox->setSuffix(QString(" |%1|").arg(sizeMod));
        }
    }
}

// END OF void PreviewDialog::recalculateCropMods()
//==============================================================================

void PreviewDialog::resetCropSpinBoxes()
{
    BEGIN_CROP_VALUES_CHANGE

    m_ui.cropLeftSpinBox->setMaximum(m_cpVideoInfo->width - 1);
    m_ui.cropLeftSpinBox->setValue(0);
    m_ui.cropTopSpinBox->setMaximum(m_cpVideoInfo->height - 1);
    m_ui.cropTopSpinBox->setValue(0);

    m_ui.cropWidthSpinBox->setMaximum(m_cpVideoInfo->width);
    m_ui.cropWidthSpinBox->setValue(m_cpVideoInfo->width);
    m_ui.cropHeightSpinBox->setMaximum(m_cpVideoInfo->height);
    m_ui.cropHeightSpinBox->setValue(m_cpVideoInfo->height);

    m_ui.cropRightSpinBox->setMaximum(m_cpVideoInfo->width - 1);
    m_ui.cropRightSpinBox->setValue(0);
    m_ui.cropBottomSpinBox->setMaximum(m_cpVideoInfo->height - 1);
    m_ui.cropBottomSpinBox->setValue(0);

    recalculateCropMods();

    END_CROP_VALUES_CHANGE
}

// END OF void PreviewDialog::resetCropSpinBoxes()
//==============================================================================

void PreviewDialog::setCurrentFrame(const VSFrameRef *a_cpOutputFrameRef,
                                    const VSFrameRef *a_cpPreviewFrameRef)
{
    Q_ASSERT(m_cpVSAPI);
    m_cpVSAPI->freeFrame(m_cpFrameRef);
    m_cpFrameRef = a_cpOutputFrameRef;
    m_framePixmap = qimageFromCompatBGR32(a_cpPreviewFrameRef);
    m_cpVSAPI->freeFrame(a_cpPreviewFrameRef);
    setPreviewPixmap();
    m_ui.previewArea->checkMouseOverPreview(QCursor::pos());
}

// END OF void PreviewDialog::setCurrentFrame(
//		const VSFrameRef * a_cpOutputFrameRef,
//		const VSFrameRef * a_cpPreviewFrameRef)
//==============================================================================

double PreviewDialog::valueAtPoint(size_t a_x, size_t a_y, int a_plane)
{
    Q_ASSERT(m_cpVSAPI);

    if (!m_cpFrameRef) {
        return 0.0;
    }

    const VSFormat *cpFormat = m_cpVSAPI->getFrameFormat(m_cpFrameRef);

    Q_ASSERT((a_plane >= 0) && (a_plane < cpFormat->numPlanes));

    const uint8_t *cpPlane =
        m_cpVSAPI->getReadPtr(m_cpFrameRef, a_plane);

    size_t x = a_x;
    size_t y = a_y;

    if (a_plane != 0) {
        x = (a_x >> cpFormat->subSamplingW);
        y = (a_y >> cpFormat->subSamplingH);
    }

    int stride = m_cpVSAPI->getStride(m_cpFrameRef, a_plane);
    const uint8_t *cpLine = cpPlane + y * stride;

    double value = 0.0;

    if (cpFormat->sampleType == stInteger) {
        if (cpFormat->bytesPerSample == 1) {
            value = (double)cpLine[x];
        } else if (cpFormat->bytesPerSample == 2) {
            // TODO: when c++20, use std::assume_aligned
            value = (double)((const uint16_t *)cpLine)[x];
        } else if (cpFormat->bytesPerSample == 4) {
            value = (double)((const uint32_t *)cpLine)[x];
        }
    } else if (cpFormat->sampleType == stFloat) {
        if (cpFormat->bytesPerSample == 2) {
            vsedit::FP16 half;
            half.u = ((const uint16_t *)cpLine)[x];
            vsedit::FP32 single = vsedit::halfToSingle(half);
            value = (double)single.f;
        } else if (cpFormat->bytesPerSample == 4) {
            value = (double)((const float *)cpLine)[x];
        }
    }

    return value;
}

// END OF double PreviewDialog::valueAtPoint(size_t a_x, size_t a_y,
//		int a_plane)
//==============================================================================

QImage PreviewDialog::qimageFromCompatBGR32(
    const VSFrameRef *a_cpFrameRef)
{
    if ((!m_cpVSAPI) || (!a_cpFrameRef)) {
        return QImage();
    }

    const VSFormat *cpFormat = m_cpVSAPI->getFrameFormat(a_cpFrameRef);
    Q_ASSERT(cpFormat);

    if (cpFormat->id != pfCompatBGR32) {
        QString errorString = tr("Error forming pixmap from frame. "
                                 "Expected format CompatBGR32. Instead got \'%1\'.")
                              .arg(cpFormat->name);
        emit signalWriteLogMessage(mtCritical, errorString);
        return QImage();
    }

    int width = m_cpVSAPI->getFrameWidth(a_cpFrameRef, 0);
    int height = m_cpVSAPI->getFrameHeight(a_cpFrameRef, 0);
    const void *pData = m_cpVSAPI->getReadPtr(a_cpFrameRef, 0);
    int stride = m_cpVSAPI->getStride(a_cpFrameRef, 0);
    return QImage((const uchar *)pData, width, height,
                      stride, QImage::Format_RGB32)
            .mirrored();
}

// END OF QPixmap PreviewDialog::pixmapFromCompatBGR32(
//		const VSFrameRef * a_cpFrameRef)
//==============================================================================

void PreviewDialog::setTitle()
{
    QString l_scriptName = scriptName();
    QString scriptNameTitle =
        l_scriptName.isEmpty() ? tr("(Untitled)") : l_scriptName;
    QString title = tr("Preview - ") + scriptNameTitle;
    setWindowTitle(title);
}

// END OF void PreviewDialog::setTitle()
//==============================================================================

void PreviewDialog::saveTimelineBookmarks()
{
    QString l_scriptName = scriptName();

    if (l_scriptName.isEmpty()) {
        return;
    }

    QString bookmarksFilePath = l_scriptName +
                                QString(TIMELINE_BOOKMARKS_FILE_SUFFIX);
    QFile bookmarksFile(bookmarksFilePath);

    if (!bookmarksFile.open(QIODevice::WriteOnly)) {
        return;
    }

    std::set<int> bookmarks = m_ui.frameNumberSlider->bookmarks();
    QStringList bookmarksStringList;

    for (int i : bookmarks) {
        bookmarksStringList += QString::number(i);
    }

    QString bookmarksString = bookmarksStringList.join(", ");
    bookmarksFile.write(bookmarksString.toUtf8());
    bookmarksFile.close();
}

// END OF void PreviewDialog::saveTimelineBookmarks()
//==============================================================================

void PreviewDialog::loadTimelineBookmarks()
{
    std::set<int> bookmarks;

    QString l_scriptName = scriptName();

    if (l_scriptName.isEmpty()) {
        m_ui.frameNumberSlider->setBookmarks(bookmarks);
        return;
    }

    QString bookmarksFilePath = l_scriptName +
                                QString(TIMELINE_BOOKMARKS_FILE_SUFFIX);
    QFile bookmarksFile(bookmarksFilePath);

    if (!bookmarksFile.open(QIODevice::ReadOnly)) {
        m_ui.frameNumberSlider->setBookmarks(bookmarks);
        return;
    }

    QString bookmarksString = tr(bookmarksFile.readAll().data());
    bookmarksFile.close();

    QStringList bookmarksStringList = bookmarksString.split(",");

    for (const QString &string : bookmarksStringList) {
        bool converted = false;
        int i = string.simplified().toInt(&converted);

        if (converted) {
            bookmarks.insert(i);
        }
    }

    m_ui.frameNumberSlider->setBookmarks(bookmarks);
}

// END OF void PreviewDialog::loadTimelineBookmarks()
//==============================================================================

void PreviewDialog::saveGeometryDelayed()
{
    QApplication::processEvents();

    if (!isMaximized()) {
        m_windowGeometry = saveGeometry();
        m_pGeometrySaveTimer->start();
    }
}

// END OF void PreviewDialog::saveGeometryDelayed()
//==============================================================================
