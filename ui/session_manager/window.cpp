#include "ui/session_manager/window.h"
#include "app.h"

namespace meow {
namespace ui {
namespace session_manager {

Window::Window(QWidget *parent)
:QDialog(parent),
_saveConfirmCanceled(false)
{

    setMinimumSize(QSize(665, 400));
    setWindowTitle(tr("Session manager"));

    createMainLayout();
    createLeftSubWidgets();
    createRightSubWidgets();

    resize(QSize(750, 364));

    connect(_connectionParamsModel.data(),
            &meow::models::db::ConnectionParamsModel::selectedFormDataModified,
            [=] {
                validateControls();
            });

    connect(_connectionParamsModel.data(),
            &meow::models::db::ConnectionParamsModel::settingEmptySessionName,
            [=] {
                 showErrorMessage(tr("Session name can't be empty"));
            });
    connect(_connectionParamsModel.data(),
            &meow::models::db::ConnectionParamsModel::settingTakenSessionName,
            [=](const QString &newName) {
                showErrorMessage(tr("Session \"%1\" already exists!").arg(newName));
                // TODO: rm ! from the message above
            });
}

void Window::createMainLayout()
{

    _mainLayout = new QHBoxLayout();
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(_mainLayout);

    _mainSplitter = new QSplitter(this);
    _mainSplitter->setChildrenCollapsible(false); // don't like when true
    _mainLayout->addWidget(_mainSplitter);

    _mainLeftWidget = new QWidget();
    _mainLeftWidget->setMinimumSize(QSize(240, 400));

    _mainRightWidget = new QWidget();
    _mainRightWidget->setMinimumSize(QSize(350, 400));

    _mainSplitter->addWidget(_mainLeftWidget);
    _mainSplitter->setStretchFactor(0, 1);
    _mainSplitter->addWidget(_mainRightWidget);
    _mainSplitter->setStretchFactor(1, 2);

}

void Window::createLeftSubWidgets()
{
    _leftWidgetMainLayout = new QVBoxLayout();
    _mainLeftWidget->setLayout(_leftWidgetMainLayout);

    createSessionsList();
    createLeftWidgetButtons();

}

void Window::createSessionsList()
{



    _connectionParamsModel.reset(
        new meow::models::db::ConnectionParamsModel(
            meow::app()->dbConnectionManager()
        )
    );

    _sessionsList = new QTableView();
    _sessionsList->verticalHeader()->hide();
    _sessionsList->horizontalHeader()->setHighlightSections(false);

    //_proxySortModel.setSourceModel(_connectionParamsModel.data());
    //_sessionsList->setModel(&_proxySortModel);
    _sessionsList->setModel(_connectionParamsModel.data());
    _leftWidgetMainLayout->addWidget(_sessionsList);
    _sessionsList->setSortingEnabled(false); // TODO
    _sessionsList->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    _sessionsList->setSelectionMode(QAbstractItemView::SingleSelection);
    // TODO
    //proxyModel->sort(0, Qt::AscendingOrder);

    for (int i=0; i<_connectionParamsModel->columnCount(); ++i) {
        _sessionsList->setColumnWidth(i, _connectionParamsModel->columnWidth(i));
    }

    connect(_sessionsList->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &Window::currentSessionDetailsChanged
    );
}

void Window::createLeftWidgetButtons()
{
    _leftWidgetButtonsLayout = new QHBoxLayout();
    _leftWidgetMainLayout->addLayout(_leftWidgetButtonsLayout);

    _newSessionButton = new QPushButton(tr("New"));
    _leftWidgetButtonsLayout->addWidget(_newSessionButton);
    connect(_newSessionButton,
            &QAbstractButton::clicked,
            this,
            &Window::createNewSession
    );

    _saveSessionButton = new QPushButton(tr("Save"));
    _leftWidgetButtonsLayout->addWidget(_saveSessionButton);
    connect(_saveSessionButton,
            &QAbstractButton::clicked,
            this,
            &Window::saveCurrentSession
    );

    _deleteSessionButton = new QPushButton(tr("Delete"));
    _leftWidgetButtonsLayout->addWidget(_deleteSessionButton);
    connect(_deleteSessionButton,
            &QAbstractButton::clicked,
            this,
            &Window::deleteCurrentSession
    );

}

void Window::createRightSubWidgets()
{
    _rightWidgetMainLayout = new QVBoxLayout();
    _mainRightWidget->setLayout(_rightWidgetMainLayout);

    _sessionDetails = new SessionForm();
    _rightWidgetMainLayout->addWidget(_sessionDetails);

    createRightWidgetButtons();
}

void Window::createRightWidgetButtons()
{
    _rightWidgetButtonsLayout = new QHBoxLayout();
    _rightWidgetMainLayout->addLayout(_rightWidgetButtonsLayout);

    _rightWidgetButtonsLayout->addStretch(1);

    _openSessionButton = new QPushButton(tr("Open"));
    _rightWidgetButtonsLayout->addWidget(_openSessionButton);

    _cancelButton = new QPushButton(tr("Cancel"));
    _rightWidgetButtonsLayout->addWidget(_cancelButton);

    _moreButton = new QPushButton(tr("More"));
    _rightWidgetButtonsLayout->addWidget(_moreButton);

}

// returns true if can proceed
bool Window::finalizeCurModifications()
{
    bool leftEditedRow = _connectionParamsModel->isSelectedConnectionParamModified();

    if (leftEditedRow) {

        QString curSessionName("Unknown");

        meow::models::forms::ConnectionParametersForm * curForm = _connectionParamsModel->selectedForm();
        if (curForm) {
            curSessionName = curForm->sessionName();
        }

        QMessageBox msgBox;
        msgBox.setText("Save modifications?");
        msgBox.setInformativeText(tr("Settings for \"%1\" were changed.").arg(curSessionName));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setIcon(QMessageBox::Question);
        int ret = msgBox.exec();

        switch (ret) {
          case QMessageBox::Yes:
              saveCurrentSession();
              return true;
          case QMessageBox::No:
              return true;
          case QMessageBox::Cancel:
              return false;
          default:
              return true;
        }

    }

    return true;
}

void Window::currentSessionDetailsChanged(
    const QItemSelection &selected,
    const QItemSelection &deselected)
{

    qDebug() << "currentSessionDetailsChanged";

    if (_saveConfirmCanceled) {
        _saveConfirmCanceled = false;
        return;
    }

    if (finalizeCurModifications() == false) {

        // http://www.qtcentre.org/threads/19754-QItemSelectionModel-currentChanged-and-revert-change

        QModelIndex deselectedIndex;
        QModelIndexList deselectedItems = deselected.indexes();
        if (!deselectedItems.isEmpty()) {
            deselectedIndex = deselectedItems.at(0);
            _saveConfirmCanceled = true;;
            _sessionsList->setCurrentIndex(deselectedIndex);
            return;
        }
    }

    QModelIndex index;
    QModelIndexList items = selected.indexes();

    if (!items.isEmpty()) {
        index = items.at(0);

        //QModelIndex realIndex = _proxySortModel.mapToSource(index);

        _sessionDetails->setConnectionParamsForm(
            _connectionParamsModel->selectFormAt(index.row())
        );
    } else {
        qDebug() << "Selection is empty";
        _sessionDetails->setConnectionParamsForm(nullptr);
    }

    validateControls();
}

void Window::validateControls()
{
    // TODO: session form controls

    meow::models::forms::ConnectionParametersForm * curForm = _connectionParamsModel->selectedForm();
    bool hasCurForm = curForm != nullptr;
    bool curModified = _connectionParamsModel->isSelectedConnectionParamModified();

    _openSessionButton->setDisabled(!hasCurForm);
    _saveSessionButton->setDisabled(!(curModified && hasCurForm));
    _deleteSessionButton->setDisabled(!hasCurForm);
    // TODO menus enablement
}

void Window::saveCurrentSession()
{
    _connectionParamsModel->saveSelectedConnectionParam();
    validateControls();
}

void Window::createNewSession()
{
    //qDebug() << "createNewSession";

    if (finalizeCurModifications() == false) {
        return;
    }

    meow::models::forms::ConnectionParametersForm * newForm = _connectionParamsModel->createNewForm();

    selectSessionAt(newForm->index());

    // scroll to just added (== selected)
    QModelIndexList selectedIndexes =  _sessionsList->selectionModel()->selectedIndexes();
    if (selectedIndexes.size() == 0) {
        return;
    }
    QModelIndex index = selectedIndexes.at(0);
    _sessionsList->scrollTo(index);

    _sessionsList->edit(_connectionParamsModel->indexForSessionNameAt(index.row()));
}



void Window::deleteCurrentSession()
{
    // BUG: removing of just added row loses focus if off screen

    //qDebug() << "\n\ndeleteCurrentSession()";

    meow::models::forms::ConnectionParametersForm * curForm = _connectionParamsModel->selectedForm();

    if (!curForm) {
        return;
    }

    QModelIndexList selectedIndexes =  _sessionsList->selectionModel()->selectedIndexes();

    if (selectedIndexes.size() == 0) {
        return;
    }

    QModelIndex index = selectedIndexes.at(0);

    if (!index.isValid()) {
        return;
    }

    QString curSessionName("Unknown");

    if (curForm) {
        curSessionName = curForm->sessionName();
    } else {
        qDebug() << "no curForm";
        return;
    }

    QMessageBox msgBox;
    msgBox.setText(tr("Delete session \"%1\"?").arg(curSessionName));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Question);
    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes) {
        return;
    }

    // map model's deletion index to UI
    //QModelIndex removedIndexModel =_connectionParamsModel->index(curForm->index(), 0) ;
    //QModelIndex removedIndexUI = _proxySortModel.mapFromSource(removedIndexModel);

    _connectionParamsModel->deleteSelectedFormAt(index);

    _sessionsList->setFocus();

    return;
}

void Window::selectSessionAt(int rowIndex)
{

    //QModelIndex newSelectedIndex =_connectionParamsModel->index(rowIndex, 0) ;
    //QModelIndex mappedNewIndex = _proxySortModel.mapFromSource(newSelectedIndex);

    _sessionsList->selectionModel()->select(_connectionParamsModel->indexForSessionNameAt(rowIndex),
                                            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    //_sessionsList->setFocus();
}

void Window::showErrorMessage(const QString& message)
{
    QMessageBox msgBox;
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.exec();
}

} // namespace session_manager
} // namespace ui
} // namespace