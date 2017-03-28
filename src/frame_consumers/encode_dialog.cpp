#include "encode_dialog.h"

#include "../common/helpers.h"
#include "../vapoursynth/vapoursynth_script_processor.h"
#include "../settings/settings_dialog.h"
#include "frame_header_writers/frame_header_writer_null.h"
#include "frame_header_writers/frame_header_writer_y4m.h"

#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

#include <QMessageBox>
#include <QFileDialog>
#include <cassert>
#include <algorithm>

#ifdef Q_OS_WIN
	#include <QWinTaskbarButton>
	#include <QWinTaskbarProgress>
#endif

//==============================================================================

EncodeDialog::EncodeDialog(SettingsManager * a_pSettingsManager,
	VSScriptLibrary * a_pVSScriptLibrary, QWidget * a_pParent) :
	VSScriptProcessorDialog(a_pSettingsManager, a_pVSScriptLibrary, a_pParent,
	(Qt::WindowFlags)0
		| Qt::Window
		| Qt::CustomizeWindowHint
		| Qt::WindowMinimizeButtonHint
		| Qt::WindowCloseButtonHint
		)
	, m_firstFrame(-1)
	, m_lastFrame(-1)
	, m_framesTotal(0)
	, m_framesProcessed(0)
	, m_lastFrameProcessed(-1)
	, m_lastFrameRequested(-1)
	, m_state(State::Idle)
	, m_bytesToWrite(0)
	, m_bytesWritten(0)
	, m_headerType(EncodingHeaderType::NoHeader)
	, m_pFrameHeaderWriter(nullptr)

#ifdef Q_OS_WIN
	, m_pWinTaskbarButton(nullptr)
	, m_pWinTaskbarProgress(nullptr)
#endif
{
	m_ui.setupUi(this);
	setWindowIcon(QIcon(":film_save.png"));

	fillVariables();

	createStatusBar();

	m_ui.executableBrowseButton->setIcon(QIcon(":folder.png"));

	m_ui.argumentsHelpButton->setIcon(QIcon(":information.png"));

	m_ui.feedbackTextEdit->setName("encode_log");
	m_ui.feedbackTextEdit->setSettingsManager(m_pSettingsManager);
	m_ui.feedbackTextEdit->loadSettings();

	m_ui.headerTypeComboBox->addItem(trUtf8("No header"),
		(int)EncodingHeaderType::NoHeader);
	m_ui.headerTypeComboBox->addItem(trUtf8("Y4M"),
		(int)EncodingHeaderType::Y4M);

	connect(m_ui.wholeVideoButton, SIGNAL(clicked()),
		this, SLOT(slotWholeVideoButtonPressed()));
	connect(m_ui.startStopEncodeButton, SIGNAL(clicked()),
		this, SLOT(slotStartStopEncodeButtonPressed()));
	connect(m_ui.executableBrowseButton, SIGNAL(clicked()),
		this, SLOT(slotExecutableBrowseButtonPressed()));
	connect(m_ui.argumentsHelpButton, SIGNAL(clicked()),
		this, SLOT(slotArgumentsHelpButtonPressed()));

	connect(&m_encoder, SIGNAL(started()),
		this, SLOT(slotEncoderStarted()));
	connect(&m_encoder, SIGNAL(finished(int, QProcess::ExitStatus)),
		this, SLOT(slotEncoderFinished(int, QProcess::ExitStatus)));
	connect(&m_encoder, SIGNAL(error(QProcess::ProcessError)),
		this, SLOT(slotEncoderError(QProcess::ProcessError)));
	connect(&m_encoder, SIGNAL(readChannelFinished()),
		this, SLOT(slotEncoderReadChannelFinished()));
	connect(&m_encoder, SIGNAL(bytesWritten(qint64)),
		this, SLOT(slotEncoderBytesWritten(qint64)));
	connect(&m_encoder, SIGNAL(readyReadStandardError()),
		this, SLOT(slotEncoderReadyReadStandardError()));

	setUpEncodingPresets();
}

// END OF EncodeDialog::EncodeDialog(SettingsManager * a_pSettingsManager,
//		VSScriptLibrary * a_pVSScriptLibrary, QWidget * a_pParent)
//==============================================================================

EncodeDialog::~EncodeDialog()
{
}

// END OF EncodeDialog::~EncodeDialog()
//==============================================================================

bool EncodeDialog::initialize(const QString & a_script,
	const QString & a_scriptName)
{
	bool initialized =
		VSScriptProcessorDialog::initialize(a_script, a_scriptName);
	if(!initialized)
		emit signalWriteLogMessage(mtCritical,
			m_pVapourSynthScriptProcessor->error());
	return initialized;
}

// END OF bool EncodeDialog::initialize(const QString & a_script,
//		const QString & a_scriptName)
//==============================================================================

void EncodeDialog::call()
{
	if(m_state != State::Idle)
	{
		show();
		return;
	}

	if((!m_pVapourSynthScriptProcessor->isInitialized()) || m_wantToFinalize)
		return;

	assert(m_cpVideoInfo);

	m_ui.feedbackTextEdit->clear();
	setWindowTitle(trUtf8("Encode: %1").arg(scriptName()));
	QString text = trUtf8("Ready to encode script %1").arg(scriptName());
	m_ui.feedbackTextEdit->addEntry(text);
	m_ui.metricsEdit->clear();
	m_firstFrame = 0;
	m_lastFrame = m_cpVideoInfo->numFrames - 1;
	m_ui.fromFrameSpinBox->setMaximum(m_lastFrame);
	m_ui.toFrameSpinBox->setMaximum(m_lastFrame);
	m_ui.fromFrameSpinBox->setValue(m_firstFrame);
	m_ui.toFrameSpinBox->setValue(m_lastFrame);
	m_ui.processingProgressBar->setMaximum(m_lastFrame);
	m_ui.processingProgressBar->setValue(0);
	m_state = State::Idle;
	show();

#ifdef Q_OS_WIN
	if(!m_pWinTaskbarButton)
	{
		m_pWinTaskbarButton = new QWinTaskbarButton(this);
		m_pWinTaskbarButton->setWindow(windowHandle());
		m_pWinTaskbarProgress = m_pWinTaskbarButton->progress();
	}

	m_pWinTaskbarProgress->hide();
#endif
}

// END OF void EncodeDialog::call()
//==============================================================================

void EncodeDialog::slotWriteLogMessage(int a_messageType,
	const QString & a_message)
{
	QString style = vsMessageTypeToStyleName(a_messageType);
	m_ui.feedbackTextEdit->addEntry(a_message, style);
}

// END OF void EncodeDialog::slotWriteLogMessage(int a_messageType,
//		const QString & a_message)
//==============================================================================

void EncodeDialog::slotWholeVideoButtonPressed()
{
	assert(m_cpVideoInfo);
	m_firstFrame = 0;
	m_lastFrame = m_cpVideoInfo->numFrames - 1;
	m_ui.fromFrameSpinBox->setValue(m_firstFrame);
	m_ui.toFrameSpinBox->setValue(m_lastFrame);
}

// END OF void EncodeDialog::slotWholeVideoButtonPressed()
//==============================================================================

void EncodeDialog::slotStartStopEncodeButtonPressed()
{
	State validStates[] = {State::Idle, State::WaitingForFrames,
		State::WritingHeader, State::WritingFrame};
	if(!vsedit::contains(validStates, m_state))
		return;

	if(m_state != State::Idle)
	{
		m_state = State::Aborting;
		stopProcessing();
		return;
	}

	setWindowTitle(trUtf8("Encode: %1").arg(scriptName()));

	m_framesProcessed = 0;
	m_firstFrame = m_ui.fromFrameSpinBox->value();
	m_lastFrame = m_ui.toFrameSpinBox->value();

	if(m_firstFrame > m_lastFrame)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("First frame number is "
			"larger than the last frame number."), LOG_STYLE_WARNING);
			return;
	}

	m_lastFrameProcessed = m_firstFrame - 1;
	m_lastFrameRequested = m_firstFrame - 1;
	m_framesTotal = m_lastFrame - m_firstFrame + 1;
	m_ui.processingProgressBar->setMaximum(m_framesTotal);

	QString executable = vsedit::resolvePathFromApplication(
		m_ui.executablePathEdit->text());

	// Make sure every needed variable is properly set
	// before decoding arguments.
	QString decodedArguments =
		decodeArguments(m_ui.argumentsTextEdit->toPlainText());
	QString commandLine = QString("\"%1\" %2").arg(executable)
		.arg(decodedArguments);

	m_headerType = (EncodingHeaderType)
		m_ui.headerTypeComboBox->currentData().toInt();

	if(m_pFrameHeaderWriter)
		delete m_pFrameHeaderWriter;

	assert(m_cpVSAPI);
	if(m_headerType == EncodingHeaderType::Y4M)
		m_pFrameHeaderWriter =
			new FrameHeaderWriterY4M(m_cpVSAPI, m_cpVideoInfo, this);
	else
		m_pFrameHeaderWriter =
			new FrameHeaderWriterNull(m_cpVSAPI, m_cpVideoInfo, this);

	bool compatibleHeader = m_pFrameHeaderWriter->isCompatible();
	if(!compatibleHeader)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Video is not compatible "
			"with the chosen header."), LOG_STYLE_ERROR);
		return;
	}

	m_ui.feedbackTextEdit->addEntry(trUtf8("Command line:"));
	m_ui.feedbackTextEdit->addEntry(commandLine);

	m_ui.startStopEncodeButton->setText(trUtf8("Stop"));

	m_ui.feedbackTextEdit->addEntry(trUtf8("Checking the encoder sanity."),
		LOG_STYLE_DEBUG);
	m_state = State::CheckingEncoderSanity;

	m_encoder.start(commandLine);
	if(!m_encoder.waitForStarted(3000))
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder wouldn't start."),
			LOG_STYLE_ERROR);
		goIdle();
		return;
	}

	m_encoder.closeWriteChannel();
	if(!m_encoder.waitForFinished(3000))
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Program is not behaving "
			"like a CLI encoder. Terminating."), LOG_STYLE_ERROR);
		m_encoder.kill();
		m_encoder.waitForFinished(-1);
		goIdle();
		return;
	}

	m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder seems sane. Starting."),
		LOG_STYLE_DEBUG);
	m_state = State::StartingEncoder;
	m_encoder.start(commandLine);
}

// END OF void EncodeDialog::slotStartStopBenchmarkButtonPressed()
//==============================================================================

void EncodeDialog::slotExecutableBrowseButtonPressed()
{
	QString applicationPath = QCoreApplication::applicationDirPath();
	QFileDialog fileDialog;
	fileDialog.setWindowTitle(trUtf8("Choose encoder executable"));
	fileDialog.setDirectory(applicationPath);

#ifdef Q_OS_WIN
	fileDialog.setNameFilter("*.exe");
#endif

	if(!fileDialog.exec())
		return;

	QStringList filesList = fileDialog.selectedFiles();
	m_ui.executablePathEdit->setText(filesList[0]);
}

// END OF void EncodeDialog::slotExecutableBrowseButtonPressed()
//==============================================================================

void EncodeDialog::slotArgumentsHelpButtonPressed()
{
	QString argumentsHelpString = trUtf8("Use following placeholders:");
	for(const vsedit::VariableToken & variable : m_variables)
	{
		argumentsHelpString += QString("\n%1 - %2")
			.arg(variable.token).arg(variable.description);
	}
	QString title = trUtf8("Encoder arguments");
	QMessageBox::information(this, title, argumentsHelpString);
}

// END OF void EncodeDialog::slotArgumentsHelpButtonPressed()
//==============================================================================

void EncodeDialog::slotEncodingPresetSaveButtonPressed()
{
	EncodingPreset preset(m_ui.encodingPresetComboBox->currentText());
	if(preset.name.isEmpty())
	{
		m_ui.feedbackTextEdit->addEntry(
			trUtf8("Preset name must not be empty."), LOG_STYLE_WARNING);
		return;
	}

	if(preset.type == EncodingType::CLI)
	{
		preset.executablePath = m_ui.executablePathEdit->text();
		if(preset.executablePath.isEmpty())
		{
			m_ui.feedbackTextEdit->addEntry(
				trUtf8("Executable path must not be empty."),
				LOG_STYLE_WARNING);
			return;
		}

		preset.arguments = m_ui.argumentsTextEdit->toPlainText();
	}

	preset.headerType = (EncodingHeaderType)
		m_ui.headerTypeComboBox->currentData().toInt();

	bool success = m_pSettingsManager->saveEncodingPreset(preset);
	if(!success)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error saving preset."),
			LOG_STYLE_ERROR);
		return;
	}

	std::vector<EncodingPreset>::iterator it = std::find(
		m_encodingPresets.begin(), m_encodingPresets.end(), preset);
	if(it == m_encodingPresets.end())
	{
		assert(m_ui.encodingPresetComboBox->findText(preset.name) == -1);
		m_encodingPresets.push_back(preset);
		m_ui.encodingPresetComboBox->addItem(preset.name);
		m_ui.encodingPresetComboBox->model()->sort(0);
	}
	else
	{
		assert(m_ui.encodingPresetComboBox->findText(preset.name) != -1);
		*it = preset;
	}

	m_ui.feedbackTextEdit->addEntry(trUtf8("Preset \'%1\' saved.")
		.arg(preset.name), LOG_STYLE_POSITIVE);
}

// END OF void EncodeDialog::slotEncodingPresetSaveButtonPressed()
//==============================================================================

void EncodeDialog::slotEncodingPresetDeleteButtonPressed()
{
	EncodingPreset preset(m_ui.encodingPresetComboBox->currentText());
	if(preset.name.isEmpty())
		return;

	QMessageBox::StandardButton result = QMessageBox::question(this,
		trUtf8("Delete preset"), trUtf8("Do you really want to delete "
		"preset \'%1\'?").arg(preset.name),
		QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
		QMessageBox::No);
	if(result == QMessageBox::No)
		return;

	std::vector<EncodingPreset>::iterator it = std::find(
		m_encodingPresets.begin(), m_encodingPresets.end(), preset);
	if(it == m_encodingPresets.end())
	{
		assert(m_ui.encodingPresetComboBox->findText(preset.name) == -1);
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error deleting preset. "
			"Preset was never saved."), LOG_STYLE_ERROR);
		return;
	}

	int index = m_ui.encodingPresetComboBox->findText(preset.name);
	assert(index != -1);
	m_ui.encodingPresetComboBox->removeItem(index);
	m_encodingPresets.erase(it);
	m_ui.encodingPresetComboBox->setCurrentIndex(0);
	slotEncodingPresetComboBoxActivated(
		m_ui.encodingPresetComboBox->currentText());

	bool success = m_pSettingsManager->deleteEncodingPreset(preset.name);
	if(!success)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error deleting "
			"preset \'%1\'.").arg(preset.name), LOG_STYLE_ERROR);
		return;
	}

	m_ui.feedbackTextEdit->addEntry(trUtf8("Preset \'%1\' deleted.")
		.arg(preset.name), LOG_STYLE_POSITIVE);
}

// END OF void EncodeDialog::slotEncodingPresetDeleteButtonPressed()
//==============================================================================

void EncodeDialog::slotEncodingPresetComboBoxActivated(const QString & a_text)
{
	if(a_text.isEmpty())
	{
		m_ui.executablePathEdit->clear();
		m_ui.argumentsTextEdit->clear();
		return;
	}

	EncodingPreset preset(a_text);

	std::vector<EncodingPreset>::iterator it = std::find(
		m_encodingPresets.begin(), m_encodingPresets.end(), preset);
	if(it == m_encodingPresets.end())
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error. There is no preset "
			"named \'%1\'.").arg(preset.name), LOG_STYLE_ERROR);
		return;
	}

	preset = *it;

	m_ui.executablePathEdit->setText(preset.executablePath);
	m_ui.argumentsTextEdit->setPlainText(preset.arguments);

	int headerTypeIndex =
		m_ui.headerTypeComboBox->findData((int)preset.headerType);
	if(headerTypeIndex < 0)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error. Preset \'%1\' "
			"has unknown header type.").arg(preset.name), LOG_STYLE_ERROR);
		headerTypeIndex = 0;
	}
	m_ui.headerTypeComboBox->setCurrentIndex(headerTypeIndex);
}

// END OF void EncodeDialog::slotEncodingPresetComboBoxActivated(
//		const QString & a_text)
//==============================================================================

void EncodeDialog::slotReceiveFrame(int a_frameNumber, int a_outputIndex,
	const VSFrameRef * a_cpOutputFrameRef,
	const VSFrameRef * a_cpPreviewFrameRef)
{
	(void)a_cpPreviewFrameRef;

	State validStates[] = {State::WaitingForFrames, State::WritingHeader,
		State::WritingFrame};
	if(!vsedit::contains(validStates, m_state))
		return;

	if((a_frameNumber < m_firstFrame) || (a_frameNumber > m_lastFrame))
		return;

	assert(m_cpVSAPI);
	const VSFrameRef * cpFrameRef =
		m_cpVSAPI->cloneFrameRef(a_cpOutputFrameRef);
	Frame newFrame(a_frameNumber, a_outputIndex, cpFrameRef);
	m_framesCache.push_back(newFrame);

	if(m_state == State::WaitingForFrames)
		processFramesQueue();
}

// END OF void EncodeDialog::slotReceiveFrame(int a_frameNumber,
//		int a_outputIndex, const VSFrameRef * a_cpOutputFrameRef,
//		const VSFrameRef * a_cpPreviewFrameRef)
//==============================================================================

void EncodeDialog::slotFrameRequestDiscarded(int a_frameNumber,
	int a_outputIndex, const QString & a_reason)
{
	(void)a_frameNumber;
	(void)a_outputIndex;
	(void)a_reason;

	State validStates[] = {State::WaitingForFrames, State::WritingHeader,
		State::WritingFrame};
	if(!vsedit::contains(validStates, m_state))
		return;

	m_state = State::Aborting;
	stopProcessing();
}

// END OF void EncodeDialog::slotFrameRequestDiscarded(int a_frameNumber,
//		int a_outputIndex, const QString & a_reason)
//==============================================================================

void EncodeDialog::slotEncoderStarted()
{
	if(m_state == State::CheckingEncoderSanity)
		return;

	m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder started. "
		"Beginning encoding."), LOG_STYLE_DEBUG);

	if(!m_encoder.isWritable())
	{
		m_state = State::Aborting;
		m_ui.feedbackTextEdit->addEntry(trUtf8("Can not write to encoder. "
			"Aborting."), LOG_STYLE_ERROR);
		stopProcessing();
		return;
	}

	setWindowTitle(trUtf8("0% Encode: %1").arg(scriptName()));

#ifdef Q_OS_WIN
	assert(m_pWinTaskbarProgress);
	m_pWinTaskbarProgress->setMaximum(m_framesTotal);
	m_pWinTaskbarProgress->setValue(0);
	m_pWinTaskbarProgress->resume();
	m_pWinTaskbarProgress->show();
#endif

	assert(m_pFrameHeaderWriter);
	if(m_pFrameHeaderWriter->needVideoHeader())
	{
		QByteArray videoHeader =
			m_pFrameHeaderWriter->videoHeader(m_framesTotal);

		if(m_headerType == EncodingHeaderType::Y4M)
			m_ui.feedbackTextEdit->addEntry(trUtf8("Y4M header: ") +
				QString::fromLatin1(videoHeader), LOG_STYLE_DEBUG);

		m_bytesToWrite = videoHeader.size();
		if(m_bytesToWrite > 0)
		{
			m_bytesWritten = 0;
			m_state = State::WritingHeader;
			qint64 bytesWritten = m_encoder.write(videoHeader);
			if(bytesWritten < 0)
			{
				m_state = State::Aborting;
				m_ui.feedbackTextEdit->addEntry(
					trUtf8("Error on writing header to encoder. Aborting."),
					LOG_STYLE_ERROR);
				stopProcessing();
				return;
			}

			return;
		}
	}

	m_state = State::WaitingForFrames;
	m_encodeStartTime = hr_clock::now();
	processFramesQueue();
}

// END OF void EncodeDialog::slotEncoderStarted()
//==============================================================================

void EncodeDialog::slotEncoderFinished(int a_exitCode,
	QProcess::ExitStatus a_exitStatus)
{
	State workingStates[] = {State::WaitingForFrames, State::WritingFrame,
		State::WritingHeader};

	if(m_state == State::CheckingEncoderSanity)
		return;
	else if(m_state == State::Idle)
		return;
	else if(m_state == State::Finishing)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Finished encoding."),
			LOG_STYLE_POSITIVE);
	}
	else if(m_state == State::Aborting)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Aborted encoding."),
			LOG_STYLE_WARNING);
	}
	else if(vsedit::contains(workingStates, m_state))
	{
		QString exitStatusString = (a_exitStatus == QProcess::CrashExit) ?
			trUtf8("crash") : trUtf8("normal exit");
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has finished "
			"unexpectedly.\nReason: %1; exit code: %2")
			.arg(exitStatusString).arg(a_exitCode), LOG_STYLE_ERROR);
	}

	stopProcessing();
	goIdle();
}

// END OF void EncodeDialog::slotEncoderFinished(int a_exitCode,
//		QProcess::ExitStatus a_exitStatus)
//==============================================================================

void EncodeDialog::slotEncoderError(QProcess::ProcessError a_error)
{
	if(m_state == State::CheckingEncoderSanity)
		return;

	if(m_state == State::Idle)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has reported an error "
			"while it shouldn't be running at all. Ignoring."),
			LOG_STYLE_WARNING);
		return;
	}

	switch(a_error)
	{
	case QProcess::FailedToStart:
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has failed to start. "
			"Aborting."), LOG_STYLE_ERROR);
		m_state = State::Aborting;
		stopProcessing();
		break;

	case QProcess::Crashed:
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has crashed. "
			"Aborting."), LOG_STYLE_ERROR);
		m_state = State::EncoderCrashed;
		stopProcessing();
		break;

	case QProcess::Timedout:
		break;

	case QProcess::WriteError:
		if(m_state == State::WritingFrame)
		{
			m_ui.feedbackTextEdit->addEntry(trUtf8("Writing to encoder failed. "
				"Aborting."), LOG_STYLE_ERROR);
			m_state = State::Aborting;
			stopProcessing();
		}
		else
		{
			m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has returned a "
				"writing error, but we were not writing. Ignoring."),
				LOG_STYLE_WARNING);
		}
		break;

	case QProcess::ReadError:
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error on reading the encoder "
			"feedback."), LOG_STYLE_WARNING);
		break;

	case QProcess::UnknownError:
		m_ui.feedbackTextEdit->addEntry(trUtf8("Unknown error in encoder."),
			LOG_STYLE_WARNING);
		break;

	default:
		assert(false);
	}
}

// END OF void EncodeDialog::slotEncoderError(QProcess::ProcessError a_error)
//==============================================================================

void EncodeDialog::slotEncoderReadChannelFinished()
{
	if(m_state == State::CheckingEncoderSanity)
		return;

	if(m_state == State::Idle)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has suddenly stopped "
			"accepting data while it shouldn't be running at all. Ignoring."),
			LOG_STYLE_WARNING);
		return;
	}

	if((m_state != State::Finishing) && (m_state != State::Aborting))
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has suddenly stopped "
			"accepting data. Aborting."), LOG_STYLE_ERROR);
		m_state = State::Aborting;
		stopProcessing();
	}
}

// END OF void EncodeDialog::slotEncoderReadChannelFinished()
//==============================================================================

void EncodeDialog::slotEncoderBytesWritten(qint64 a_bytes)
{
	if(m_state == State::CheckingEncoderSanity)
		return;

	if(m_state == State::Idle)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has reported written "
			"data while it shouldn't be running at all. Ignoring."),
			LOG_STYLE_WARNING);
		return;
	}

	if((m_state == State::Aborting) || (m_state == State::Finishing))
		return;

	if((m_state != State::WritingFrame) && (m_state != State::WritingHeader))
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder reports successful "
			"write, but we were not writing anything.\nData written: "
			"%1 bytes.").arg(a_bytes), LOG_STYLE_WARNING);
		return;
	}

	if(a_bytes <= 0)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error on writing data to "
			"encoder.\nExpected to write: %1 bytes. Data written: %2 bytes.\n"
			"Aborting.").arg(m_bytesToWrite).arg(m_bytesWritten),
			LOG_STYLE_ERROR);
		m_state = State::Aborting;
		stopProcessing();
		return;
	}

	m_bytesWritten += a_bytes;

	if((m_bytesWritten + m_encoder.bytesToWrite()) < m_bytesToWrite)
	{
		m_ui.feedbackTextEdit->addEntry(trUtf8("Encoder has lost written "
			"data. Aborting."), LOG_STYLE_ERROR);
		m_state = State::Aborting;
		stopProcessing();
		return;
	}

	if(m_bytesWritten < m_bytesToWrite)
		return;

	assert(m_cpVSAPI);
	if(m_state == State::WritingHeader)
	{
		m_encodeStartTime = hr_clock::now();
	}
	else if(m_state == State::WritingFrame)
	{
		Frame referenceFrame(m_lastFrameProcessed + 1, 0, nullptr);
		std::list<Frame>::iterator it =
			std::find(m_framesCache.begin(), m_framesCache.end(),
			referenceFrame);
		assert(it != m_framesCache.end());

		m_cpVSAPI->freeFrame(it->cpOutputFrameRef);
		m_framesCache.erase(it);
		m_lastFrameProcessed++;
		m_framesProcessed++;
		hr_time_point now = hr_clock::now();
		m_ui.processingProgressBar->setValue(m_framesProcessed);
		double passed = duration_to_double(now - m_encodeStartTime);
		QString passedString = vsedit::timeToString(passed);
		double fps = (double)m_framesProcessed / passed;
		QString text = trUtf8("Time elapsed: %1 - %2 FPS")
			.arg(passedString).arg(QString::number(fps, 'f', 20));

		if(m_framesProcessed < m_framesTotal)
		{
			double estimated = (m_framesTotal - m_framesProcessed) / fps;
			QString estimatedString = vsedit::timeToString(estimated);
			text += trUtf8("; estimated time to finish: %1")
				.arg(estimatedString);
		}

		m_ui.metricsEdit->setText(text);

		int percentage = (int)((double)m_framesProcessed * 100.0 /
			(double)m_framesTotal);
		setWindowTitle(trUtf8("%1% Encode: %2")
			.arg(percentage).arg(scriptName()));

#ifdef Q_OS_WIN
		assert(m_pWinTaskbarProgress);
		m_pWinTaskbarProgress->setValue(m_framesProcessed);
#endif
	}

	m_state = State::WaitingForFrames;
	processFramesQueue();
}

// END OF void EncodeDialog::slotEncoderBytesWritten(qint64 a_bytes)
//==============================================================================

void EncodeDialog::slotEncoderReadyReadStandardError()
{
	outputStandardError();
}

// END OF void EncodeDialog::slotEncoderReadyReadStandardError()
//==============================================================================

void EncodeDialog::stopAndCleanUp()
{
	stopProcessing();
	m_ui.metricsEdit->clear();
	m_ui.processingProgressBar->setValue(0);
	VSScriptProcessorDialog::stopAndCleanUp();
}

// END OF void EncodeDialog::stopAndCleanUp()
//==============================================================================

void EncodeDialog::stopProcessing()
{
	if(m_state == State::Idle)
		return;

	m_pVapourSynthScriptProcessor->flushFrameTicketsQueue();
	clearFramesCache();
	m_framebuffer.clear();

	if(m_encoder.state() == QProcess::Running)
	{
		if(m_state != State::Aborting)
			m_state = State::Finishing;
		m_encoder.closeWriteChannel();
	}
}

// END OF void EncodeDialog::stopProcessing()
//==============================================================================

void EncodeDialog::goIdle()
{
	m_ui.startStopEncodeButton->setText(trUtf8("Start"));
	m_state = State::Idle;

#ifdef Q_OS_WIN
	assert(m_pWinTaskbarProgress);
	if(m_framesProcessed == m_framesTotal)
		m_pWinTaskbarProgress->hide();
	else
		m_pWinTaskbarProgress->stop();
#endif
}

// END OF void EncodeDialog::goIdle()
//==============================================================================

void EncodeDialog::processFramesQueue()
{
	if(m_state != State::WaitingForFrames)
		return;

	if(m_framesProcessed == m_framesTotal)
	{
		assert(m_framesCache.empty());
		m_state = State::Finishing;
		stopProcessing();
		return;
	}

	while((m_lastFrameRequested < m_lastFrame) &&
		(m_framesInProcess < m_maxThreads) &&
		(m_framesCache.size() < m_cachedFramesLimit))
	{
		m_pVapourSynthScriptProcessor->requestFrameAsync(
			m_lastFrameRequested + 1);
		m_lastFrameRequested++;
	}

	Frame frame(m_lastFrameProcessed + 1, 0, nullptr);
	std::list<Frame>::iterator it = std::find(m_framesCache.begin(),
		m_framesCache.end(), frame);
	if(it == m_framesCache.end())
		return;

	frame.cpOutputFrameRef = it->cpOutputFrameRef;

	// VapourSynth frames are padded so every line has aligned address.
	// But encoder expects frames tightly packed. We pack frame lines
	// into an intermediate buffer, because writing whole frame at once
	// is faster than feeding it to encoder line by line.

	size_t currentDataSize = 0;

	assert(m_cpVideoInfo);
	const VSFormat * cpFormat = m_cpVideoInfo->format;
	assert(cpFormat);

	if(m_pFrameHeaderWriter->needFramePrefix())
	{
		QByteArray framePrefix =
			m_pFrameHeaderWriter->framePrefix(frame.cpOutputFrameRef);
		int prefixSize = framePrefix.size();
		if(prefixSize > 0)
		{
			if((size_t)prefixSize > m_framebuffer.size())
				m_framebuffer.resize(prefixSize);
			memcpy(m_framebuffer.data(), framePrefix.data(), prefixSize);
			currentDataSize += prefixSize;
		}
	}

	for(int i = 0; i < cpFormat->numPlanes; ++i)
	{
		const uint8_t * cpPlane =
			m_cpVSAPI->getReadPtr(frame.cpOutputFrameRef, i);
		int stride = m_cpVSAPI->getStride(frame.cpOutputFrameRef, i);
		int width = m_cpVSAPI->getFrameWidth(frame.cpOutputFrameRef, i);
		int height = m_cpVSAPI->getFrameHeight(frame.cpOutputFrameRef, i);
		int bytes = cpFormat->bytesPerSample;

		size_t planeSize = width * bytes * height;
		size_t neededFramebufferSize = currentDataSize + planeSize;
		if(neededFramebufferSize > m_framebuffer.size())
			m_framebuffer.resize(neededFramebufferSize);
		int framebufferStride = width * bytes;

		vs_bitblt(m_framebuffer.data() + currentDataSize, framebufferStride,
			cpPlane, stride, framebufferStride, height);

		currentDataSize += planeSize;
	}

	if(m_pFrameHeaderWriter->needFramePostfix())
	{
		QByteArray framePostfix =
			m_pFrameHeaderWriter->framePostfix(frame.cpOutputFrameRef);
		int postfixSize = framePostfix.size();
		if(postfixSize > 0)
		{
			size_t neededFramebufferSize = currentDataSize + postfixSize;
			if(neededFramebufferSize > m_framebuffer.size())
				m_framebuffer.resize(neededFramebufferSize);
			memcpy(m_framebuffer.data() + currentDataSize,
				framePostfix.data(), postfixSize);
			currentDataSize += postfixSize;
		}
	}

	m_state = State::WritingFrame;
	m_bytesToWrite = currentDataSize;
	m_bytesWritten = 0;
	qint64 bytesWritten =
		m_encoder.write(m_framebuffer.data(), (qint64)m_bytesToWrite);
	if(bytesWritten < 0)
	{
		m_state = State::Aborting;
		m_ui.feedbackTextEdit->addEntry(trUtf8("Error on writing data to "
			"encoder. Aborting."), LOG_STYLE_ERROR);
		stopProcessing();
		return;
	}

	// Wait until encoder reads the frame.
	// Then this function will be called again.
}

// END OF void EncodeDialog::stopProcessing()
//==============================================================================

QString EncodeDialog::decodeArguments(const QString & a_arguments)
{
	QString decodedString = a_arguments.simplified();

	for(const vsedit::VariableToken & variable : m_variables)
	{
		decodedString = decodedString.replace(variable.token,
			variable.evaluate());
	}

	return decodedString;
}

// END OF void QString EncodeDialog::decodeArguments(
//		const QString & a_arguments)
//==============================================================================

void EncodeDialog::outputStandardError()
{
	QByteArray standardError = m_encoder.readAllStandardError();
	QString standardErrorText = QString::fromUtf8(standardError);
	standardErrorText = standardErrorText.trimmed();
	if(!standardErrorText.isEmpty())
		m_ui.feedbackTextEdit->addEntry(standardErrorText);
}

// END OF void EncodeDialog::clearFramesQueue()
//==============================================================================

void EncodeDialog::fillVariables()
{
	m_variables =
	{
		{"{w}", trUtf8("video width"),
			[&]()
			{
				return QString::number(m_cpVideoInfo->width);
			}
		},

		{"{h}", trUtf8("video height"),
			[&]()
			{
				return QString::number(m_cpVideoInfo->height);
			}
		},

		{"{fpsn}", trUtf8("video framerate numerator"),
			[&]()
			{
				return QString::number(m_cpVideoInfo->fpsNum);
			}
		},

		{"{fpsd}", trUtf8("video framerate denominator"),
			[&]()
			{
				return QString::number(m_cpVideoInfo->fpsDen);
			}
		},

		{"{fps}", trUtf8("video framerate as fraction"),
			[&]()
			{
				double fps = (double)m_cpVideoInfo->fpsNum /
					(double)m_cpVideoInfo->fpsDen;
				return QString::number(fps, 'f', 10);
			}
		},

		{"{bits}", trUtf8("video colour bitdepth"),
			[&]()
			{
				return QString::number(m_cpVideoInfo->format->bitsPerSample);
			}
		},

		{"{sd}", trUtf8("script directory"),
			[&]()
			{
				QFileInfo scriptFile(scriptName());
				return scriptFile.canonicalPath();
			}
		},

		{"{sn}", trUtf8("script name without extension"),
			[&]()
			{
				QFileInfo scriptFile(scriptName());
				return scriptFile.completeBaseName();
			}
		},

		{"{f}", trUtf8("total frames number"),
			[&]()
			{
				return QString::number(m_framesTotal);
			}
		},

		{"{ss}", trUtf8("subsampling string (like 420)"),
			[&]()
			{
				const VSFormat * cpFormat = m_cpVideoInfo->format;
				if(!cpFormat)
					return QString();
				return vsedit::subsamplingString(cpFormat->subSamplingW,
					cpFormat->subSamplingH);
			}
		},
	};

	std::sort(m_variables.begin(), m_variables.end(),
		[&](const vsedit::VariableToken & a_first,
			const vsedit::VariableToken & a_second) -> bool
		{
			return (a_first.token.length() > a_second.token.length());
		});
}

// END OF void EncodeDialog::fillVariables()
//==============================================================================

void EncodeDialog::setUpEncodingPresets()
{
	m_encodingPresets = m_pSettingsManager->getAllEncodingPresets();
	for(const EncodingPreset & preset : m_encodingPresets)
		m_ui.encodingPresetComboBox->addItem(preset.name);

	connect(m_ui.encodingPresetSaveButton, SIGNAL(clicked()),
		this, SLOT(slotEncodingPresetSaveButtonPressed()));
	connect(m_ui.encodingPresetDeleteButton, SIGNAL(clicked()),
		this, SLOT(slotEncodingPresetDeleteButtonPressed()));
	connect(m_ui.encodingPresetComboBox, SIGNAL(activated(const QString &)),
		this, SLOT(slotEncodingPresetComboBoxActivated(const QString &)));

	m_ui.encodingPresetComboBox->setCurrentIndex(0);
	slotEncodingPresetComboBoxActivated(
		m_ui.encodingPresetComboBox->currentText());
}

// END OF void EncodeDialog::setUpEncodingPresets()
//==============================================================================
