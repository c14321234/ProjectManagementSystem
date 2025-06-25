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

// ���ݿ��������ã�����ʵ������޸�
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

// �ĵ��汾��Ϣ�ṹ�壨��Ӧdocument���ֶΣ�
struct VersionInfo {
    int docId;           // �ĵ�ID
    QString docName;     // �ĵ�����
    int versionNumber;   // �汾��
    string modifyTime;   // �޸�ʱ�䣨��ѡ��
    string modifier;     // �޸��ߣ���ѡ��
    int projectId;       // ��ĿID
    QString filePath;    // �ļ�·��
};

// ������Ϣ�ṹ�壨�Ƴ�taskId��ʹ��projectId��
struct ScoreInfo {
    int scoreId;      // �������ּ�¼ID
    int userId;       // user_id
    int projectId;    // ��ĿID�����ԭtaskId
    double score;     // score
    string createTime;// create_time���Զ�����
};

//ƽ���ֽṹ��
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

// �ĵ�����ģ��
class DocumentManager {
public:
    // �ϴ��ĵ���ʹ���ļ�������ĿID��֧�ָ���
    void uploadDocument(const QString& docName, int projectId, bool overwrite) {
        if (overwrite) {
            int docId = getDocumentId(projectId, docName);
            if (docId > 0) {
                int currentVersion = getDocumentVersion(docId);
                // **ɾ�������ļ����߼����� FileDropWidget���˴������°汾��**
                updateDocumentVersion(docId, currentVersion + 1);
                return;
            }
        }

        // ���ļ��ϴ������ɹ�ϣֵ���������ݿ�
        string hash = calculateHash(docName.toStdString());
        stmt.str("");
        stmt << "INSERT INTO document (doc_name, hash_code, version, tags, project_id) "
            << "VALUES ('" << docName.toStdString() << "', '" << hash << "', 1, '[]', " << projectId << ")";
        executeQuery(stmt.str());
        freeResult(); // �ͷŽ����
    }

    // ��ѯ�ĵ��汾��ʷ�����ĵ�IDΪ��ѯ����
    vector<VersionInfo> getVersionHistoryByProject(int projectId) {
        vector<VersionInfo> history;
        stmt.str("");
        // ��ѯ doc_id��doc_name��version �ֶ�
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
        freeResult(); // �ͷŽ����
        return history;
    }

    // ��ӻ�ȡ��Ŀ�ļ��б���
    vector<pair<int, QString>> getProjectDocuments(int projectId) {
        vector<pair<int, QString>> documents;
        stmt.str("");
        stmt << "SELECT doc_id, doc_name FROM document WHERE project_id=" << projectId;
        executeQuery(stmt.str());

        while ((row = mysql_fetch_row(res_set))) {
            documents.emplace_back(atoi(row[0]), QString::fromUtf8(row[1]));
        }
        freeResult(); // �ͷŽ����
        return documents;
    }

    // ��ȡ��ȡ��ĿID
    int getProjectIdFromDocId(int docId) {
        stmt.str("");
        stmt << "SELECT project_id FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int projectId = row ? atoi(row[0]) : 0; // ������ĿID�����������򷵻�0
        freeResult(); // �ͷŽ����
        return projectId;
    }

    // ���ɾ���ĵ�����
    void deleteDocument(int docId) {
        // ��ѯ�ļ���
        stmt.str("");
        stmt << "SELECT doc_name FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        if (!row) {
            freeResult(); // �ͷŽ����
            return;
        }

        QString docName = QString::fromUtf8(row[0]);

        // ��ȡ��ĿID��������ݿ��ѯ������document����project_id�ֶΣ�
        stmt.str("");
        stmt << "SELECT project_id FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        if (!row) {
            freeResult(); // �ͷŽ����
            return;
        }
        int projectId = atoi(row[0]);

        // ɾ�������ļ���·��Ϊuploads/��ĿID/�ļ�����
        deletePhysicalFile(projectId, docName);

        // ɾ�����ݿ��¼
        stmt.str("");
        stmt << "DELETE FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        freeResult(); // �ͷŽ����
    }

private:
    // ����ļ��Ƿ���ڣ������ĵ�ID
    int getDocumentId(int projectId, const QString& docName) {
        stmt.str("");
        stmt << "SELECT doc_id FROM document WHERE project_id=" << projectId
            << " AND doc_name='" << docName.toStdString() << "'";
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int docId = row ? atoi(row[0]) : 0;
        freeResult(); // �ͷŽ����
        return docId;
    }

    // ��ȡ�ĵ��汾��
    int getDocumentVersion(int docId) {
        stmt.str("");
        stmt << "SELECT version FROM document WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        row = mysql_fetch_row(res_set);
        int version = row ? atoi(row[0]) : 0;
        freeResult(); // �ͷŽ����
        return version;
    }

    // �����ĵ��汾��
    void updateDocumentVersion(int docId, int newVersion) {
        stmt.str("");
        stmt << "UPDATE document SET version=" << newVersion
            << ", modify_time=NOW() WHERE doc_id=" << docId;
        executeQuery(stmt.str());
        freeResult(); // �ͷŽ����
    }

    // ɾ�������ļ�
    void deletePhysicalFile(int projectId, const QString& docName) {
        QDir uploadDir(QCoreApplication::applicationDirPath());
        uploadDir.cd(QString("uploads/%1").arg(projectId)); // ������ĿID��Ŀ¼
        QFile file(uploadDir.filePath(docName));
        file.remove();
    }

    string calculateHash(const string& content) {
        return "HASH-" + to_string(rand() % 1000000);
    }

    void executeQuery(const string& q) {
        freeResult(); // ���ͷ�֮ǰ�Ľ����
        /*if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }*/
        // ��һ�γ���ִ�в�ѯ
        if (mysql_query(conn, q.c_str())) {
            // ����Ƿ��������ѶϿ��Ĵ���
            unsigned int err_no = mysql_errno(conn);
            if (err_no == CR_SERVER_GONE_ERROR || err_no == CR_SERVER_LOST) {
                QMessageBox::warning(nullptr, u8"�����ж�", u8"���ݿ������ѶϿ������ڳ�����������...");

                // ������������
                if (mysql_real_connect(conn, HOST, USER, PASS, DATABASE, PORT, nullptr, 0)) {
                    QMessageBox::information(nullptr, u8"�����ɹ�", u8"���������ӵ����ݿ⣬�����ԸղŵĲ�����");

                    // �����ɹ����ٴ�ִ��֮ǰ�Ĳ�ѯ
                    if (mysql_query(conn, q.c_str())) {
                        // ������Ժ���Ȼʧ�ܣ��򱨸����մ���
                        QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
                    }
                    else {
                        res_set = mysql_store_result(conn);
                    }

                }
                else {
                    QMessageBox::critical(nullptr, u8"����ʧ��", u8"�޷��������ӵ����ݿ⣬������������ݿ������״̬��");
                    // ����ѡ���˳�����
                    // exit(1);
                }
            }
            else {
                // ������������͵� SQL ����ֱ�ӱ���
                QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
            }
        }
        else {
            // �״�ִ�гɹ�
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

// ��ק�ļ��ϴ�����
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
        layout->setAlignment(Qt::AlignmentFlag::AlignCenter); // ��ѡ�����ò��ֶ��뷽ʽ

        QLabel* infoLabel = new QLabel(u8"��ק�ļ����˴��ϴ�", this);
        infoLabel->setAlignment(Qt::AlignCenter); // ֱ��ʹ�� Qt::AlignCenter
        infoLabel->setStyleSheet("font-size: 14px; color: #555;");
        layout->addWidget(infoLabel);
    }

    void uploadFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, u8"�ļ���ʧ��", u8"�޷����ļ�: " + filePath);
            return;
        }

        // �����ϴ�Ŀ¼·��
        QDir uploadBaseDir(QCoreApplication::applicationDirPath());

        // ֱ�Ӵ�����Ŀ��Ŀ¼������uploads/��ĿID��
        QString projectDirPath = QString("uploads/%1").arg(currentProjectId);
        if (!uploadBaseDir.mkpath(projectDirPath)) {
            QMessageBox::critical(this, u8"����", u8"�޷������ϴ�Ŀ¼");
            return;
        }

        QDir uploadDir(uploadBaseDir.filePath(projectDirPath));
        QString originalFileName = QFileInfo(filePath).fileName();
        QString uploadPath = uploadDir.filePath(originalFileName);

        bool overwrite = false;
        if (uploadDir.exists(originalFileName)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, u8"�ļ��Ѵ���",
                u8"�ļ� " + originalFileName + u8" �Ѵ��ڣ��Ƿ񸲸ǣ�",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply != QMessageBox::Yes) {
                QMessageBox::information(this, u8"�ϴ�ȡ��", u8"�ļ��ϴ���ȡ��");
                return;
            }
            overwrite = true;
            if (!QFile::remove(uploadPath)) {
                QMessageBox::critical(this, u8"ɾ��ʧ��", u8"�޷�ɾ�����ļ�");
                return;
            }
        }

        if (!QFile::copy(filePath, uploadPath)) {
            QMessageBox::critical(this, u8"�ϴ�ʧ��", u8"�޷������ļ����ϴ�Ŀ¼");
            return;
        }

        if (uploadCallback) {
            uploadCallback(originalFileName, currentProjectId, overwrite);
        }
        emit fileUploaded(originalFileName);
        QMessageBox::information(this, u8"�ϴ��ɹ�", overwrite ? u8"�ļ��Ѹ���" : u8"�ļ����ϴ�");
    }

private:
    function<void(const QString&, int, bool)> uploadCallback;
    int currentProjectId = 0;
};

// ���ֶ�̬����ģ�飨�Ƴ�������ظ��
class ScoreAnalyzer {
public:
    // ¼�����֣�ʹ��projectId���taskId
    void enterScore(ScoreInfo score) {
        if (score.score < 0 || score.score > 100) {
            QMessageBox::critical(nullptr, u8"����", u8"���ֱ�����0.00-100.00֮��");
            return;
        }
        stmt.str("");
        // ��ʽָ���ֶΣ�ʹ��fixed��ʽ����֤��λС��
        stmt << "INSERT INTO score (user_id, project_id, score) "
            << "VALUES (" << score.userId << ", " << score.projectId << ", "
            << fixed << setprecision(2) << score.score << ")"; // ������ʽ�����
        executeQuery(stmt.str());
        freeResult(); // �ͷŽ����
        QMessageBox::information(nullptr, u8"��ʾ", u8"����¼��ɹ�");
    }

    // �������ּ�Ч���棬����ĿIDͳ���û�����
    void generateReport(int projectId) {
        QMessageBox infoBox;
        infoBox.setWindowTitle(u8"��Ч����");
        QString output = QString(u8"=== ��Ŀ %1 ��Ч���� ===\n").arg(projectId);

        stmt.str("");
        stmt << "SELECT user_id, score, create_time "
            << "FROM `score` "
            << "WHERE project_id=" << projectId;

        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"��ѯʧ��", QString::fromUtf8(mysql_error(conn)));
            return;
        }

        MYSQL_RES* result = mysql_store_result(conn);
        if (!result) {
            QMessageBox::critical(nullptr, u8"����", u8"�޷���ȡ��������");
            return;
        }

        // ��ӱ�ͷ
        output += u8"�û�ID\t����\t�ύʱ��\n";

        // ���������
        while ((row = mysql_fetch_row(result))) {
            // �����ֶ�ֵ��ע�⣺��ȷ���ֶδ��ڣ�
            QString userId = row[0] ? QString::fromUtf8(row[0]) : "δ֪";
            QString score = row[1] ? QString::fromUtf8(row[1]) : "0.00";
            QString time = row[2] ? QString::fromUtf8(row[2]) : "δ֪ʱ��";

            output += QString("%1\t%2\t%3\n").arg(userId).arg(score).arg(time);
        }

        // �ͷŽ����
        mysql_free_result(result);
        result = nullptr;

        infoBox.setText(output);
        infoBox.exec();
    }

    // ��ȡ��ʷ���ּ�¼
    vector<ScoreInfo> getScoreHistory(int projectId) {
        vector<ScoreInfo> scores;
        stmt.str("");
        stmt << "SELECT score_id, user_id, score, create_time "
            << "FROM score "
            << "WHERE project_id=" << projectId
            << " ORDER BY create_time DESC";

        // �޸�����ӽ��������
        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"��ѯʧ��", QString::fromUtf8(mysql_error(conn)));
            return scores;
        }

        MYSQL_RES* result = mysql_store_result(conn);  // ��ȡ�����
        if (!result) {
            QMessageBox::critical(nullptr, u8"����", u8"�޷���ȡ���ּ�¼");
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

        // �ͷŽ����
        mysql_free_result(result);
        result = nullptr;

        return scores;
    }

    // ɾ�����ּ�¼
    void deleteScore(int scoreId) {
        stmt.str("");
        stmt << "DELETE FROM score WHERE score_id=" << scoreId;
        executeQuery(stmt.str());

        if (mysql_affected_rows(conn) > 0) {
            QMessageBox::information(nullptr, u8"ɾ���ɹ�", u8"���ּ�¼��ɾ��");
        }
        else {
            QMessageBox::warning(nullptr, u8"ɾ��ʧ��", u8"δ�ҵ���Ӧ�����ּ�¼");
        }
        freeResult(); // �ͷŽ����
    }

    // ��ȡ��������
    vector<AverageScore> getScoreRanking() {
        int tot = 0;
        map<int, int> f; //��projectIdӳ�䵽0-tot-1
        map<pair<int, int>, int> mp; //ȥ�أ�ȷ��ÿ���û���ÿ����Ŀֻ��һ������
        vector<AverageScore> ave;

        stmt.str("");
        stmt << "SELECT score_id, user_id, score, create_time, project_id "
            << "FROM score "
            << "ORDER BY create_time DESC";

        // �޸�����ӽ��������
        if (mysql_query(conn, stmt.str().c_str())) {
            QMessageBox::critical(nullptr, u8"��ѯʧ��", QString::fromUtf8(mysql_error(conn)));
            return ave;
        }

        MYSQL_RES* result = mysql_store_result(conn);  // ��ȡ�����
        if (!result) {
            QMessageBox::critical(nullptr, u8"����", u8"�޷���ȡ���ּ�¼");
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
            if (mp.find(key) == mp.end()) { // ֻͳ��ÿ���û���ÿ����Ŀ����������
                mp[key] = 1;
                ave[f[info.projectId]].sum += info.score;
                ave[f[info.projectId]].n++;
            }
        }

        // �ͷŽ����
        mysql_free_result(result);
        result = nullptr;

        for (int i = 0; i < tot; i++) {
            if (ave[i].n > 0) {
                ave[i].aveScore = ave[i].sum / ave[i].n;
            }
            else {
                ave[i].aveScore = 0.0; // ���������
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
        freeResult(); // ���ͷ�֮ǰ�Ľ����
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
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

// GitHub����ģ�飨֧����Ŀ��֧�͸��£�
// GitHub����ģ�飨�����Ż��棬���޸��������
class GitHubIntegration {
public:
    GitHubIntegration() {
        gitPath = QStandardPaths::findExecutable("git");
        if (gitPath.isEmpty()) {
            QMessageBox::critical(nullptr, u8"����",
                u8"δ��ϵͳ���ҵ�git.exe����ȷ��Git�Ѱ�װ���ѽ���·����ӵ�ϵͳPATH���������С�");
        }
    }

    void syncProjectToGitHub(int projectId, const QString& remoteUrl) {
        if (gitPath.isEmpty()) return;

        QString projectFilesPath = QCoreApplication::applicationDirPath() + "/uploads/" + QString::number(projectId);
        QString gitRepoPath = QCoreApplication::applicationDirPath() + "/git_repos/" + QString::number(projectId);
        QString branchName = "project/" + QString::number(projectId);

        QDir projectDir(projectFilesPath);
        if (!projectDir.exists() || projectDir.isEmpty()) {
            QMessageBox::critical(nullptr, u8"����", u8"��Ŀ�ļ�Ŀ¼�����ڻ�Ϊ��: " + projectFilesPath);
            return;
        }

        if (!prepareLocalRepository(gitRepoPath, remoteUrl)) return;
        if (!checkoutProjectBranch(gitRepoPath, branchName)) return;
        if (!syncFilesToRepo(projectFilesPath, gitRepoPath)) return;

        // --- Add, Commit, Push ���� ---
        auto addResult = runGitCommand({ "add", "." }, gitRepoPath);
        if (!addResult.first) {
            showErrorDialog("Git Add ʧ��", addResult.second);
            return;
        }

        QString commitMessage = QString("Update project %1 at %2").arg(projectId).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        auto commitResult = runGitCommand({ "commit", "-m", commitMessage }, gitRepoPath);
        if (!commitResult.first) {
            if (commitResult.second.contains("nothing to commit") || commitResult.second.contains(u8"���ļ�Ҫ�ύ")) {
                QMessageBox::information(nullptr, u8"��ʾ", u8"�����ļ���ֿ��е����°汾һ�£�������¡�");
                return;
            }
            showErrorDialog("Git Commit ʧ��", commitResult.second);
            return;
        }

        auto pushResult = runGitCommand({ "push", "--set-upstream", "origin", branchName }, gitRepoPath);
        if (!pushResult.first) {
            showErrorDialog("Git Push ʧ��", pushResult.second);
            return;
        }

        QMessageBox::information(nullptr, u8"�ɹ�", QString(u8"��Ŀ %1 �ѳɹ�ͬ������֧ '%2'��\n%3").arg(projectId).arg(branchName).arg(pushResult.second));
        saveRepoUrlToDatabase(projectId, remoteUrl);
    }

private:
    QString gitPath;

    // ������� runGitCommand
    QPair<bool, QString> runGitCommand(const QStringList& args, const QString& workingDir) {
        QProcess process; // �ں����ڲ������ֲ� QProcess ����
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
        msgBox.setText(u8"Git����ִ��ʧ�ܡ�");
        msgBox.setDetailedText(gitOutput);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }

    bool prepareLocalRepository(const QString& repoPath, const QString& remoteUrl) {
        QDir repoDir(repoPath);
        if (repoDir.exists()) {
            auto fetchResult = runGitCommand({ "fetch", "origin" }, repoPath);
            if (!fetchResult.first) {
                showErrorDialog("Git Fetch ʧ��", fetchResult.second);
                return false;
            }
            runGitCommand({ "remote", "set-url", "origin", remoteUrl }, repoPath);
        }
        else {
            auto cloneResult = runGitCommand({ "clone", remoteUrl, repoPath }, QCoreApplication::applicationDirPath());
            if (!cloneResult.first) {
                showErrorDialog("Git Clone ʧ��", cloneResult.second);
                return false;
            }
        }
        return true;
    }

    bool checkoutProjectBranch(const QString& repoPath, const QString& branchName) {
        auto checkoutResult = runGitCommand({ "checkout", branchName }, repoPath);
        if (checkoutResult.first) {
            auto pullResult = runGitCommand({ "pull", "origin", branchName }, repoPath);
            if (!pullResult.first && !pullResult.second.contains("Already up to date") && !pullResult.second.contains(u8"�Ѿ�������")) {
                showErrorDialog("Git Pull ʧ��", pullResult.second);
                // ��ʹpullʧ�ܣ�Ҳ��һ����Ҫ��ֹ�����Լ������ԣ���push�������Ƿ��ͻ
                // return false; 
            }
            return true;
        }

        auto createBranchResult = runGitCommand({ "checkout", "-b", branchName }, repoPath);
        if (!createBranchResult.first) {
            showErrorDialog("������֧ʧ��", createBranchResult.second);
            return false;
        }
        return true;
    }

    // (��ȷ�����Ѿ�������ĺ���)
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
                QDir(dstPath).mkpath("."); // ������Ŀ¼
                // ע�⣺������Ҫһ���ݹ麯����������Ŀ¼���ݣ�Ϊ�򻯣�����Ŀǰֻ��һ��
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
            QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
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

// �ļ��б�����
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
        QString docName = item->text().split(" (v")[0]; // �Ƴ��汾�Ų���

        // ȷ��·���������ϴ�ʱһ��
        QDir uploadDir(QCoreApplication::applicationDirPath());
        if (!uploadDir.cd("uploads/" + QString::number(projectId))) {
            QMessageBox::warning(this, "����", "�޷��ҵ���ĿĿ¼");
            return;
        }

        QString filePath = uploadDir.filePath(docName);

        if (!QFile::exists(filePath)) {
            QMessageBox::warning(this, "����", "�ļ�������: " + filePath);
            return;
        }

        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            QMessageBox::warning(this, "����", "�޷��򿪸��ļ�: " + filePath);
        }
    }

    void onDeleteFile() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item) return;

        if (QMessageBox::question(this, "ȷ��ɾ��", "ȷ��Ҫɾ�����ļ���",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            int docId = item->data(Qt::UserRole).toInt();
            DocumentManager dm;
            dm.deleteDocument(docId);
            delete item;
            QMessageBox::information(this, "ɾ���ɹ�", "�ļ��ѳɹ�ɾ��");
        }
    }

private:
    void setupUI() {
        setWindowTitle("���ϴ��ļ��б�");
        setGeometry(300, 300, 500, 400);

        QVBoxLayout* layout = new QVBoxLayout(this);

        listWidget = new QListWidget();
        layout->addWidget(listWidget);

        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* openBtn = new QPushButton("���ļ�");
        QPushButton* deleteBtn = new QPushButton("ɾ���ļ�");

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
            // ��ȡ�ļ��汾��
            stmt.str("");
            stmt << "SELECT version FROM document WHERE doc_id=" << doc.first;
            executeQuery(stmt.str());
            row = mysql_fetch_row(res_set);
            int version = row ? atoi(row[0]) : 1;
            freeResult(); // �ͷŽ����

            // ֻʹ��ԭʼ�ļ�����������·��
            QString fileName = QFileInfo(doc.second).fileName();

            QListWidgetItem* item = new QListWidgetItem(fileName + " (v" + QString::number(version) + ")");
            item->setData(Qt::UserRole, doc.first);
            item->setData(Qt::UserRole + 1, fileName); // �洢ԭʼ�ļ���
            listWidget->addItem(item);
        }
    }

    void executeQuery(const string& q) {
        freeResult(); // ���ͷ�֮ǰ�Ľ����
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
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

// �����б�����
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
        if (QMessageBox::question(this, "ȷ��ɾ��", "ȷ��Ҫɾ���������ּ�¼��",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            ScoreAnalyzer sa;
            sa.deleteScore(scoreId);
            delete item;
        }
    }

private:
    void setupUI() {
        setWindowTitle("��ʷ���ּ�¼");
        setGeometry(300, 300, 600, 400);

        QVBoxLayout* layout = new QVBoxLayout(this);

        listWidget = new QListWidget();
        layout->addWidget(listWidget);

        QPushButton* deleteBtn = new QPushButton("ɾ��ѡ������");
        layout->addWidget(deleteBtn);

        connect(deleteBtn, &QPushButton::clicked, this, &ScoreListWindow::onDeleteScore);
    }

    void loadScores() {
        ScoreAnalyzer sa;
        auto scores = sa.getScoreHistory(projectId);

        // ���û�л�ȡ�����ּ�¼��������ʾ
        if (scores.empty()) {
            QMessageBox::information(this, "��ʾ", "δ�ҵ�����Ŀ�����ּ�¼��");
            return;
        }

        // �����ʽ���ַ�����������ߴ����ά����
        const QString itemFormat = "ʱ��: %1 | ����: %2 | �û�ID: %3";

        foreach(auto & score, scores) {
            // ��ʽ����ʾ��ʱ��-����-�û�ID��ʵ��Ӧ����Ӧ�滻Ϊ�û�����
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

        // ���û�л�ȡ�����ּ�¼��������ʾ
        if (scores.empty()) {
            QMessageBox::information(this, "��ʾ", "δ�ҵ����ּ�¼��");
            return;
        }

        // �����ʽ���ַ�����������ߴ����ά����
        const QString itemFormat = "����: %1 | ��Ŀ: %2 | ƽ����: %3 | ��������: %4";

        foreach(auto & score, scores) {
            // ��ʽ����ʾ������-��Ŀ-ƽ����-��������
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

// �ĵ�������
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
        setWindowTitle("�ĵ��ϴ�");
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

// �� MainSystemWidget ��Ķ���֮ǰ���������µĶԻ�����

class DocumentMenuDialog : public QDialog {
    Q_OBJECT
public:
    DocumentMenuDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(u8"�ĵ�����ģ��");
        setModal(true); // ����Ϊģ̬�Ի���
        setupUI();
    }

private slots:
    void onUploadClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"������ĿID", u8"������Ҫ�ϴ��ĵ�����ĿID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        DocumentManagementWindow docUploadWindow(this);
        docUploadWindow.setProjectId(projectId);
        docUploadWindow.exec();
        accept(); // ��ɲ�����ر��Ӳ˵�
    }

    void onHistoryClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"������ĿID", u8"������Ҫ�鿴�汾��ʷ����ĿID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        DocumentManager dm;
        auto history = dm.getVersionHistoryByProject(projectId);
        if (history.empty()) {
            QMessageBox::information(this, u8"��ʾ", u8"����Ŀ�����ĵ��汾��¼��");
            return;
        }

        map<int, vector<VersionInfo>> docVersions;
        for (const auto& vi : history) {
            docVersions[vi.docId].push_back(vi);
        }

        QString output = u8"��ĿID��" + QString::number(projectId) + u8" �ĵ��汾��ʷ��\n";
        for (const auto& pair : docVersions) {
            output += u8"\n�ĵ����ƣ�" + pair.second.front().docName + u8"\n";
            for (const auto& v : pair.second) {
                output += QString(u8"  �汾 %1\n").arg(v.versionNumber);
            }
        }
        QMessageBox::information(this, u8"��Ŀ�汾��ʷ", output);
        accept();
    }

    void onListFilesClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"������ĿID", u8"������Ҫ�鿴�ļ�����ĿID��", 1, 1, INT_MAX, 1, &ok);
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

        QPushButton* uploadButton = new QPushButton(u8"�ϴ����ĵ�");
        QPushButton* historyButton = new QPushButton(u8"�鿴�汾��ʷ");
        QPushButton* listFilesButton = new QPushButton(u8"�������ϴ��ļ�");

        // Ӧ�������������Ƶ���ʽ������ɫ��������
        QString subMenuStyle = R"(
            QPushButton {
                background-color: #3498db; /* ��һ����ɫ */
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

        layout->addWidget(new QLabel(u8"��ѡ��һ�������"));
        layout->addWidget(uploadButton);
        layout->addWidget(historyButton);
        layout->addWidget(listFilesButton);

        connect(uploadButton, &QPushButton::clicked, this, &DocumentMenuDialog::onUploadClicked);
        connect(historyButton, &QPushButton::clicked, this, &DocumentMenuDialog::onHistoryClicked);
        connect(listFilesButton, &QPushButton::clicked, this, &DocumentMenuDialog::onListFilesClicked);
    }
};

// �� DocumentMenuDialog ��Ķ���֮���������µĶԻ�����

class ScoreMenuDialog : public QDialog {
    Q_OBJECT
public:
    ScoreMenuDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(u8"��Ч����ģ��");
        setModal(true);
        setupUI();
    }

private slots:
    void onEnterScoreClicked() {
        ScoreAnalyzer sa;
        ScoreInfo score;
        bool ok;

        score.userId = QInputDialog::getInt(this, u8"�����û�ID", u8"�����������û�ID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        score.projectId = QInputDialog::getInt(this, u8"������ĿID", u8"��������Ҫ���ֵ���ĿID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        score.score = QInputDialog::getDouble(this, u8"��������", u8"���������֣�0-100����", 50.0, 0, 100, 2, &ok);
        if (!ok) return;

        sa.enterScore(score);
        sa.generateReport(score.projectId);
        accept();
    }

    void onViewHistoryClicked() {
        bool ok;
        int projectId = QInputDialog::getInt(this, u8"������ĿID", u8"������Ҫ�鿴������ʷ����ĿID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;
        ScoreListWindow scoreWindow(projectId, this);
        scoreWindow.exec();
        accept();
    }

    void onRankingClicked() {
        ScoreListWindow scoreWindow(this); // �����޲ι��캯������ʾ����
        scoreWindow.exec();
        accept();
    }

private:
    void setupUI() {
        this->setMinimumWidth(400);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setSpacing(15);
        layout->setContentsMargins(20, 20, 20, 20);

        QPushButton* enterScoreButton = new QPushButton(u8"¼������");
        QPushButton* viewHistoryButton = new QPushButton(u8"�鿴��Ŀ������ʷ");
        QPushButton* rankingButton = new QPushButton(u8"�鿴������Ŀ����");

        QString subMenuStyle = R"(
            QPushButton {
                background-color: #2ecc71; /* ��ɫϵ */
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

        layout->addWidget(new QLabel(u8"��ѡ��һ�������"));
        layout->addWidget(enterScoreButton);
        layout->addWidget(viewHistoryButton);
        layout->addWidget(rankingButton);

        connect(enterScoreButton, &QPushButton::clicked, this, &ScoreMenuDialog::onEnterScoreClicked);
        connect(viewHistoryButton, &QPushButton::clicked, this, &ScoreMenuDialog::onViewHistoryClicked);
        connect(rankingButton, &QPushButton::clicked, this, &ScoreMenuDialog::onRankingClicked);
    }
};

// ��ϵͳ����
class MainSystemWidget : public QWidget {
    Q_OBJECT
public:
    MainSystemWidget(QWidget* parent = nullptr) : QWidget(parent) {
        initDatabase();
        setupUI();
    }

    void executeQuery(const string& q) {
        freeResult(); // ���ͷ�֮ǰ�Ľ����
        if (mysql_query(conn, q.c_str())) {
            QMessageBox::critical(nullptr, u8"���ݿ����ʧ��", QString::fromUtf8(mysql_error(conn)));
        }
        else {
            res_set = mysql_store_result(conn);
        }
    }

private:
    void initDatabase() {
        conn = mysql_init(nullptr);
        if (!conn || !mysql_real_connect(conn, HOST, USER, PASS, DATABASE, PORT, nullptr, 0)) {
            QMessageBox::critical(nullptr, u8"���ݿ�����ʧ��", QString::fromUtf8(mysql_error(conn)));
            exit(1);
        }
        QMessageBox::information(nullptr, u8"��ʾ", u8"���ݿ����ӳɹ�");
    }

    void setupUI() {
        // --- 1. ���岼�ֺͱ���ɫ ---
        this->setStyleSheet("background-color: #f5f5f7;"); // ����һ����͵�ǳ��ɫ����
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(50, 40, 50, 40); // ���ӱ߾࣬�����ݺ���
        mainLayout->setSpacing(30); // ���ӿؼ�֮��ļ��

        // --- 2. ��ӱ����ͼ�� ---
        QHBoxLayout* titleLayout = new QHBoxLayout();
        // ����Ҫ����Ŀ��Դ�ļ�(.qrc)�����һ��ͼ���ļ������� icon.png
        // ���û�У�������ע�͵���������
        // QLabel* iconLabel = new QLabel();
        // iconLabel->setPixmap(QPixmap(":/icons/icon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel* titleLabel = new QLabel(u8"��ĿЭͬ����ϵͳ");
        titleLabel->setStyleSheet(
            "font-size: 32px;"
            "font-weight: bold;"
            "color: #333;"
        );

        // titleLayout->addWidget(iconLabel); // �����ͼ����ȡ��ע��
        titleLayout->addWidget(titleLabel);
        titleLayout->addStretch(); // �������Ƶ����

        // --- 3. �������ܰ�ť��ʹ�����񲼾ָ��� ---
        QGridLayout* buttonLayout = new QGridLayout();
        buttonLayout->setSpacing(20);

        QPushButton* docManagementButton = new QPushButton(u8"�ĵ�����");
        QPushButton* scoreManagementButton = new QPushButton(u8"��Ч����");
        QPushButton* gitHubIntegrationButton = new QPushButton(u8"����ͬ��");
        QPushButton* exitButton = new QPushButton(u8"�˳�ϵͳ");

        // ���ĸ���ť�������񲼾�
        buttonLayout->addWidget(docManagementButton, 0, 0);
        buttonLayout->addWidget(scoreManagementButton, 0, 1);
        buttonLayout->addWidget(gitHubIntegrationButton, 1, 0);
        buttonLayout->addWidget(exitButton, 1, 1);

        // --- 4. ͳһ���ð�ť��ʽ (���Ĳ���) ---
        QString buttonStyle = R"(
            QPushButton {
                background-color: #007aff; /* ƻ���� */
                color: white;
                font-size: 18px;
                font-weight: bold;
                border-radius: 12px;
                padding: 15px;
                min-height: 80px; /* ��֤��ť���㹻�ĸ߶� */
                border: none;
            }
            QPushButton:hover {
                background-color: #005ecb; /* ��ͣʱ��ɫ���� */
            }
            QPushButton:pressed {
                background-color: #004a9e; /* ���ʱ��ɫ���� */
            }
        )";

        // Ϊÿ����ťӦ����ʽ
        docManagementButton->setStyleSheet(buttonStyle);
        scoreManagementButton->setStyleSheet(buttonStyle);
        gitHubIntegrationButton->setStyleSheet(buttonStyle);

        // �˳���ť��������һ����ͬ����ɫ����ʾ����
        exitButton->setStyleSheet(R"(
            QPushButton {
                background-color: #e74c3c; /* ��ɫ����ʾΣ�ջ��˳����� */
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

        // --- 5. �����в�����ӵ��������� ---
        mainLayout->addLayout(titleLayout);
        mainLayout->addStretch(1); // ���һ��������Ѱ�ť������
        mainLayout->addLayout(buttonLayout);
        mainLayout->addStretch(2); // �ڵײ����һ�������������

        // --- 6. �����źźͲ� (���ֲ���) ---
        connect(docManagementButton, &QPushButton::clicked, this, &MainSystemWidget::handleDocumentManagement);
        connect(scoreManagementButton, &QPushButton::clicked, this, &MainSystemWidget::handleScoreManagement);
        connect(gitHubIntegrationButton, &QPushButton::clicked, this, &MainSystemWidget::handleGitHubIntegration);
        connect(exitButton, &QPushButton::clicked, this, &MainSystemWidget::exitSystem);
    }

    void handleDocumentManagement() {
        DocumentMenuDialog dialog(this);
        dialog.exec(); // ��ʾ�ĵ������Ӳ˵������ȴ����ر�
    }

    void handleScoreManagement() {
        ScoreMenuDialog dialog(this);
        dialog.exec(); // ��ʾ���ֹ����Ӳ˵������ȴ����ر�
    }

    void handleGitHubIntegration() {
        GitHubIntegration git;
        int projectId;

        QMessageBox::information(nullptr, u8"��ʾ", u8"--- GitHub���� ---");
        bool ok;
        projectId = QInputDialog::getInt(this, u8"��������ĿID", u8"������Ҫͬ����GitHub����ĿID��", 1, 1, INT_MAX, 1, &ok);
        if (!ok) return;

        // �����Ŀ�ļ�Ŀ¼�Ƿ����
        QString projectPath = QCoreApplication::applicationDirPath() + "/uploads/" + QString::number(projectId);
        if (!QDir(projectPath).exists()) {
            QMessageBox::warning(this, u8"����", u8"��ĿID " + QString::number(projectId) + u8" ���ļ�Ŀ¼�����ڣ������ϴ��ļ���");
            return;
        }

        // �����Ի������û�����Զ�ֿ̲�URL
        QString remoteUrl = QInputDialog::getText(this, u8"����GitHub�ֿ�URL",
            u8"��������Ŀ������Զ��Git�ֿ�URL��\n"
            u8"(�״�ʹ��ʱ����ȷ���òֿ�����GitHub�ϴ���)\n"
            u8"����: https://github.com/your-username/your-repo.git",
            QLineEdit::Normal, "", &ok);

        if (!ok || remoteUrl.isEmpty()) {
            QMessageBox::warning(this, u8"����ȡ��", u8"δ����URL��������ȡ����");
            return;
        }

        // �����µ�ͬ������
        git.syncProjectToGitHub(projectId, remoteUrl);
    }

    void exitSystem() {
        mysql_close(conn);
        QMessageBox::information(nullptr, u8"��ʾ", u8"ϵͳ���˳�");
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
    window.setWindowTitle(u8"��Ŀ����ϵͳ");
    window.resize(800, 600);

    MainSystemWidget* mainWidget = new MainSystemWidget(&window);
    window.setCentralWidget(mainWidget);

    window.show();

    return app.exec();
}

#include "main.moc"