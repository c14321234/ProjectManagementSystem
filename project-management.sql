CREATE DATABASE IF NOT EXISTS projectManagement;
USE projectManagement;

-- 用户表
CREATE TABLE user (
    user_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    role ENUM('admin', 'user', 'guest') NOT NULL DEFAULT 'user',
    password VARCHAR(64) NOT NULL
);

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

-- 任务表（如后续需要任务粒度的管理，可扩展）
CREATE TABLE task (
    task_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    task_name VARCHAR(100),
    project_id INT(11),
    weight INT(11) DEFAULT 1
);

-- GitHub 仓库表
CREATE TABLE github_repo (
    repo_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    project_id INT(11) NOT NULL,
    repo_url VARCHAR(200) NOT NULL,
    permission ENUM('public', 'private') NOT NULL DEFAULT 'public',
    sync_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 操作日志表（选做）
CREATE TABLE log (
    log_id INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
    user_id INT(11),
    action VARCHAR(255),
    timestamp DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
