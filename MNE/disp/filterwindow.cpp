//=============================================================================================================
/**
* @file     filterwindow.cpp
* @author   Lorenz Esch <Lorenz.Esch@tu-ilmenau.de>;
*           Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     August, 2014
*
* @section  LICENSE
*
* Copyright (C) 2014, Lorenz Esch, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of MNE-CPP authors nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Contains the implementation of the FilterWindow class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "ui_filterwindowwidget.h"
#include "filterwindow.h"


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace DISPLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FilterWindow::FilterWindow(QWidget *parent)
: QWidget(parent,Qt::Window)
, ui(new Ui::FilterWindowWidget)
, m_pFilterPlotScene(new FilterPlotScene)
{
    ui->setupUi(this);

    initCheckBoxes();
    initSpinBoxes();
    initButtons();
    initComboBoxes();
    initFilterPlot();
    initMVC();
    initFilters();

    m_iWindowSize = 4016;
    m_iFilterTaps = 128;
}


//*************************************************************************************************************

FilterWindow::~FilterWindow()
{
    delete ui;
}


//*************************************************************************************************************

void FilterWindow::setFiffInfo(const FiffInfo &fiffInfo)
{
    m_fiffInfo = fiffInfo;

    filterParametersChanged();

    //Init m_filterData with designed filter and add to model
    m_pFilterDataModel->addFilter(m_filterData);

    //Update min max of spin boxes to nyquist
    double samplingFrequency = m_fiffInfo.sfreq;
    double nyquistFrequency = samplingFrequency/2;

    ui->m_doubleSpinBox_highpass->setMaximum(nyquistFrequency);
    ui->m_doubleSpinBox_lowpass->setMaximum(nyquistFrequency);

    updateFilterPlot();
}


//*************************************************************************************************************

void FilterWindow::setWindowSize(int iWindowSize)
{
    m_iWindowSize = iWindowSize;

    ui->m_spinBox_filterTaps->setMaximum(iWindowSize);
}


//*************************************************************************************************************

QList<FilterData> FilterWindow::getCurrentFilter()
{
    //Get active filters
    QList<FilterData> activeFilters = m_pFilterDataModel->data( m_pFilterDataModel->index(0,8), FilterDataModelRoles::GetActiveFilters).value<QList<FilterData>>();

    return activeFilters;
}


//*************************************************************************************************************

void FilterWindow::initCheckBoxes()
{
    connect(ui->m_checkBox_activateFilter,static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
                this,&FilterWindow::onBtnActivateFilter);
}


//*************************************************************************************************************

void FilterWindow::initSpinBoxes()
{
    connect(ui->m_doubleSpinBox_lowpass,static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this,&FilterWindow::filterParametersChanged);

    connect(ui->m_doubleSpinBox_highpass,static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this,&FilterWindow::filterParametersChanged);

    connect(ui->m_doubleSpinBox_transitionband,static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this,&FilterWindow::filterParametersChanged);

    connect(ui->m_spinBox_filterTaps,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this,&FilterWindow::filterParametersChanged);

    //Intercept events from the spin boxes to get control over key events
    ui->m_doubleSpinBox_lowpass->installEventFilter(this);
    ui->m_doubleSpinBox_highpass->installEventFilter(this);
    ui->m_doubleSpinBox_transitionband->installEventFilter(this);
}


//*************************************************************************************************************

void FilterWindow::initButtons()
{
    connect(ui->m_pushButton_exportPlot,&QPushButton::released,
                this,&FilterWindow::onBtnExportFilterPlot);

    connect(ui->m_pushButton_exportFilter,&QPushButton::released,
                this,&FilterWindow::onBtnExportFilterCoefficients);

    connect(ui->m_pushButton_loadFilter,&QPushButton::released,
                this,&FilterWindow::onBtnLoadFilter);
}


//*************************************************************************************************************

void FilterWindow::initComboBoxes()
{
    connect(ui->m_comboBox_designMethod,static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,&FilterWindow::changeStateSpinBoxes);

    connect(ui->m_comboBox_filterType,static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,&FilterWindow::changeStateSpinBoxes);

    //Initial selection is a lowpass and Cosine design method
    ui->m_doubleSpinBox_lowpass->setVisible(true);
    ui->m_label_lowpass->setVisible(true);
    ui->m_label_lowpass->setText("Highpass (Hz):");

    ui->m_doubleSpinBox_highpass->setVisible(false);
    ui->m_label_highpass->setVisible(false);
    ui->m_doubleSpinBox_highpass->setEnabled(false);

    ui->m_spinBox_filterTaps->setVisible(true);
    ui->m_label_filterTaps->setVisible(true);

    connect(ui->m_comboBox_filterApplyTo, &QComboBox::currentTextChanged,
            this, &FilterWindow::onSpinBoxFilterChannelType);
}


//*************************************************************************************************************

void FilterWindow::initFilterPlot()
{
    ui->m_graphicsView_filterPlot->setScene(m_pFilterPlotScene);
}


//*************************************************************************************************************

void FilterWindow::initMVC()
{
    m_pFilterDataModel = FilterDataModel::SPtr(new FilterDataModel(this));
    m_pFilterDataDelegate = FilterDataDelegate::SPtr(new FilterDataDelegate(this));

    ui->m_tableView_filterDataView->setModel(m_pFilterDataModel.data());
    ui->m_tableView_filterDataView->setItemDelegate(m_pFilterDataDelegate.data());

    ui->m_tableView_filterDataView->resizeColumnToContents(0);

    //Only show the names of the filter and activity check boxes
    ui->m_tableView_filterDataView->verticalHeader()->hide();
    ui->m_tableView_filterDataView->hideColumn(2);
    ui->m_tableView_filterDataView->hideColumn(3);
    ui->m_tableView_filterDataView->hideColumn(4);
    ui->m_tableView_filterDataView->hideColumn(5);
    ui->m_tableView_filterDataView->hideColumn(6);
    ui->m_tableView_filterDataView->hideColumn(7);
    ui->m_tableView_filterDataView->hideColumn(8);

    //Connect selection in event window to jumpEvent slot
    connect(ui->m_tableView_filterDataView->selectionModel(),&QItemSelectionModel::currentRowChanged,
                this, &FilterWindow::filterSelectionChanged);
}


//*************************************************************************************************************

void FilterWindow::initFilters()
{
    //Init filter data model with all default filters located in the resource directory
    QStringList defaultFilters;

    defaultFilters << "NOTCH_60Hz.txt"
                   << "NOTCH_50Hz.txt";

    for(int i = 0; i<defaultFilters.size(); i++) {
        FilterData tmpFilter;
        QString fileName = defaultFilters.at(i);
        QString path = QCoreApplication::applicationDirPath() + fileName.prepend("/mne_x_libs/xDisp/default_filters/");

        if(FilterIO::readFilter(path, tmpFilter))
            m_pFilterDataModel->addFilter(tmpFilter);
    }
}


//*************************************************************************************************************

void FilterWindow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    ui->m_graphicsView_filterPlot->fitInView(m_pFilterPlotScene->itemsBoundingRect(), Qt::KeepAspectRatio);
}


//*************************************************************************************************************

void FilterWindow::keyPressEvent(QKeyEvent * event)
{
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        emit applyFilter(ui->m_comboBox_filterApplyTo->currentText());

    if((event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Z) || event->key() == Qt::Key_Delete)
        emit applyFilter(ui->m_comboBox_filterApplyTo->currentText());
}


//*************************************************************************************************************

bool FilterWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == ui->m_doubleSpinBox_highpass || obj == ui->m_doubleSpinBox_lowpass || obj == ui->m_doubleSpinBox_transitionband) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            if((keyEvent->modifiers() == Qt::ControlModifier && keyEvent->key() == Qt::Key_Z)/* || keyEvent->key() == Qt::Key_Delete*/)
                emit applyFilter(ui->m_comboBox_filterApplyTo->currentText());
            else // standard event processing
                return QObject::eventFilter(obj, event);

            return true;
        } else {
            // standard event processing
            return QObject::eventFilter(obj, event);
        }
    }

    return true;
}


//*************************************************************************************************************

void FilterWindow::updateFilterPlot()
{
    //Update the filter of the scene
    m_pFilterPlotScene->updateFilter(m_filterData,
                                     m_filterData.m_sFreq,//m_fiffInfo.sfreq,
                                     ui->m_doubleSpinBox_lowpass->value(),
                                     ui->m_doubleSpinBox_highpass->value());

    ui->m_graphicsView_filterPlot->fitInView(m_pFilterPlotScene->itemsBoundingRect(), Qt::KeepAspectRatio);
}


//*************************************************************************************************************

void FilterWindow::changeStateSpinBoxes(int currentIndex)
{
    Q_UNUSED(currentIndex);

    //Change visibility of filter tap spin boxes depending on filter design method
    switch(ui->m_comboBox_designMethod->currentIndex()) {
        case 0: //Cosine
//            ui->m_spinBox_filterTaps->setVisible(false);
//            ui->m_label_filterTaps->setVisible(false);
            ui->m_spinBox_filterTaps->setVisible(true);
            ui->m_label_filterTaps->setVisible(true);
            break;

        case 1: //Tschebyscheff
            ui->m_spinBox_filterTaps->setVisible(true);
            ui->m_label_filterTaps->setVisible(true);
            break;
    }

    //Change visibility of spin boxes depending on filter type
    switch(ui->m_comboBox_filterType->currentIndex()) {
        case 0: //Lowpass
            ui->m_doubleSpinBox_lowpass->setVisible(true);
            ui->m_label_lowpass->setVisible(true);
            ui->m_label_lowpass->setText("Highpass (Hz):");

            ui->m_doubleSpinBox_highpass->setVisible(false);
            ui->m_label_highpass->setVisible(false);
            ui->m_doubleSpinBox_highpass->setEnabled(false);
            break;

        case 1: //Highpass
            ui->m_doubleSpinBox_highpass->setVisible(true);
            ui->m_label_highpass->setVisible(true);
            ui->m_label_highpass->setText("Lowpass (Hz):");

            ui->m_doubleSpinBox_lowpass->setVisible(false);
            ui->m_label_lowpass->setVisible(false);
            ui->m_doubleSpinBox_highpass->setEnabled(true);
            break;

        case 2: //Bandpass
            ui->m_doubleSpinBox_highpass->setVisible(true);
            ui->m_label_highpass->setVisible(true);
            ui->m_doubleSpinBox_lowpass->setVisible(true);
            ui->m_label_lowpass->setText("Highpass (Hz):");

            ui->m_label_lowpass->setVisible(true);
            ui->m_doubleSpinBox_lowpass->setEnabled(true);
            ui->m_doubleSpinBox_highpass->setEnabled(true);
            ui->m_label_highpass->setText("Lowpass (Hz):");
            break;
    }

    filterParametersChanged();
}


//*************************************************************************************************************

void FilterWindow::filterParametersChanged()
{
    //User defined filter parameters
    double lowpassHz = ui->m_doubleSpinBox_lowpass->value();
    double highpassHz = ui->m_doubleSpinBox_highpass->value();

    double trans_width = ui->m_doubleSpinBox_transitionband->value();

    double bw = highpassHz-lowpassHz;
    double center = lowpassHz+bw/2;

    double samplingFrequency = m_fiffInfo.sfreq <= 0 ? 600 : m_fiffInfo.sfreq;
    double nyquistFrequency = samplingFrequency/2;

    //Calculate the needed fft length
    int filterTaps = ui->m_spinBox_filterTaps->value();
    int fftLength = m_iWindowSize;
    int exp = ceil(MNEMath::log2(fftLength));
    fftLength = pow(2, exp+1);

    //set maximum and minimum for cut off frequency spin boxes
    if(ui->m_comboBox_filterType->currentText() == "Bandpass") {
        ui->m_doubleSpinBox_highpass->setMinimum(ui->m_doubleSpinBox_lowpass->value());
        ui->m_doubleSpinBox_lowpass->setMaximum(ui->m_doubleSpinBox_highpass->value());
    }
    else {
        ui->m_doubleSpinBox_highpass->setMaximum(nyquistFrequency);
        ui->m_doubleSpinBox_lowpass->setMaximum(nyquistFrequency);
    }

    //set filter design method
    FilterData::DesignMethod dMethod;
    if(ui->m_comboBox_designMethod->currentText() == "Tschebyscheff")
        dMethod = FilterData::Tschebyscheff;

    if(ui->m_comboBox_designMethod->currentText() == "Cosine")
        dMethod = FilterData::Cosine;

    //Generate filters
    QSharedPointer<FilterData> userDefinedFilterOperator;

    if(ui->m_comboBox_filterType->currentText() == "Lowpass") {
        userDefinedFilterOperator = QSharedPointer<FilterData>(
                                                new FilterData("Designed",
                                                               FilterData::LPF,
                                                               filterTaps,
                                                               lowpassHz/nyquistFrequency,
                                                               0.2,
                                                               (double)trans_width/nyquistFrequency,
                                                               samplingFrequency,
                                                               fftLength,
                                                               dMethod));
    }

    if(ui->m_comboBox_filterType->currentText() == "Highpass") {
        userDefinedFilterOperator = QSharedPointer<FilterData>(
                                        new FilterData("Designed",
                                            FilterData::HPF,
                                            filterTaps,
                                            highpassHz/nyquistFrequency,
                                            0.2,
                                            (double)trans_width/nyquistFrequency,
                                            samplingFrequency,
                                            fftLength,
                                            dMethod));
    }

    if(ui->m_comboBox_filterType->currentText() == "Bandpass") {
        userDefinedFilterOperator = QSharedPointer<FilterData>(
                   new FilterData("Designed",
                                  FilterData::BPF,
                                  filterTaps,
                                  (double)center/nyquistFrequency,
                                  (double)bw/nyquistFrequency,
                                  (double)trans_width/nyquistFrequency,
                                  samplingFrequency,
                                  fftLength,
                                  dMethod));
    }

    //Replace old with new filter operator
    m_filterData = *userDefinedFilterOperator.data();

    QList<FilterData> activeFilters = m_pFilterDataModel->data( m_pFilterDataModel->index(0,8), FilterDataModelRoles::GetActiveFilters).value<QList<FilterData>>();
    std::cout<<"activeFilters.size()"<<activeFilters.size()<<std::endl;

    emit filterChanged(activeFilters);

    //update filter plot
    updateFilterPlot();
}

//*************************************************************************************************************

void FilterWindow::onBtnActivateFilter()
{
    //Undo all previous filters
    emit activateFilter(ui->m_checkBox_activateFilter->isChecked());
}


//*************************************************************************************************************

void FilterWindow::onSpinBoxFilterChannelType(QString channelType)
{
    //Apply filter
    emit applyFilter(channelType);
}


//*************************************************************************************************************

void FilterWindow::onBtnExportFilterPlot()
{
    // Open file dialog
    QDate date;
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save filter plot",
                                                    QString("%1/%2_%3_%4_FilterPlot").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(date.currentDate().year()).arg(date.currentDate().month()).arg(date.currentDate().day()),
                                                    tr("Vector graphic(*.svg);;Images (*.png)"));

    if(!fileName.isEmpty())
    {
        // Generate screenshot
        if(fileName.contains(".svg"))
        {
            QSvgGenerator svgGen;

            svgGen.setFileName(fileName);
            QRectF rect = m_pFilterPlotScene->itemsBoundingRect();
            svgGen.setSize(QSize(rect.width(), rect.height()));
            //svgGen.setViewBox(QRect(0, 0, rect.width(), rect.height()));

            QPainter painter(&svgGen);
            m_pFilterPlotScene->render(&painter);
        }

        if(fileName.contains(".png"))
        {
            m_pFilterPlotScene->setSceneRect(m_pFilterPlotScene->itemsBoundingRect());                  // Re-shrink the scene to it's bounding contents
            QImage image(m_pFilterPlotScene->sceneRect().size().toSize(), QImage::Format_ARGB32);       // Create the image with the exact size of the shrunk scene
            image.fill(Qt::transparent);                                                                // Start all pixels transparent

            QPainter painter(&image);
            m_pFilterPlotScene->render(&painter);
            image.save(fileName);
        }
    }
}


//*************************************************************************************************************

void FilterWindow::onBtnExportFilterCoefficients()
{
    QDate date;
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save filter coefficients",
                                                    QString("%1/%2_%3_%4_FilterCoeffs").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(date.currentDate().year()).arg(date.currentDate().month()).arg(date.currentDate().day()),
                                                    tr("Text file(*.txt)"));

    FilterIO::writeFilter(fileName, m_filterData);
}


//*************************************************************************************************************

void FilterWindow::onBtnLoadFilter()
{
    QString path = QFileDialog::getOpenFileName(this,
                                                QString("Load filter"),
                                                QString("./"),
                                                tr("txt files (*.txt)"));

    if(!path.isEmpty()) {
        //Replace old with new filter operator
        FilterIO::readFilter(path, m_filterData);

        m_pFilterDataModel->addFilter(m_filterData);

        QList<FilterData> activeFilters = m_pFilterDataModel->data( m_pFilterDataModel->index(0,8), FilterDataModelRoles::GetActiveFilters).value<QList<FilterData>>();
        emit filterChanged(activeFilters);

        updateFilterPlot();
    }
    else
        qDebug()<<"Could not load filter.";
}


//*************************************************************************************************************

void FilterWindow::filterSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    //Get filter from model and set as current filter
    QModelIndex index = m_pFilterDataModel->index(current.row(), 7);

    m_filterData = m_pFilterDataModel->data(index, FilterDataModelRoles::GetFilter).value<FilterData>();

    QList<FilterData> activeFilters = m_pFilterDataModel->data( m_pFilterDataModel->index(0,8), FilterDataModelRoles::GetActiveFilters).value<QList<FilterData>>();
    emit filterChanged(activeFilters);

    updateFilterPlot();
}


