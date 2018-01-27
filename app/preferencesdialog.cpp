/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2013-2018 Matt Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "ui_generalpage.h"
#include <QComboBox>
#include <QMessageBox>

PreferencesDialog::PreferencesDialog( QWidget* parent ) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::init( PreferenceManager* m )
{
    Q_ASSERT( m != nullptr );
    mPrefManager = m;

    ui->general->setManager( mPrefManager );
    ui->general->updateValues();
    connect( ui->general, &GeneralPage::windowOpacityChange, this, &PreferencesDialog::windowOpacityChange );
    //connect(mPrefManager, &PreferenceManager::effectChanged, general, &GeneralPage::updateValues);

    ui->filesPage->setManager( mPrefManager );
    ui->filesPage->updateValues();

    connect(ui->filesPage, &FilesPage::clearRecentList, this, &PreferencesDialog::clearRecentList);
    connect(this, &PreferencesDialog::updateRecentFileListBtn, ui->filesPage, &FilesPage::updateClearRecentListButton);

    ui->timeline->setManager( mPrefManager );
    ui->timeline->updateValues();

    ui->tools->setManager( mPrefManager );
    ui->tools->updateValues();

    ui->shortcuts->setManager( mPrefManager );
}

void PreferencesDialog::closeEvent(QCloseEvent *)
{
    done( QDialog::Accepted );
}

void PreferencesDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
        current = previous;

    ui->pagesWidget->setCurrentIndex(ui->contentsWidget->row(current));
}

void PreferencesDialog::updateRecentListBtn(bool isEmpty)
{
    if (isEmpty == true)
    {
        emit updateRecentFileListBtn();
    }
}

GeneralPage::GeneralPage(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::GeneralPage)
{
    ui->setupUi(this);

    QSettings settings( PENCIL2D, PENCIL2D );

    ui->languageCombo->addItem( tr( "Czech" ), "cs" );
    ui->languageCombo->addItem( tr( "Danish" ), "da" );
    ui->languageCombo->addItem( tr( "English" ), "en" );
    ui->languageCombo->addItem( tr( "German" ), "de" );
    ui->languageCombo->addItem( tr( "Spanish" ), "es" );
    ui->languageCombo->addItem( tr( "French" ), "fr" );
    ui->languageCombo->addItem( tr( "Hungarian" ), "hu-HU" );
    ui->languageCombo->addItem( tr( "Italian" ), "it" );
    ui->languageCombo->addItem( tr( "Japanese" ), "ja" );
    ui->languageCombo->addItem( tr( "Portuguese - Brazil" ), "pt-BR" );
    ui->languageCombo->addItem( tr( "Russian" ), "ru" );
    ui->languageCombo->addItem( tr( "Chinese - Taiwan" ), "zh-TW" );

    int value = settings.value("windowOpacity").toInt();
    ui->windowOpacityLevel->setValue( 100 - value );

    QPixmap previewCheckerboard( ":background/checkerboard.png" );
    QPixmap previewWhite(32,32);
    QPixmap previewGrey(32,32);
    QPixmap previewDots( ":background/dots.png" );
    QPixmap previewWeave( ":background/weave.jpg" );

    previewWhite.fill( Qt::white );
    previewGrey.fill( Qt:: lightGray );

    ui->checkerBackgroundButton->setIcon( previewCheckerboard.scaled(32, 32) );
    ui->whiteBackgroundButton->setIcon( previewWhite );
    ui->greyBackgroundButton->setIcon( previewGrey );
    ui->dotsBackgroundButton->setIcon( previewDots.scaled(32, 32) );
    ui->weaveBackgroundButton->setIcon( previewWeave.scaled(32, 32) );
    ui->backgroundButtons->setId(ui->checkerBackgroundButton, 1);
    ui->backgroundButtons->setId(ui->whiteBackgroundButton, 2);
    ui->backgroundButtons->setId(ui->greyBackgroundButton, 3);
    ui->backgroundButtons->setId(ui->dotsBackgroundButton, 4);
    ui->backgroundButtons->setId(ui->weaveBackgroundButton, 5);

    gridSize = settings.value("gridSize").toInt();
    ui->gridSizeInput->setValue( gridSize );

    value = settings.value("curveSmoothing").toInt();
    ui->curveSmoothingLevel->setValue( value );

    connect( ui->windowOpacityLevel, &QSlider::valueChanged, this, &GeneralPage::windowOpacityChange );
}

GeneralPage::~GeneralPage()
{
    delete ui;
}

void GeneralPage::resizeEvent(QResizeEvent* event)
{
    int size = 0;
    if (this->height() < 560 ) {
       size = this->height();
    } else if (this->height() >= 560) {
        size = 560;
    }
    ui->scrollArea->setMinimumHeight(size);
    QWidget::resizeEvent(event);
}


void GeneralPage::updateValues()
{
    int index = ui->languageCombo->findData( mManager->getString( SETTING::LANGUAGE ) );

    if ( index >= 0 )
    {
        ui->languageCombo->blockSignals( true );
        ui->languageCombo->setCurrentIndex( index );
        ui->languageCombo->blockSignals( false );
    }

    ui->curveSmoothingLevel->setValue(mManager->getInt(SETTING::CURVE_SMOOTHING));
    ui->windowOpacityLevel->setValue(100 - mManager->getInt(SETTING::WINDOW_OPACITY));
    ui->shadowsBox->setChecked(mManager->isOn(SETTING::SHADOW));
    ui->toolCursorsBox->setChecked(mManager->isOn(SETTING::TOOL_CURSOR));
    ui->antialiasingBox->setChecked(mManager->isOn(SETTING::ANTIALIAS));
    ui->dottedCursorBox->setChecked(mManager->isOn(SETTING::DOTTED_CURSOR));
    ui->gridSizeInput->setValue(mManager->getInt(SETTING::GRID_SIZE));
    ui->gridCheckBox->setChecked(mManager->isOn(SETTING::GRID));

    ui->highResBox->setChecked(mManager->isOn(SETTING::HIGH_RESOLUTION));

    QString bgName = mManager->getString(SETTING::BACKGROUND_STYLE);
    if (bgName == "checkerboard") {
        ui->backgroundButtons->button(1)->setChecked(true);
    }
    if (bgName == "white") {
        ui->backgroundButtons->button(2)->setChecked(true);
    }
    if (bgName == "grey") {
        ui->backgroundButtons->button(3)->setChecked(true);
    }
    if (bgName == "dots") {
        ui->backgroundButtons->button(4)->setChecked(true);
    }
    if (bgName == "weave") {
        ui->backgroundButtons->button(5)->setChecked(true);
    }
}

void GeneralPage::languageChanged( int i )
{
    QString strLocale = ui->languageCombo->itemData( i ).toString();
    mManager->set( SETTING::LANGUAGE, strLocale );

    QMessageBox::warning( this,
                          tr( "Restart Required" ),
                          tr( "The language change will take effect after a restart of Pencil2D" ) );
}

void GeneralPage::backgroundChange(int value)
{
    QString brushName = "white";
    switch (value)
	{
    case 1:
        brushName = "checkerboard";
        break;
    case 2:
        brushName = "white";
        break;
    case 3:
        brushName = "grey";
        break;
    case 4:
        brushName = "dots";
        break;
    case 5:
        brushName = "weave";
        break;
    default:
        break;
    }
    mManager->set(SETTING::BACKGROUND_STYLE, brushName);
}

void GeneralPage::curveSmoothingChange(int value)
{
    mManager->set(SETTING::CURVE_SMOOTHING, value);
}

void GeneralPage::highResCheckboxStateChanged( int b )
{
    mManager->set( SETTING::HIGH_RESOLUTION, b != Qt::Unchecked );
}

void GeneralPage::shadowsCheckboxStateChanged( int b )
{
    mManager->set( SETTING::SHADOW, b != Qt::Unchecked );
}

void GeneralPage::antiAliasCheckboxStateChanged( int b )
{
    mManager->set( SETTING::ANTIALIAS, b != Qt::Unchecked );
}

void GeneralPage::toolCursorsCheckboxStateChanged(int b)
{
    mManager->set( SETTING::TOOL_CURSOR, b != Qt::Unchecked );
}

void GeneralPage::dottedCursorCheckboxStateChanged(int b)
{
    mManager->set( SETTING::DOTTED_CURSOR, b != Qt::Unchecked );
}

void GeneralPage::gridSizeChange(int value)
{
    mManager->set(SETTING::GRID_SIZE, value);
}

void GeneralPage::gridCheckBoxStateChanged(int b)
{
    mManager->set(SETTING::GRID, b != Qt::Unchecked);
}

TimelinePage::TimelinePage(QWidget* parent) : QWidget(parent)
{
    QSettings settings( PENCIL2D, PENCIL2D );

    QVBoxLayout* lay = new QVBoxLayout();

    QGroupBox* timeLineBox = new QGroupBox(tr("Timeline"));
    mDrawLabel = new QCheckBox(tr("Draw timeline labels"));
    mFontSize = new QSpinBox();
    QLabel* frameSizeLabel = new QLabel(tr("Frame size"));
    mFrameSize = new QSlider(Qt::Horizontal, this);
    QLabel* lengthSizeLabel = new QLabel(tr("Timeline size in Frames"));
    mLengthSize = new QLineEdit(this);
    QIntValidator* lengthSizeValidator = new QIntValidator(this);
    lengthSizeValidator->setBottom(2);
    mLengthSize->setValidator( lengthSizeValidator );

    mScrubBox = new QCheckBox(tr("Short scrub"));

    mFontSize->setRange(4, 20);
    mFrameSize->setRange(4, 20);

    mFontSize->setFixedWidth(50);
    mLengthSize->setFixedWidth(50);


    mFrameSize->setValue(settings.value("frameSize").toInt());
    if (settings.value("labelFontSize").toInt()==0) mFontSize->setValue(12);
    if (settings.value("frameSize").toInt()==0) mFrameSize->setValue(6);
    mLengthSize->setText(settings.value("length").toString());
    if (settings.value("length").toInt()==0) mLengthSize->setText("240");

    connect(mFontSize, SIGNAL(valueChanged(int)), this, SLOT(fontSizeChange(int)));
    connect(mFrameSize, SIGNAL(valueChanged(int)), this, SLOT(frameSizeChange(int)));
    connect(mLengthSize, SIGNAL(textChanged(QString)), this, SLOT(lengthSizeChange(QString)));
    connect( mDrawLabel, &QCheckBox::stateChanged, this, &TimelinePage::labelChange );
    connect( mScrubBox, &QCheckBox::stateChanged, this, &TimelinePage::scrubChange );

    lay->addWidget(frameSizeLabel);
    lay->addWidget(mFrameSize);
    lay->addWidget(lengthSizeLabel);
    lay->addWidget(mLengthSize);
    lay->addWidget(mScrubBox);
    timeLineBox->setLayout(lay);

    QVBoxLayout* lay2 = new QVBoxLayout();
    lay2->addWidget(timeLineBox);
    lay2->addStretch(1);
    setLayout(lay2);
}

void TimelinePage::updateValues()
{
    mScrubBox->setChecked(mManager->isOn(SETTING::SHORT_SCRUB));
    mDrawLabel->setChecked(mManager->isOn(SETTING::DRAW_LABEL));
    mFontSize->setValue(mManager->getInt(SETTING::LABEL_FONT_SIZE));
    mFrameSize->setValue(mManager->getInt(SETTING::FRAME_SIZE));
    mLengthSize->setText(mManager->getString(SETTING::TIMELINE_SIZE));
}

void TimelinePage::lengthSizeChange(QString value)
{
    int length = value.toInt();
    mManager->set(SETTING::TIMELINE_SIZE, length);
}

void TimelinePage::fontSizeChange(int value)
{
    mManager->set(SETTING::LABEL_FONT_SIZE, value);
}

void TimelinePage::frameSizeChange(int value)
{
    mManager->set(SETTING::FRAME_SIZE, value);
}

void TimelinePage::labelChange(bool value)
{
    mManager->set(SETTING::DRAW_LABEL, value);
}

void TimelinePage::scrubChange(bool value)
{
    mManager->set(SETTING::SHORT_SCRUB, value);
}

FilesPage::FilesPage(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout();

    QVBoxLayout *clearRecentChangesLay = new QVBoxLayout();

	QGroupBox *autosaveBox = new QGroupBox( tr( "Autosave documents", "Preference" ) );
    mAutosaveCheckBox = new QCheckBox(tr("Enable autosave", "Preference" ));
	QLabel *autosaveNumberLabel = new QLabel( tr( "Number of modifications before autosaving:", "Preference" ) );
    mAutosaveNumberBox = new QSpinBox();

    QGroupBox *clearRecentFilesBox = new QGroupBox(tr("Clear recent files list", "Clear Recent Files (Preference)" ));
    QLabel *clearRecentFilesLbl = new QLabel(tr("This will clear your list of recently opened files", "Clear Recent Files (Preference)" ));
	mClearRecentFilesBtn = new QPushButton( tr( "Clear", "Clear Recent Files (Preference)" ) );

    mAutosaveNumberBox = new QSpinBox();

    mAutosaveNumberBox->setMinimum(5);
    mAutosaveNumberBox->setMaximum(200);
    mAutosaveNumberBox->setFixedWidth(50);

    connect(mAutosaveCheckBox, &QCheckBox::stateChanged, this, &FilesPage::autosaveChange);
    connect(mAutosaveNumberBox, SIGNAL(valueChanged(int)), this, SLOT(autosaveNumberChange(int)));
    connect(mClearRecentFilesBtn, SIGNAL(clicked(bool)), this, SLOT(clearRecentFilesList()));

    lay->addWidget(mAutosaveCheckBox);
    lay->addWidget(autosaveNumberLabel);
    lay->addWidget(mAutosaveNumberBox);
    autosaveBox->setLayout(lay);

    clearRecentChangesLay->addWidget(clearRecentFilesLbl);
    clearRecentChangesLay->addWidget(mClearRecentFilesBtn);
    clearRecentFilesBox->setLayout(clearRecentChangesLay);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addWidget(autosaveBox);
    mainLayout->addWidget(clearRecentFilesBox);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void FilesPage::updateValues()
{
    mAutosaveCheckBox->setChecked(mManager->isOn(SETTING::AUTO_SAVE));
    mAutosaveNumberBox->setValue(mManager->getInt(SETTING::AUTO_SAVE_NUMBER));
}

void FilesPage::updateClearRecentListButton()
{
    mClearRecentFilesBtn->setEnabled(false);
    mClearRecentFilesBtn->setText("List is empty");
}

void FilesPage::autosaveChange(bool b)
{
    mManager->set(SETTING::AUTO_SAVE, b);
}

void FilesPage::autosaveNumberChange(int number)
{
    mManager->set(SETTING::AUTO_SAVE_NUMBER, number);
}

void FilesPage::clearRecentFilesList()
{
    emit clearRecentList();
}

ToolsPage::ToolsPage(QWidget* parent) : QWidget(parent)
{
    QSettings settings( PENCIL2D, PENCIL2D );

    QVBoxLayout* lay = new QVBoxLayout();

    QGroupBox* onionSkinBox = new QGroupBox(tr("Onion skin"));

    QLabel* onionMaxOpacityLabel = new QLabel(tr("Maximum onion opacity %"));
    mOnionMaxOpacityBox = new QSpinBox();
    QLabel* onionMinOpacityLabel = new QLabel(tr("Minimum onion opacity %"));
    mOnionMinOpacityBox = new QSpinBox();
    QLabel* onionPrevFramesNumLabel = new QLabel(tr("Number of previous onion frames shown"));
    mOnionPrevFramesNumBox = new QSpinBox();
    QLabel* onionNextFramesNumLabel = new QLabel(tr("Number of next onion frames shown"));
    mOnionNextFramesNumBox = new QSpinBox();

    mOnionMaxOpacityBox->setMinimum(0);
    mOnionMaxOpacityBox->setMaximum(100);
    mOnionMaxOpacityBox->setFixedWidth(50);
    mOnionMinOpacityBox->setMinimum(0);
    mOnionMinOpacityBox->setMaximum(100);
    mOnionMinOpacityBox->setFixedWidth(50);
    mOnionPrevFramesNumBox->setMinimum(1);
    mOnionPrevFramesNumBox->setMaximum(60);
    mOnionPrevFramesNumBox->setFixedWidth(50);
    mOnionNextFramesNumBox->setMinimum(1);
    mOnionNextFramesNumBox->setMaximum(60);
    mOnionNextFramesNumBox->setFixedWidth(50);

    mOnionMaxOpacityBox->setValue(settings.value( SETTING_ONION_MAX_OPACITY ).toInt());
    mOnionMinOpacityBox->setValue(settings.value( SETTING_ONION_MIN_OPACITY ).toInt());
    mOnionPrevFramesNumBox->setValue(settings.value( SETTING_ONION_PREV_FRAMES_NUM).toInt());
    mOnionNextFramesNumBox->setValue(settings.value( SETTING_ONION_NEXT_FRAMES_NUM ).toInt());

    connect(mOnionMaxOpacityBox, SIGNAL(valueChanged(int)), this, SLOT(onionMaxOpacityChange(int)));
    connect(mOnionMinOpacityBox, SIGNAL(valueChanged(int)), this, SLOT(onionMinOpacityChange(int)));
    connect(mOnionPrevFramesNumBox, SIGNAL(valueChanged(int)), this, SLOT(onionPrevFramesNumChange(int)));
    connect(mOnionNextFramesNumBox, SIGNAL(valueChanged(int)), this, SLOT(onionNextFramesNumChange(int)));

    lay->addWidget(onionMaxOpacityLabel);
    lay->addWidget(mOnionMaxOpacityBox);
    lay->addWidget(onionMinOpacityLabel);
    lay->addWidget(mOnionMinOpacityBox);
    lay->addWidget(onionPrevFramesNumLabel);
    lay->addWidget(mOnionPrevFramesNumBox);
    lay->addWidget(onionNextFramesNumLabel);
    lay->addWidget(mOnionNextFramesNumBox);
    onionSkinBox->setLayout(lay);

    QGroupBox* brushBox = new QGroupBox(tr("Brush Tools"));
    mUseQuickSizingBox = new QCheckBox(tr("Use Quick Sizing"));
    QVBoxLayout* brushBoxLayout = new QVBoxLayout();
    brushBoxLayout->addWidget(mUseQuickSizingBox);

    connect( mUseQuickSizingBox, &QCheckBox::stateChanged, this, &ToolsPage::quickSizingChange );

    brushBox->setLayout(brushBoxLayout);


    QVBoxLayout* lay2 = new QVBoxLayout();
    lay2->addWidget(onionSkinBox);
    lay2->addWidget(brushBox);
    lay2->addStretch(1);
    setLayout(lay2);
}

void ToolsPage::updateValues()
{
    mOnionMaxOpacityBox->setValue(mManager->getInt(SETTING::ONION_MAX_OPACITY));
    mOnionMinOpacityBox->setValue(mManager->getInt(SETTING::ONION_MIN_OPACITY));
    mOnionPrevFramesNumBox->setValue(mManager->getInt(SETTING::ONION_PREV_FRAMES_NUM));
    mOnionNextFramesNumBox->setValue(mManager->getInt(SETTING::ONION_NEXT_FRAMES_NUM));
    mUseQuickSizingBox->setChecked(mManager->isOn(SETTING::QUICK_SIZING));
}

void ToolsPage::onionMaxOpacityChange(int value)
{
    mManager->set(SETTING::ONION_MAX_OPACITY, value);
}

void ToolsPage::quickSizingChange( bool b )
{
    mManager->set(SETTING::QUICK_SIZING, b);
}

void ToolsPage::onionMinOpacityChange(int value)
{
    mManager->set(SETTING::ONION_MIN_OPACITY, value);
}

void ToolsPage::onionPrevFramesNumChange(int value)
{
    mManager->set(SETTING::ONION_PREV_FRAMES_NUM, value);
}

void ToolsPage::onionNextFramesNumChange(int value)
{
    mManager->set(SETTING::ONION_NEXT_FRAMES_NUM, value);
}
