#include "vs_script_processor_dialog.h"

#include "../common/helpers.h"
#include "../settings/settings_manager.h"
#include "vapoursynth_script_processor.h"
#include "vs_script_library.h"

#include <vapoursynth/VapourSynth.h>

#include <cassert>
#include <QCloseEvent>
#include <QStatusBar>
#include <QLabel>
#include <QLayout>

//==============================================================================

VSScriptProcessorDialog::VSScriptProcessorDialog(
	SettingsManager * a_pSettingsManager, VSScriptLibrary * a_pVSScriptLibrary,
	QWidget * a_pParent, Qt::WindowFlags a_flags):
	  QDialog(a_pParent, a_flags)
	, m_pSettingsManager(a_pSettingsManager)
	, m_pVSScriptLibrary(a_pVSScriptLibrary)
	, m_pVapourSynthScriptProcessor(nullptr)
	, m_cpVSAPI(nullptr)
	, m_cpVideoInfo(nullptr)
	, m_framesInQueue(0)
	, m_framesInProcess(0)
	, m_maxThreads(0)
	, m_wantToFinalize(false)
	, m_pStatusBar(nullptr)
	, m_pStatusBarWidget(nullptr)
	, m_readyPixmap(":tick.png")
	, m_busyPixmap(":busy.png")
	, m_errorPixmap(":cross.png")
	, m_cachedFramesLimit(100)
{
	assert(m_pSettingsManager);
	assert(m_pVSScriptLibrary);

	connect(m_pVSScriptLibrary,
		SIGNAL(signalWriteLogMessage(int, const QString &)),
		this, SLOT(slotWriteLogMessage(int, const QString &)));

	m_cpVSAPI = m_pVSScriptLibrary->getVSAPI();
	assert(m_cpVSAPI);

	m_pVapourSynthScriptProcessor = new VapourSynthScriptProcessor(
		m_pSettingsManager, m_pVSScriptLibrary, this);

	m_pStatusBarWidget = new ScriptStatusBarWidget();

	connect(m_pVapourSynthScriptProcessor,
		SIGNAL(signalWriteLogMessage(int, const QString &)),
		this, SLOT(slotWriteLogMessage(int, const QString &)));
	connect(m_pVapourSynthScriptProcessor,
		SIGNAL(signalFrameQueueStateChanged(size_t, size_t, size_t)),
		this, SLOT(slotFrameQueueStateChanged(size_t, size_t, size_t)));
	connect(m_pVapourSynthScriptProcessor,
		SIGNAL(signalCoreFramebufferUsedBytes(int64_t)),
		this, SLOT(slotCoreFramebufferUsedBytes(int64_t)));
	connect(m_pVapourSynthScriptProcessor,
		SIGNAL(signalDistributeFrame(int, int, const VSFrameRef *,
			const VSFrameRef *)),
		this, SLOT(slotReceiveFrame(int, int, const VSFrameRef *,
			const VSFrameRef *)));
	connect(m_pVapourSynthScriptProcessor,
		SIGNAL(signalFrameRequestDiscarded(int, int, const QString &)),
		this, SLOT(slotFrameRequestDiscarded(int, int, const QString &)));
}

// END OF VSScriptProcessorDialog::VSScriptProcessorDialog(
//		SettingsManager * a_pSettingsManager,
//		VSScriptLibrary * a_pVSScriptLibrary,
//		QWidget * a_pParent, Qt::WindowFlags a_flags)
//==============================================================================

VSScriptProcessorDialog::~VSScriptProcessorDialog()
{
	stopAndCleanUp();
	m_pVapourSynthScriptProcessor->finalize();
}

// END OF VSScriptProcessorDialog::~VSScriptProcessorDialog()
//==============================================================================

bool VSScriptProcessorDialog::initialize(const QString & a_script,
	const QString & a_scriptName)
{
	assert(m_pVapourSynthScriptProcessor);

	if(m_pVapourSynthScriptProcessor->isInitialized())
	{
		stopAndCleanUp();
		bool finalized = m_pVapourSynthScriptProcessor->finalize();
		if(!finalized)
		{
			m_wantToFinalize = true;
			return false;
		}
	}

	bool initialized = m_pVapourSynthScriptProcessor->initialize(a_script,
		a_scriptName);
	if(!initialized)
	{
		if(isVisible())
			hide();
		return false;
	}

	m_cpVideoInfo = m_pVapourSynthScriptProcessor->videoInfo();
	assert(m_cpVideoInfo);

	m_pStatusBarWidget->setVideoInfo(m_cpVideoInfo);

	return true;
}

// END OF bool VSScriptProcessorDialog::initialize(const QString & a_script,
//		const QString & a_scriptName)
//==============================================================================

bool VSScriptProcessorDialog::busy() const
{
	return ((m_framesInProcess + m_framesInQueue) != 0);
}

// END OF bool VSScriptProcessorDialog::busy()
//==============================================================================

const QString & VSScriptProcessorDialog::script() const
{
	return m_pVapourSynthScriptProcessor->script();
}

// END OF const QString & VSScriptProcessorDialog::script() const
//==============================================================================

const QString & VSScriptProcessorDialog::scriptName() const
{
	return m_pVapourSynthScriptProcessor->scriptName();
}

// END OF const QString & VSScriptProcessorDialog::scriptName() const
//==============================================================================

void VSScriptProcessorDialog::setScriptName(const QString & a_scriptName)
{
	m_pVapourSynthScriptProcessor->setScriptName(a_scriptName);
}

// END OF void VSScriptProcessorDialog::setScriptName(
//		const QString & a_scriptName)
//==============================================================================

void VSScriptProcessorDialog::slotWriteLogMessage(int a_messageType,
	const QString & a_message)
{
	emit signalWriteLogMessage(a_messageType, a_message);
}

// END OF void VSScriptProcessorDialog::slotWriteLogMessage(int a_messageType,
//		const QString & a_message)
//==============================================================================

void VSScriptProcessorDialog::slotFrameQueueStateChanged(size_t a_inQueue,
	size_t a_inProcess, size_t a_maxThreads)
{
	m_framesInQueue = a_inQueue;
	m_framesInProcess = a_inProcess;
	m_maxThreads = a_maxThreads;

	m_pStatusBarWidget->setQueueState(m_framesInQueue, m_framesInProcess,
		m_maxThreads);

	if(m_wantToFinalize && (m_framesInProcess == 0) && (m_framesInQueue == 0))
	{
		m_wantToFinalize = false;
		QString message = trUtf8("Script processor has finished its work. "
			"It is safe to repeat the failed action.");
		slotWriteLogMessage(mtDebug, message);
	}
}

// END OF void VSScriptProcessorDialog::slotFrameQueueStateChanged(
//		size_t a_inQueue, size_t a_inProcess, size_t a_maxThreads)
//==============================================================================

void VSScriptProcessorDialog::slotCoreFramebufferUsedBytes(int64_t a_bytes)
{
	m_pStatusBarWidget->setCoreFramebufferUsedBytes(a_bytes);
}

// END OF void VSScriptProcessorDialog::slotCoreFramebufferUsedBytes(
//		int64_t a_bytes)
//==============================================================================

void VSScriptProcessorDialog::closeEvent(QCloseEvent * a_pEvent)
{
    stopAndCleanUp();

    bool finalized = m_pVapourSynthScriptProcessor->finalize();
	if(!finalized)
	{
		m_wantToFinalize = true;
		a_pEvent->ignore();
		return;
	}

	QDialog::closeEvent(a_pEvent);
}

// END OF void VSScriptProcessorDialog::closeEvent(QCloseEvent * a_pEvent)
//==============================================================================

void VSScriptProcessorDialog::stopAndCleanUp()
{
	clearFramesCache();
	m_cpVideoInfo = nullptr;
}

// END OF void VSScriptProcessorDialog::stopAndCleanUp()
//==============================================================================

void VSScriptProcessorDialog::clearFramesCache()
{
	if(m_framesCache.empty())
		return;

	assert(m_cpVSAPI);
	for(Frame & frame : m_framesCache)
	{
		m_cpVSAPI->freeFrame(frame.cpOutputFrameRef);
		m_cpVSAPI->freeFrame(frame.cpPreviewFrameRef);
	}
	m_framesCache.clear();
}

// END OF void VSScriptProcessorDialog::stopAndCleanUp()
//==============================================================================

void VSScriptProcessorDialog::createStatusBar()
{
	QLayout * pLayout = layout();
	assert(pLayout);
	if(!pLayout)
		return;

	m_pStatusBar = new QStatusBar(this);
	pLayout->addWidget(m_pStatusBar);

	m_pStatusBar->addPermanentWidget(m_pStatusBarWidget, 1);
}

// END OF void VSScriptProcessorDialog::createStatusBar()
//==============================================================================
