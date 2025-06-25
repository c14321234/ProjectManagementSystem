#pragma execution_character_set("utf-8")
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <mysql.h>
#include <sstream>
#include <vector>
#include <string>
#include <qinputdialog.h>
#include <map>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QtCore/Qt>
#include <QDesktopServices>
#include <QListWidget>
#include <QDialog>
#include <iomanip>
#include <QCoreApplication>
#include <algorithm>
#include <errmsg.h>
#include <QProcess>
#include <QStandardPaths>
#include <QGridLayout>

using namespace std;

// 数据库连接配置，根据实际情况修改
#define HOST "127.0.0.1"
#define USER "root"
#define PASS "root"
#define DATABASE "projectManagement"
#define PORT 3306

MYSQL* conn;
MYSQL_RES* res_set;
MYSQL_ROW row;
stringstream stmt;
string query;

// 文档版本信息结构体（对应document表字段）
struct VersionInfo {
    int docId;           // 文档ID
    QString docName;     // 文档名称
    int versionNumber;   // 版本号
    string modifyTime;   // 修改时间（可选）
    string modifier;     // 修改者（可选）
    int projectId;       // 项目ID
    QString filePath;    // 文件路径
};

// 评分信息结构体（移除taskId，使用projectId）
struct ScoreInfo {
    int scoreId;      // 新增评分记录ID
    int userId;       // user_id
    int projectId;    // 项目ID，替代原taskId
    double score;     // score
    string createTime;// create_time，自动生成
};

//平均分结构体
struct AverageScore {
    double sum;
    int n;
    int id;
    double aveScore;
    int rank;
};

bool cmp(AverageScore a, AverageScore b) {
    return a.aveScore > b.aveScore;
}

// 文档管理模块
class DocumentManager {
public:
    // 上传文档，使用文件名和项目ID，支持覆盖
    void uploadDocument(const QString& docName, int projectId, bool overwrite) {
        if (overwrite) {
            int docId = getDocumentId(projectId, docName);
            if (docId > 0) {
                int currentVersion = getDocumentVersion(docId);
                // **删除物理文件的逻辑移至 FileDropWidget，此处仅更新版本号**
                updateDocumentVersion(docId, currentVersion + 1);
                return;
            }
        }

        // 新文件上传：生成哈希值并插入数据库
        string hash = calculateHash(docName.toStdString());
        stmt.str("");
        stmt << "INSERT INTO document (doc_name, hash_code, version, tags, project_id) "
            << "VALUES ('" << docName.toStdString() << "', '" << hash << "', 1, '[]', " << projectId << ")";
        executeQuery(stmt.str());
        freeResult(); // 释放结果集
    }

    // 查询文档版本历史，以文档ID为查询条件
    vector<VersionInfo> getVersionHistoryByProject(int projectId) {
        vector<VersionInfo> history;
        stmt.str("");
        // 查询 doc_id、doc_name、version 字段
        stmt << "SELECT doc_id, doc_name, version FROM document WHERE project_id=" << projectId;
        executeQuery(stmt.str());

        while ((row = mysql_fetch_row(res_set))) {
            VersionInfo vi;
            vi.docId = atoi(row[0]);
            vi.docName = QString::fromUtf8(row[1]);
            vi.versionNumber = atoi(row[2]);
            vi.projectId = projectId;
            vi.filePath = QString("uploads/%1/%2").arg(projectId).arg(vi.docName);
            history.push_back(vi);
        }
        freeResult(); // 释放结果集
        return history;
    }

    // 添加获取项目文件列表方法
    vector<pair<int, QString>> getProjectDocuments(int projectId) {
        vector<pair<int, QString>> documents;
        stmt.str("");
        stmt << "SELECT doc_id, doc_name FROM document WHERE project_id=" << projectId;
        executeQuery(stmt.str());

        while ((row = mysql_fetch_row(res_set))) {
            documents.emplace_back(atoi(row[0]), QString::fromUtf8(row[1]));
        }
        freeResult(); // 释放结果集
        return documents;
    }

    // 获取获取项目ID
    int getProjectIdFromDocId(int docId) {
        stmt.str("");
        stmt << "SELECT project_id FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int projectId = row ? atoi(row[0]) : 0; // 返回项目ID，若不存在则返回0
        freeResult(); // 释放结果集
        return projectId;
    }

    // 添加删除文档方法
    void deleteDocument(int docId) {
        // 查询文件名
        stmt.str("");
        stmt << "SELECT doc_name FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        if (!row) {
            freeResult(); // 释放结果集
            return;
        }

        QString docName = QString::fromUtf8(row[0]);

        // 获取项目ID（需从数据库查询，假设document表有project_id字段）
        stmt.str("");
        stmt << "SELECT project_id FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        if (!row) {
            freeResult(); // 释放结果集
            return;
        }
        int projectId = atoi(row[0]);

        // 删除物理文件（路径为uploads/项目ID/文件名）
        deletePhysicalFile(projectId, docName);

        // 删除数据库记录
        stmt.str("");
        stmt << "DELETE FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        freeResult(); // 释放结果集
    }

private:
    // 检查文件是否存在，返回文档ID
    int getDocumentId(int projectId, const QString& docName) {
        stmt.str("");
        stmt << "SELECT doc_id FROM document WHERE project_id=" << projectId
            << " AND doc_name='" << docName.toStdString() << "'";
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int docId = row ? atoi(row[0]) : 0;
        freeResult(); // 释放结果集
        return docId;
    }

    // 获取文档版本号
    int getDocumentVersion(int docId) {
        stmt.str("");
        stmt << "SELECT version FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int version = row ? atoi(row[0]) : 0;
        freeResult(); // 释放结果集
        return version;
    }

    // 更新文档版本号
    void updateDocumentVersion(int docId, int newVersion) {
        stmt.str("");
        stmt << "UPDATE document SET version=" << newVersion
            << ", modify_time=NOW() WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        freeResult(); // 释放结果集
    }

    // 删除物理文件
    void deletePhysicalFile(int projectId, const QString& docName) {
        QDir uploadDir(QCoreApplication::applicationDirPath());
        uploadDir.cd(QString("uploads/%1").arg(projectId)); // 进入项目ID子目录
        QFile file(uploadDir.filePath(docName));
        file.remove();
    }

    string calculateHash(const string& content) {
        return "HASH-" + to_string(rand() % 1000000);
    }

    void executeQuery(const string& q) {
        freeResult(); // 先释放之前的结果集
        /*if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }*/
        // 第一次尝试执行查询
        if (mysql_query(conn, q.c_str())) {
            // 检查是否是连接已断开的错误
            unsigned int err_no = mysql_errno(conn);
            if (err_no == CR_SERVER_GONE_ERROR || err_no == CR_SERVER_LOST) {
                QMessageBox::warning(nullptr, u8"连接中断", u8"数据库连接已断开，正在尝试重新连接...");

                // 尝试重新连接
                if (mysql_real_connect(conn, HOST, USER, PASS, DATABASE, PORT, nullptr, 0)) {
                    QMessageBox::information(nullptr, u8"重连成功", u8"已重新连接到数据库，请重试刚才的操作。");

                    // 重连成功后，再次执行之前的查询
                    if (mysql_query(conn, q.c_str())) {
                        // 如果重试后仍然失败，则报告最终错误
                        QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
                    }
                    else {
                        res_set = mysql_store_result(conn);
                    }

                }
                else {
                    QMessageBox::critical(nullptr, u8"重连失败", u8"无法重新连接到数据库，请检查网络和数据库服务器状态。");
                    // 可以选择退出程序
                    // exit(1);
                }
            }
            else {
                // 如果是其他类型的 SQL 错误，直接报告
                QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
            }
        }
        else {
            // 首次执行成功
            res_set = mysql_store_result(conn);
        }
    }

    void freeResult() {
        if (res_set) {
            mysql_free_result(res_set);
            res_set = nullptr;
        }
    }
};

// 拖拽文件上传窗口
class FileDropWidget : public QWidget {
    Q_OBJECT
public:
    FileDropWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAcceptDrops(true);
        initUI();
    }

    void setCallback(function<void(const QString&, int, bool)> callback) {
        uploadCallback = callback;
    }

    void setProjectId(int id) {
        currentProjectId = id;
    }

signals:
    void fileUploaded(const QString& fileName);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override {
        const QMimeData* mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            QList<QUrl> urlList = mimeData->urls();
            for (const QUrl& url : urlList) {
                QString filePath = url.toLocalFile();
                uploadFile(filePath);
            }
        }
    }

private:
    void initUI() {
        setStyleSheet("background-color: #f0f0f0; border: 2px dashed #aaa; border-radius: 10px;");

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignmentFlag::AlignCenter); // 可选，设置布局对齐方式

        QLabel* infoLabel = new QLabel(u8"拖拽文件到此处上传", this);
        infoLabel->setAlignment(Qt::AlignCenter); // 直接使用 Qt::AlignCenter
        infoLabel->setStyleSheet("font-size: 14px; color: #555;");
        layout->addWidget(infoLabel);
    }

    void uploadFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, u8"文件打开失败", u8"无法打开文件: " + filePath);
            return;
        }

        // 构建上传目录路径
        QDir uploadBaseDir(QCoreApplication::applicationDirPath());

        // 直接创建项目子目录（包含uploads/项目ID）
        QString projectDirPath = QString("uploads/%1").arg(currentProjectId);
        if (!uploadBaseDir.mkpath(projectDirPath)) {
            QMessageBox::critical(this, u8"错误", u8"无法创建上传目录");
            return;
        }

        QDir uploadDir(uploadBaseDir.filePath(projectDirPath));
        QString originalFileName = QFileInfo(filePath).fileName();
        QString uploadPath = uploadDir.filePath(originalFileName);

        bool overwrite = false;
        if (uploadDir.exists(originalFileName)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, u8"文件已存在",
                u8"文件 " + originalFileName + u8" 已存在，是否覆盖？",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply != QMessageBox::Yes) {
                QMessageBox::information(this, u8"上传取消", u8"文件上传已取消");
                return;
            }
            overwrite = true;
            if (!QFile::remove(uploadPath)) {
                QMessageBox::critical(this, u8"删除失败", u8"无法删除旧文件");
                return;
            }
        }

        if (!QFile::copy(filePath, uploadPath)) {
            QMessageBox::critical(this, u8"上传失败", u8"无法复制文件到上传目录");
            return;
        }

        if (uploadCallback) {
            uploadCallback(originalFileName, currentProjectId, overwrite);
        }
        emit fileUploaded(originalFileName);
        QMessageBox::information(this, u8"上传成功", overwrite ? u8"文件已覆盖" : u8"文件已上传");
    }

private:
    function<void(const QString&, int, bool)> uploadCallback;
    int currentProjectId = 0;
};

// 评分动态管理模块（移除任务相关概念）
class ScoreAnalyzer {
public:
    // 录入评分，使用projectId替代taskId
    void enterScore(ScoreInfo score) {
        if (score.score < 0 || score.score > 100) {
            QMessageBox::critical(nullptr, u8"错误", u8"评分必须在0.00-100.00之间");
            return;
        }
        stmt.str("");
        // 显式指定字段，使用fixed格式化保证两位小数
        stmt << "INSERT INTO score (user_id, project_id, score) "
            << "VALUES (" << score.userId << ", " << score.projectId << ", "
            << fixed << setprecision(2) << score.score << ")"; // 新增格式化输出
        executeQuery(stmt.str());
        freeResult(); // 释放结果集
        QMessageBox::information(nullptr, u8"提示", u8"评分录入成功");
    }

    // 生成评分绩效报告，按项目ID统计用户评分
    void generateReport(int projectId) {
        QMessageBox infoBox;
        infoBox.setWindowTitle(u8"绩效报告");
        QString output = QString(u8"=== 项目 %1 绩效报告 ===\n").arg(projectId);

        stmt.str("");
        stmt << "SELECT user_id, score, create_time "
            << "FROM `score` "
            << "WHERE project_id=" << projectId;

        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"查询失败", QString::fromUtf8(mysql_error(conn)));
            return;
        }

        MYSQL_RES* result = mysql_store_result(conn);
        if (!result) {
            QMessageBox::critical(nullptr, u8"错误", u8"无法获取评分数据");
            return;
        }

        // 添加表头
        output += u8"用户ID\t评分\t提交时间\n";

        // 遍历结果集
        while ((row = mysql_fetch_row(result))) {
            // 处理字段值（注意：需确保字段存在）
            QString userId = row[0] ? QString::fromUtf8(row[0]) : "未知";
            QString score = row[1] ? QString::fromUtf8(row[1]) : "0.00";
            QString time = row[2] ? QString::fromUtf8(row[2]) : "未知时间";

            output += QString("%1\t%2\t%3\n").arg(userId).arg(score).arg(time);
        }

        // 释放结果集
        mysql_free_result(result);
        result = nullptr;

        infoBox.setText(output);
        infoBox.exec();
    }

    // 获取历史评分记录
    vector<ScoreInfo> getScoreHistory(int projectId) {
        vector<ScoreInfo> scores;
        stmt.str("");
        stmt << "SELECT score_id, user_id, score, create_time "
            << "FROM score "
            << "WHERE project_id=" << projectId
            << " ORDER BY create_time DESC";

        // 修复：添加结果集处理
        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"查询失败", QString::fromUtf8(mysql_error(conn)));
            return scores;
        }

        MYSQL_RES* result = mysql_store_result(conn);  // 获取结果集
        if (!result) {
            QMessageBox::critical(nullptr, u8"错误", u8"无法获取评分记录");
            return scores;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            ScoreInfo info;
            info.scoreId = atoi(row[0]);
            info.userId = atoi(row[1]);
            info.score = atof(row[2]);
            info.createTime = row[3];
            scores.push_back(info);
        }

        // 释放结果集
        mysql_free_result(result);
        result = nullptr;

        return scores;
    }

    // 删除评分记录
    void deleteScore(int scoreId) {
        stmt.str("");
        stmt << "DELETE FROM score WHERE score_id=" << scoreId;
        executeQuery(stmt.str());

        if (mysql_affected_rows(conn) > 0) {
            QMessageBox::information(nullptr, u8"删除成功", u8"评分记录已删除");
        }
        else {
            QMessageBox::warning(nullptr, u8"删除失败", u8"未找到对应的评分记录");
        }
        freeResult(); // 释放结果集
    }

    // 获取评分排名
    vector<AverageScore> getScoreRanking() {
        int tot = 0;
        map<int, int> f; //将projectId映射到0-tot-1
        map<pair<int, int>, int> mp; //去重，确保每个用户对每个项目只计一次评分
        vector<AverageScore> ave;

        stmt.str("");
        stmt << "SELECT score_id, user_id, score, create_time, project_id "
            << "FROM score "
            << "ORDER BY create_time DESC";

        // 修复：添加结果集处理
        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"查询失败", QString::fromUtf8(mysql_error(conn)));
            return ave;
        }

        MYSQL_RES* result = mysql_store_result(conn);  // 获取结果集
        if (!result) {
            QMessageBox::critical(nullptr, u8"错误", u8"无法获取评分记录");
            return ave;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            ScoreInfo info;
            info.scoreId = atoi(row[0]);
            info.userId = atoi(row[1]);
            info.score = atof(row[2]);
            info.createTime = row[3];
            info.projectId = atoi(row[4]);

            pair<int, int> key = { info.projectId, info.userId };

            if (f.find(info.projectId) == f.end()) {
                f[info.projectId] = tot++;
                ave.push_back({ 0, 0, info.projectId, 0 });
            }
            if (mp.find(key) == mp.end()) { // 只统计每个用户对每个项目的最新评分
                mp[key] = 1;
                ave[f[info.projectId]].sum += info.score;
                ave[f[info.projectId]].n++;
            }
        }

        // 释放结果集
        mysql_free_result(result);
        result = nullptr;

        for (int i = 0; i < tot; i++) {
            if (ave[i].n > 0) {
                ave[i].aveScore = ave[i].sum / ave[i].n;
            }
            else {
                ave[i].aveScore = 0.0; // 避免除以零
            }
        }

        sort(ave.begin(), ave.end(), cmp);

        for (int i = 0; i < tot; i++) {
            ave[i].rank = i + 1;
        }

        return ave;
    }

private:

    void executeQuery(const string& q) {
        freeResult(); // 先释放之前的结果集
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }
    }

    void freeResult() {
        if (res_set) {
            mysql_free_result(res_set);
            res_set = nullptr;
        }
    }
};

// GitHub集成模块（支持项目分支和更新）
// GitHub集成模块（最终优化版，已修复编译错误）
class GitHubIntegration {
public:
    GitHubIntegration() {
        gitPath = QStandardPaths::findExecutable("git");
        if (gitPath.isEmpty()) {
            QMessageBox::critical(nullptr, u8"错误",
                u8"未在系统中找到git.exe。请确保Git已安装并已将其路径添加到系统PATH环境变量中。");
        }
    }

    void syncProjectToGitHub(int projectId, const QString& remoteUrl) {
        if (gitPath.isEmpty()) return;

        QString projectFilesPath = QCoreApplication::applicationDirPath() + "/uploads/" + QString::number(projectId);
        QString gitRepoPath = QCoreApplication::applicationDirPath() + "/git_repos/" + QString::number(projectId);
        QString branchName = "project/" + QString::number(projectId);

        QDir projectDir(projectFilesPath);
        if (!projectDir.exists() || projectDir.isEmpty()) {
            QMessageBox::critical(nullptr, u8"错误", u8"项目文件目录不存在或为空: " + projectFilesPath);
            return;
        }

        if (!prepareLocalRepository(gitRepoPath, remoteUrl)) return;
        if (!checkoutProjectBranch(gitRepoPath, branchName)) return;
        if (!syncFilesToRepo(projectFilesPath, gitRepoPath)) return;

        // --- Add, Commit, Push 流程 ---
        auto addResult = runGitCommand({ "add", "." }, gitRepoPath);
        if (!addResult.first) {
            showErrorDialog("Git Add 失败", addResult.second);
            return;
        }

        QString commitMessage = QString("Update project %1 at %2").arg(projectId).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        auto commitResult = runGitCommand({ "commit", "-m", commitMessage }, gitRepoPath);
        if (!commitResult.first) {
            if (commitResult.second.contains("nothing to commit") || commitResult.second.contains(u8"无文件要提交")) {
                QMessageBox::information(nullptr, u8"提示", u8"本地文件与仓库中的最新版本一致，无需更新。");
                return;
            }
            showErrorDialog("Git Commit 失败", commitResult.second);
            return;
        }

        auto pushResult = runGitCommand({ "push", "--set-upstream", "origin", branchName }, gitRepoPath);
        if (!pushResult.first) {
            showErrorDialog("Git Push 失败", pushResult.second);
            return;
        }

        QMessageBox::information(nullptr, u8"成功", QString(u8"项目 %1 已成功同步到分支 '%2'！\n%3").arg(projectId).arg(branchName).arg(pushResult.second));
        saveRepoUrlToDatabase(projectId, remoteUrl);
    }

private:
    QString gitPath;

    // 修正后的 runGitCommand
    QPair<bool, QString> runGitCommand(const QStringList& args, const QString& workingDir) {
        QProcess process; // 在函数内部创建局部 QProcess 对象
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setWorkingDirectory(workingDir);
        process.start(gitPath, args);
        if (!process.waitForFinished(-1)) {
            return { false, "Git command timed out: " + args.join(' ') };
        }
        QString output = process.readAllStandardOutput().trimmed();
        return { process.exitCode() == 0, output };
    }

    void showErrorDialog(const QString& title, const QString& gitOutput) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle(title);
        msgBox.setText(u8"Git命令执行失败。");
        msgBox.setDetailedText(gitOutput);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }

    bool prepareLocalRepository(const QString& repoPath, const QString& remoteUrl) {
        QDir repoDir(repoPath);
        if (repoDir.exists()) {
            auto fetchResult = runGitCommand({ "fetch", "origin" }, repoPath);
            if (!fetchResult.first) {
                showErrorDialog("Git Fetch 失败", fetchResult.second);
                return false;
            }
            runGitCommand({ "remote", "set-url", "origin", remoteUrl }, repoPath);
        }
        else {
            auto cloneResult = runGitCommand({ "clone", remoteUrl, repoPath }, QCoreApplication::applicationDirPath());
            if (!cloneResult.first) {
                showErrorDialog("Git Clone 失败", cloneResult.second);
                return false;
            }
        }
        return true;
    }

    bool checkoutProjectBranch(const QString& repoPath, const QString& branchName) {
        auto checkoutResult = runGitCommand({ "checkout", branchName }, repoPath);
        if (checkoutResult.first) {
            auto pullResult = runGitCommand({ "pull", "origin", branchName }, repoPath);
            if (!pullResult.first && !pullResult.second.contains("Already up to date") && !pullResult.second.contains(u8"已经是最新")) {
                showErrorDialog("Git Pull 失败", pullResult.second);
                // 即使pull失败，也不一定需要终止，可以继续尝试，让push来决定是否冲突
                // return false; 
            }
            return true;
        }

        auto createBranchResult = runGitCommand({ "checkout", "-b", branchName }, repoPath);
        if (!createBranchResult.first) {
            showErrorDialog("创建分支失败", createBranchResult.second);
            return false;
        }
        return true;
    }

    // (请确保你已经有下面的函数)
    bool syncFilesToRepo(const QString& sourceDir, const QString& destDir) {
        QDir dest(destDir);
        for (const QFileInfo& info : dest.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
            if (info.fileName() == ".git") continue;
            if (info.isDir()) {
                QDir(info.filePath()).removeRecursively();
            }
            else {
                QFile::remove(info.filePath());
            }
        }

        QDir source(sourceDir);
        for (const QFileInfo& info : source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
            QString srcPath = info.filePath();
            QString dstPath = dest.filePath(info.fileName());
            if (info.isDir()) {
                QDir(dstPath).mkpath("."); // 创建子目录
                // 注意：这里需要一个递归函数来复制子目录内容，为简化，假设目前只有一层
            }
            else {
                if (!QFile::copy(srcPath, dstPath)) {
                    QMessageBox::critical(nullptr, "File Copy Error", "Failed to copy: " + srcPath);
                    return false;
                }
            }
        }
        return true;
    }

    void saveRepoUrlToDatabase(int projectId, const QString& remoteUrl) {
        stmt.str("");
        stmt << "SELECT repo_id FROM github_repo WHERE project_id=" << projectId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        freeResult();

        if (row) {
            stmt.str("");
            stmt << "UPDATE github_repo SET repo_url='" << remoteUrl.toStdString() << "' WHERE project_id=" << projectId;
        }
        else {
            stmt.str("");
            stmt << "INSERT INTO github_repo (project_id, repo_url, permission) "
                << "VALUES (" << projectId << ", '" << remoteUrl.toStdString() << "', 'private')";
        }

        executeQuery(stmt.str());
        freeResult();
    }

    void executeQuery(const string& q) {
        freeResult();
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }
    }

    void freeResult() {
        if (res_set) {
            mysql_free_result(res_set);
            res_set = nullptr;
        }
    }
};

// 文件列表窗口类
class FileListWindow : public QDialog {
    Q_OBJECT
public:
    FileListWindow(int projectId, QWidget* parent = nullptr)
        : QDialog(parent), projectId(projectId) {
        setupUI();
        loadDocuments();
    }

private slots:
    void onOpenFile() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item) return;

        int docId = item->data(Qt::UserRole).toInt();
        QString docName = item->text().split(" (v")[0]; // 移除版本号部分

        // 确保路径构建与上传时一致
        QDir uploadDir(QCoreApplication::applicationDirPath());
        if (!uploadDir.cd("uploads/" + QString::number(projectId))) {
            QMessageBox::warning(this, "错误", "无法找到项目目录");
            return;
        }

        QString filePath = uploadDir.filePath(docName);

        if (!QFile::exists(filePath)) {
            QMessageBox::warning(this, "错误", "文件不存在: " + filePath);
            return;
        }

        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            QMessageBox::warning(this, "错误", "无法打开该文件: " + filePath);
        }
    }

    void onDeleteFile() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item) return;

        if (QMessageBox::question(this, "确认删除", "确定要删除该文件吗？",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            int docId = item->data(Qt::UserRole).toInt();
            DocumentManager dm;
            dm.deleteDocument(docId);
            delete item;
            QMessageBox::information(this, "删除成功", "文件已成功删除");
        }
    }

private:
    void setupUI() {
        setWindowTitle("已上传文件列表");
        setGeometry(300, 300, 500, 400);

        QVBoxLayout* layout = new QVBoxLayout(this);

        listWidget = new QListWidget();
        layout->addWidget(listWidget);

        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* openBtn = new QPushButton("打开文件");
        QPushButton* deleteBtn = new QPushButton("删除文件");

        buttonLayout->addWidget(openBtn);
        buttonLayout->addWidget(deleteBtn);
        layout->addLayout(buttonLayout);

        connect(openBtn, &QPushButton::clicked, this, &FileListWindow::onOpenFile);
        connect(deleteBtn, &QPushButton::clicked, this, &FileListWindow::onDeleteFile);
    }

    void loadDocuments() {
        DocumentManager dm;
        auto docs = dm.getProjectDocuments(projectId);

        foreach(auto & doc, docs) {
            // 获取文件版本号
            stmt.str("");
            stmt << "SELECT version FROM document WHERE doc_id=" << doc.first;
            executeQuery(stmt.str());
            row = mysql_fetch_row(res_set);
            int version = row ? atoi(row[0]) : 1;
            freeResult(); // 释放结果集

            // 只使用原始文件名，不包含路径
            QString fileName = QFileInfo(doc.second).fileName();

            QListWidgetItem* item = new QListWidgetItem(fileName + " (v" + QString::number(version) + ")");
            item->setData(Qt::UserRole, doc.first);
            item->setData(Qt::UserRole + 1, fileName); // 存储原始文件名
            listWidget->addItem(item);
        }
    }

    void executeQuery(const string& q) {
        freeResult(); // 先释放之前的结果集
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }
    }

    void freeResult() {
        if (res_set) {
            mysql_free_result(res_set);
            res_set = nullptr;
        }
    }

    QListWidget* listWidget = nullptr;
    int projectId;
};

// 评分列表窗口类
class ScoreListWindow : public QDialog {
    Q_OBJECT
public:
    ScoreListWindow(int projectId, QWidget* parent = nullptr)
        : QDialog(parent), projectId(projectId) {
        setupUI();
        loadScores();
    }

    ScoreListWindow(QWidget* parent = nullptr) {
        setupUI();
        loadScoresforRanking();
    }

private slots:
    void onDeleteScore() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item) return;

        int scoreId = item->data(Qt::UserRole).toInt();
        if (QMessageBox::question(this, "确认删除", "确定要删除这条评分记录吗？",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            ScoreAnalyzer sa;
            sa.deleteScore(scoreId);
            delete item;
        }
    }

private:
    void setupUI() {
        setWindowTitle("历史评分记录");
        setGeometry(300, 300, 600, 400);

        QVBoxLayout* layout = new QVBoxLayout(this);

        listWidget = new QListWidget();
        layout->addWidget(listWidget);

        QPushButton* deleteBtn = new QPushButton("删除选中评分");
        layout->addWidget(deleteBtn);

        connect(deleteBtn, &QPushButton::clicked, this, &ScoreListWindow::onDeleteScore);
    }

    void loadScores() {
        ScoreAnalyzer sa;
        auto scores = sa.getScoreHistory(projectId);

        // 如果没有获取到评分记录，给出提示
        if (scores.empty()) {
            QMessageBox::information(this, "提示", "未找到该项目的评分记录。");
            return;
        }

        // 定义格式化字符串常量，提高代码可维护性
        const QString itemFormat = "时间: %1 | 评分: %2 | 用户ID: %3";

        foreach(auto & score, scores) {
            // 格式化显示：时间-评分-用户ID（实际应用中应替换为用户名）
            QString itemText = QString(itemFormat)
                .arg(QString::fromStdString(score.createTime))
                .arg(score.score)
                .arg(score.userId);
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, score.scoreId);
            listWidget->addItem(item);
        }
    }

    void loadScoresforRanking() {
        ScoreAnalyzer sa;
        auto scores = sa.getScoreRanking();

        // 如果没有获取到评分记录，给出提示
        if (scores.empty()) {
            QMessageBox::information(this, "提示", "未找到评分记录。");
            return;
        }

        // 定义格式化字符串常量，提高代码可维护性
        const QString itemFormat = "排名: %1 | 项目: %2 | 平均分: %3 | 评分人数: %4";

        foreach(auto & score, scores) {
            // 格式化显示：排名-项目-平均分-评分人数
            QString itemText = QString(itemFormat)
                .arg(score.rank)
                .arg(score.id)
                .arg(score.aveScore)
                .arg(score.n);
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, score.id);
            listWidget->addItem(item);
        }
    }

    QListWidget* listWidget = nullptr;
    int projectId;
};

// 文档管理窗口
class DocumentManagementWindow : public QDialog {
    Q_OBJECT
public:
    DocumentManagementWindow(QWidget* parent = nullptr) : QDialog(parent) {
        setupUI();
    }

    void setProjectId(int id) {
        projectId = id;
        fileDropWidget->setProjectId(id);
    }

signals:
    void documentUploaded(const QString& docName, int projectId);

private slots:
    void onFileUploaded(const QString& fileName) {
        emit documentUploaded(fileName, projectId);
        close();
    }

private:
    void setupUI() {
        setWindowTitle("文档上传");
        resize(400, 300);

        QVBoxLayout* layout = new QVBoxLayout(this);

        fileDropWidget = new FileDropWidget(this);
        fileDropWidget->setCallback([this](const QString& fileName, int projectId, bool overwrite) {
            DocumentManager dm;
            dm.uploadDocument(fileName, projectId, overwrite);
            emit documentUploaded(fileName, projectId);
            });

        connect(fileDropWidget, &FileDropWidget::fileUploaded, this, &DocumentManagementWindow::onFileUploaded);

        layout->addWidget(fileDropWidget);
    }

private:
    FileDropWidget* fileDropWidget;
    int projectId = 0;
};

// 在 MainSystemWidget 类的定义之前，添加这个新的对话框类

class DocumentMenuDialog : public QDialog {
    Q_OBJECT
public:
    DocumentMenuDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(u8"文档管理模块");
        setModal(true); // 设置为模态对话框
        setupUI();
    }

private slots:
    void onUploadClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"输入项目ID", u8"请输入要上传文档的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        DocumentManagementWindow docUploadWindow(this);
        docUploadWindow.setProjectId(projectId);
        docUploadWindow.exec();
        accept(); // 完成操作后关闭子菜单
    }

    void onHistoryClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"输入项目ID", u8"请输入要查看版本历史的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        DocumentManager dm;
        auto history = dm.getVersionHistoryByProject(projectId);
        if (history.empty()) {
            QMessageBox::information(this, u8"提示", u8"该项目下无文档版本记录。");
            return;
        }

        map<int, vector<VersionInfo>> docVersions;
        for (const auto& vi : history) {
            docVersions[vi.docId].push_back(vi);
        }

        QString output = u8"项目ID：" + QString::number(projectId) + u8" 文档版本历史：\n";
        for (const auto& pair : docVersions) {
            output += u8"\n文档名称：" + pair.second.front().docName + u8"\n";
            for (const auto& v : pair.second) {
                output += QString(u8"  版本 %1\n").arg(v.versionNumber);
            }
        }
        QMessageBox::information(this, u8"项目版本历史", output);
        accept();
    }

    void onListFilesClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"输入项目ID", u8"请输入要查看文件的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        FileListWindow fileListWindow(projectId, this);
        fileListWindow.exec();
        accept();
    }

private:
    void setupUI() {
        this->setMinimumWidth(400);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setSpacing(15);
        layout->setContentsMargins(20, 20, 20, 20);

        QPushButton* uploadButton = new QPushButton(u8"上传新文档");
        QPushButton* historyButton = new QPushButton(u8"查看版本历史");
        QPushButton* listFilesButton = new QPushButton(u8"管理已上传文件");

        // 应用与主界面类似的样式，但颜色稍作区别
        QString subMenuStyle = R"(
            QPushButton {
                background-color: #3498db; /* 换一种蓝色 */
                color: white;
                font-size: 16px;
                border-radius: 8px;
                padding: 12px;
                border: none;
            }
            QPushButton:hover { background-color: #2980b9; }
            QPushButton:pressed { background-color: #2471a3; }
        )";
        uploadButton->setStyleSheet(subMenuStyle);
        historyButton->setStyleSheet(subMenuStyle);
        listFilesButton->setStyleSheet(subMenuStyle);

        layout->addWidget(new QLabel(u8"请选择一项操作："));
        layout->addWidget(uploadButton);
        layout->addWidget(historyButton);
        layout->addWidget(listFilesButton);

        connect(uploadButton, &QPushButton::clicked, this, &DocumentMenuDialog::onUploadClicked);
        connect(historyButton, &QPushButton::clicked, this, &DocumentMenuDialog::onHistoryClicked);
        connect(listFilesButton, &QPushButton::clicked, this, &DocumentMenuDialog::onListFilesClicked);
    }
};

// 在 DocumentMenuDialog 类的定义之后，添加这个新的对话框类

class ScoreMenuDialog : public QDialog {
    Q_OBJECT
public:
    ScoreMenuDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(u8"绩效评分模块");
        setModal(true);
        setupUI();
    }

private slots:
    void onEnterScoreClicked() {
        ScoreAnalyzer sa;
        ScoreInfo score;
        bool ok;

        score.userId = QInputDialog::getInt(this, u8"输入用户ID", u8"请输入您的用户ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        score.projectId = QInputDialog::getInt(this, u8"输入项目ID", u8"请输入您要评分的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        score.score = QInputDialog::getDouble(this, u8"输入评分", u8"请输入评分（0-100）：", 50.0, 0, 100, 2, &ok);
        if (!ok) return;

        sa.enterScore(score);
        sa.generateReport(score.projectId);
        accept();
    }

    void onViewHistoryClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"输入项目ID", u8"请输入要查看评分历史的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        ScoreListWindow scoreWindow(projectId, this);
        scoreWindow.exec();
        accept();
    }

    void onRankingClicked() {
        ScoreListWindow scoreWindow(this); // 调用无参构造函数以显示排名
        scoreWindow.exec();
        accept();
    }

private:
    void setupUI() {
        this->setMinimumWidth(400);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setSpacing(15);
        layout->setContentsMargins(20, 20, 20, 20);

        QPushButton* enterScoreButton = new QPushButton(u8"录入评分");
        QPushButton* viewHistoryButton = new QPushButton(u8"查看项目评分历史");
        QPushButton* rankingButton = new QPushButton(u8"查看所有项目排名");

        QString subMenuStyle = R"(
            QPushButton {
                background-color: #2ecc71; /* 绿色系 */
                color: white;
                font-size: 16px;
                border-radius: 8px;
                padding: 12px;
                border: none;
            }
            QPushButton:hover { background-color: #27ae60; }
            QPushButton:pressed { background-color: #229954; }
        )";
        enterScoreButton->setStyleSheet(subMenuStyle);
        viewHistoryButton->setStyleSheet(subMenuStyle);
        rankingButton->setStyleSheet(subMenuStyle);

        layout->addWidget(new QLabel(u8"请选择一项操作："));
        layout->addWidget(enterScoreButton);
        layout->addWidget(viewHistoryButton);
        layout->addWidget(rankingButton);

        connect(enterScoreButton, &QPushButton::clicked, this, &ScoreMenuDialog::onEnterScoreClicked);
        connect(viewHistoryButton, &QPushButton::clicked, this, &ScoreMenuDialog::onViewHistoryClicked);
        connect(rankingButton, &QPushButton::clicked, this, &ScoreMenuDialog::onRankingClicked);
    }
};

// 主系统窗口
class MainSystemWidget : public QWidget {
    Q_OBJECT
public:
    MainSystemWidget(QWidget* parent = nullptr) : QWidget(parent) {
        initDatabase();
        setupUI();
    }

    void executeQuery(const string& q) {
        freeResult(); // 先释放之前的结果集
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"数据库操作失败", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }
    }

private:
    void initDatabase() {
        conn = mysql_init(nullptr);
        if (!conn || !mysql_real_connect(conn, HOST, USER, PASS, DATABASE, PORT, nullptr, 0)) {
            QMessageBox::critical(nullptr, u8"数据库连接失败", QString::fromUtf8(mysql_error(conn)));
            exit(1);
        }
        QMessageBox::information(nullptr, u8"提示", u8"数据库连接成功");
    }

    void setupUI() {
        // --- 1. 整体布局和背景色 ---
        this->setStyleSheet("background-color: #f5f5f7;"); // 设置一个柔和的浅灰色背景
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(50, 40, 50, 40); // 增加边距，让内容呼吸
        mainLayout->setSpacing(30); // 增加控件之间的间距

        // --- 2. 添加标题和图标 ---
        QHBoxLayout* titleLayout = new QHBoxLayout();
        // 你需要在项目资源文件(.qrc)中添加一个图标文件，例如 icon.png
        // 如果没有，可以先注释掉下面这行
        // QLabel* iconLabel = new QLabel();
        // iconLabel->setPixmap(QPixmap(":/icons/icon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel* titleLabel = new QLabel(u8"项目协同管理系统");
        titleLabel->setStyleSheet(
            "font-size: 32px;"
            "font-weight: bold;"
            "color: #333;"
        );

        // titleLayout->addWidget(iconLabel); // 如果有图标则取消注释
        titleLayout->addWidget(titleLabel);
        titleLayout->addStretch(); // 将标题推到左边

        // --- 3. 创建功能按钮（使用网格布局更灵活） ---
        QGridLayout* buttonLayout = new QGridLayout();
        buttonLayout->setSpacing(20);

        QPushButton* docManagementButton = new QPushButton(u8"文档管理");
        QPushButton* scoreManagementButton = new QPushButton(u8"绩效评分");
        QPushButton* gitHubIntegrationButton = new QPushButton(u8"代码同步");
        QPushButton* exitButton = new QPushButton(u8"退出系统");

        // 将四个按钮放入网格布局
        buttonLayout->addWidget(docManagementButton, 0, 0);
        buttonLayout->addWidget(scoreManagementButton, 0, 1);
        buttonLayout->addWidget(gitHubIntegrationButton, 1, 0);
        buttonLayout->addWidget(exitButton, 1, 1);

        // --- 4. 统一设置按钮样式 (核心部分) ---
        QString buttonStyle = R"(
            QPushButton {
                background-color: #007aff; /* 苹果蓝 */
                color: white;
                font-size: 18px;
                font-weight: bold;
                border-radius: 12px;
                padding: 15px;
                min-height: 80px; /* 保证按钮有足够的高度 */
                border: none;
            }
            QPushButton:hover {
                background-color: #005ecb; /* 悬停时颜色变深 */
            }
            QPushButton:pressed {
                background-color: #004a9e; /* 点击时颜色更深 */
            }
        )";

        // 为每个按钮应用样式
        docManagementButton->setStyleSheet(buttonStyle);
        scoreManagementButton->setStyleSheet(buttonStyle);
        gitHubIntegrationButton->setStyleSheet(buttonStyle);

        // 退出按钮可以设置一个不同的颜色，以示区别
        exitButton->setStyleSheet(R"(
            QPushButton {
                background-color: #e74c3c; /* 红色，表示危险或退出操作 */
                color: white;
                font-size: 18px;
                font-weight: bold;
                border-radius: 12px;
                padding: 15px;
                min-height: 80px;
                border: none;
            }
            QPushButton:hover {
                background-color: #c0392b;
            }
            QPushButton:pressed {
                background-color: #a93226;
            }
        )");

        // --- 5. 将所有布局添加到主布局中 ---
        mainLayout->addLayout(titleLayout);
        mainLayout->addStretch(1); // 添加一个伸缩项，把按钮往下推
        mainLayout->addLayout(buttonLayout);
        mainLayout->addStretch(2); // 在底部添加一个更大的伸缩项

        // --- 6. 连接信号和槽 (保持不变) ---
        connect(docManagementButton, &QPushButton::clicked, this, &MainSystemWidget::handleDocumentManagement);
        connect(scoreManagementButton, &QPushButton::clicked, this, &MainSystemWidget::handleScoreManagement);
        connect(gitHubIntegrationButton, &QPushButton::clicked, this, &MainSystemWidget::handleGitHubIntegration);
        connect(exitButton, &QPushButton::clicked, this, &MainSystemWidget::exitSystem);
    }

    void handleDocumentManagement() {
        DocumentMenuDialog dialog(this);
        dialog.exec(); // 显示文档管理子菜单，并等待它关闭
    }

    void handleScoreManagement() {
        ScoreMenuDialog dialog(this);
        dialog.exec(); // 显示评分管理子菜单，并等待它关闭
    }

    void handleGitHubIntegration() {
        GitHubIntegration git;
        int projectId;

        QMessageBox::information(nullptr, u8"提示", u8"--- GitHub集成 ---");
        bool ok;
        projectId = QInputDialog::getInt(this, u8"请输入项目ID", u8"请输入要同步到GitHub的项目ID：", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        // 检查项目文件目录是否存在
        QString projectPath = QCoreApplication::applicationDirPath() + "/uploads/" + QString::number(projectId);
        if (!QDir(projectPath).exists()) {
            QMessageBox::warning(this, u8"错误", u8"项目ID " + QString::number(projectId) + u8" 的文件目录不存在，请先上传文件。");
            return;
        }

        // 弹出对话框让用户输入远程仓库URL
        QString remoteUrl = QInputDialog::getText(this, u8"输入GitHub仓库URL",
            u8"请输入项目关联的远程Git仓库URL：\n"
            u8"(首次使用时，请确保该仓库已在GitHub上创建)\n"
            u8"例如: https://github.com/your-username/your-repo.git",
            QLineEdit::Normal, "", &ok);

        if (!ok || remoteUrl.isEmpty()) {
            QMessageBox::warning(this, u8"操作取消", u8"未输入URL，操作已取消。");
            return;
        }

        // 调用新的同步函数
        git.syncProjectToGitHub(projectId, remoteUrl);
    }

    void exitSystem() {
        mysql_close(conn);
        QMessageBox::information(nullptr, u8"提示", u8"系统已退出");
        qApp->quit();
    }

    void freeResult() {
        if (res_set) {
            mysql_free_result(res_set);
            res_set = nullptr;
        }
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(u8"项目管理系统");
    window.resize(800, 600);

    MainSystemWidget* mainWidget = new MainSystemWidget(&window);
    window.setCentralWidget(mainWidget);

    window.show();

    return app.exec();
}

#include "main.moc"