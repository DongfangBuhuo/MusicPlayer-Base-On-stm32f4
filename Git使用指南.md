# Git 版本控制快速指南

## 🎉 恭喜！您的第一个Git提交已成功创建

**提交ID**: 8ff90bd
**提交信息**: 修复ES8388音乐播放杂音和速度问题 - 初始版本保存
**分支**: main

---

## 📚 Git 基本概念

### 什么是Git？
Git是一个版本控制系统，可以：
- 📸 保存项目的历史快照
- 🔄 回退到之前的任何版本
- 🌿 创建分支进行实验性开发
- 👥 团队协作开发

### Git工作流程
```
工作区 → 暂存区 → 本地仓库 → 远程仓库
  ↓        ↓         ↓          ↓
修改文件  git add  git commit  git push
```

---

## 🛠️ 常用Git命令

### 1. 查看状态
```bash
# 查看当前状态（哪些文件被修改）
git status

# 查看提交历史
git log

# 查看简洁的提交历史
git log --oneline

# 查看最近3次提交
git log --oneline -3
```

### 2. 保存更改（常用！）
```bash
# 第一步：添加修改的文件到暂存区
git add .                    # 添加所有修改的文件
git add Core/Src/main.c      # 只添加特定文件

# 第二步：提交到本地仓库
git commit -m "提交说明"

# 快捷方式：添加并提交所有已追踪的文件
git commit -am "提交说明"
```

### 3. 查看修改
```bash
# 查看工作区的修改（还未添加到暂存区）
git diff

# 查看暂存区的修改（已添加但未提交）
git diff --staged

# 查看某个文件的修改
git diff Core/Src/main.c
```

### 4. 撤销修改
```bash
# 撤销工作区的修改（危险！会丢失未保存的更改）
git checkout -- Core/Src/main.c

# 从暂存区移除文件（但保留工作区的修改）
git reset HEAD Core/Src/main.c

# 回退到上一次提交（保留工作区修改）
git reset HEAD~1

# 完全回退到某个提交（危险！会丢失之后的所有提交）
git reset --hard 8ff90bd
```

### 5. 分支管理
```bash
# 查看所有分支
git branch

# 创建新分支
git branch feature-new-codec

# 切换分支
git checkout feature-new-codec

# 创建并切换分支（快捷方式）
git checkout -b feature-new-codec

# 合并分支（将feature分支合并到main）
git checkout main
git merge feature-new-codec

# 删除分支
git branch -d feature-new-codec
```

---

## 💡 实际使用场景

### 场景1: 日常开发流程
```bash
# 1. 修改代码...

# 2. 查看修改了什么
git status
git diff

# 3. 添加修改
git add .

# 4. 提交
git commit -m "添加音量调节功能"

# 5. 查看历史
git log --oneline
```

### 场景2: 尝试新功能（创建分支）
```bash
# 1. 创建实验性分支
git checkout -b experiment-mp3-support

# 2. 在新分支上开发...
# 3. 提交更改
git add .
git commit -m "尝试添加MP3解码"

# 4. 如果成功，合并回main
git checkout main
git merge experiment-mp3-support

# 5. 如果失败，直接删除分支
git checkout main
git branch -d experiment-mp3-support
```

### 场景3: 回退到之前的版本
```bash
# 1. 查看历史提交
git log --oneline

# 2. 回退到指定提交（保留修改）
git reset 8ff90bd

# 3. 或完全回退（丢弃所有修改）
git reset --hard 8ff90bd
```

### 场景4: 查看某次修改的内容
```bash
# 查看某次提交修改了什么
git show 8ff90bd

# 查看某个文件的修改历史
git log -p Core/Src/main.c
```

---

## 🎯 您的下次提交示例

当您下次修改代码后，使用以下命令保存：

```bash
# 在项目目录下打开PowerShell/终端
cd d:\Stm32Cube\MusicPlayer

# 查看修改了什么
git status

# 添加所有修改
git add .

# 提交（写清楚做了什么修改）
git commit -m "添加音量调节功能"

# 查看提交历史
git log --oneline
```

---

## 📝 提交信息规范（建议）

好的提交信息应该清楚说明**做了什么**：

✅ **好的例子**：
```
git commit -m "修复ES8388初始化顺序错误"
git commit -m "添加WAV文件自动采样率检测"
git commit -m "优化DMA缓冲区大小为8KB"
git commit -m "添加音量调节API"
```

❌ **不好的例子**：
```
git commit -m "修改"
git commit -m "更新代码"
git commit -m "fix bug"
```

---

## 🔍 .gitignore 文件说明

我已经为您创建了 `.gitignore` 文件，它会自动排除：
- ✅ 编译输出文件（Debug/Release文件夹）
- ✅ 中间文件（.o, .d等）
- ✅ 临时文件（.tmp, .bak等）
- ✅ IDE配置文件（.settings, .cproject等）

这些文件不需要版本控制，因为它们会自动生成。

---

## 🌐 进阶：使用远程仓库（可选）

如果您想将代码备份到云端（如GitHub、Gitee等）：

### 1. 创建远程仓库
在GitHub或Gitee网站上创建一个新仓库

### 2. 关联远程仓库
```bash
git remote add origin https://github.com/你的用户名/MusicPlayer.git
```

### 3. 推送到远程
```bash
git push -u origin main
```

### 4. 以后的推送
```bash
git push
```

---

## ⚡ 快速参考卡片

| 命令 | 作用 |
|------|------|
| `git status` | 查看状态 |
| `git add .` | 添加所有修改 |
| `git commit -m "说明"` | 提交 |
| `git log --oneline` | 查看历史 |
| `git diff` | 查看修改 |
| `git checkout -b 分支名` | 创建并切换分支 |
| `git reset --soft HEAD~1` | 撤销上次提交（保留修改） |

---

## 🎓 学习资源

- **Git官方文档**: https://git-scm.com/doc
- **Git中文教程**: https://www.liaoxuefeng.com/wiki/896043488029600
- **图形化工具**: 
  - SourceTree (免费)
  - GitKraken (免费版)
  - VSCode内置Git功能

---

## 💾 当前项目状态

**仓库位置**: `d:\Stm32Cube\MusicPlayer`
**当前分支**: main
**已提交**: 1次
**最新提交**: 8ff90bd - 修复ES8388音乐播放杂音和速度问题

---

**作者**: Git Assistant
**日期**: 2025-11-26
**版本**: 1.0
