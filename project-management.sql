CREATE DATABASE IF NOT EXISTS projectManagement;
USE projectManagement;

-- 文档表
CREATE TABLE document (
    doc_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    doc_name VARCHAR(100) NOT NULL,
    version INT(4) NOT NULL DEFAULT 1,
    tags TEXT NOT NULL,
    hash_code VARCHAR(32) NOT NULL,
    project_id INT(11) NOT NULL,
    modify_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP -- 新增：自动更新修改时间
);
-- 成绩表
CREATE TABLE score (
    score_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    user_id INT(11) NOT NULL,
    project_id INT(11) NOT NULL,
    score DECIMAL(5, 2) NOT NULL DEFAULT 0.00,
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- GitHub 仓库表
CREATE TABLE github_repo (
    repo_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    project_id INT(11) NOT NULL,
    repo_url VARCHAR(200) NOT NULL,
    permission ENUM('public', 'private') NOT NULL DEFAULT 'public',
    sync_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 项目编号与主题
CREATE TABLE project_themes (
    id INT(11) NOT NULL  AUTO_INCREMENT PRIMARY KEY,
    project_id INT(11) NOT NULL UNIQUE,
    theme_id INT(11) NOT NULL,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);