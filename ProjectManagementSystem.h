#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ProjectManagementSystem.h"

class ProjectManagementSystem : public QMainWindow
{
    Q_OBJECT

public:
    ProjectManagementSystem(QWidget *parent = nullptr);
    ~ProjectManagementSystem();

private:
    Ui::ProjectManagementSystemClass ui;
};

