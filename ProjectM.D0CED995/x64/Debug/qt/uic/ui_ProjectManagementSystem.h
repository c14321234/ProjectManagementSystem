/********************************************************************************
** Form generated from reading UI file 'ProjectManagementSystem.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PROJECTMANAGEMENTSYSTEM_H
#define UI_PROJECTMANAGEMENTSYSTEM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ProjectManagementSystemClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *ProjectManagementSystemClass)
    {
        if (ProjectManagementSystemClass->objectName().isEmpty())
            ProjectManagementSystemClass->setObjectName(QString::fromUtf8("ProjectManagementSystemClass"));
        ProjectManagementSystemClass->resize(600, 400);
        menuBar = new QMenuBar(ProjectManagementSystemClass);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        ProjectManagementSystemClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(ProjectManagementSystemClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        ProjectManagementSystemClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(ProjectManagementSystemClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        ProjectManagementSystemClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(ProjectManagementSystemClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        ProjectManagementSystemClass->setStatusBar(statusBar);

        retranslateUi(ProjectManagementSystemClass);

        QMetaObject::connectSlotsByName(ProjectManagementSystemClass);
    } // setupUi

    void retranslateUi(QMainWindow *ProjectManagementSystemClass)
    {
        ProjectManagementSystemClass->setWindowTitle(QApplication::translate("ProjectManagementSystemClass", "ProjectManagementSystem", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ProjectManagementSystemClass: public Ui_ProjectManagementSystemClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROJECTMANAGEMENTSYSTEM_H
