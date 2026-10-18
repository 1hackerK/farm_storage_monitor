<#
ESP32 项目一键提交推送到Github脚本
#>
# ========== 这里改成你自己的信息 ==========
$gitUserName="1hackerK"
$repoName="farm_storage_monitor"
$commitMsg="feat: 粮仓环境监测ESP32项目初始提交"
# ==========================================

# 切换到脚本所在项目目录
$projPath = $PSScriptRoot
Set-Location $projPath
Write-Host "当前项目目录：$projPath`n" -ForegroundColor Cyan

# 1. 创建ESP-IDF专用.gitignore
$gitignoreContent = @"
# ESP-IDF编译产物
build/
sdkconfig
sdkconfig.old
*.elf
*.bin
*.map
*.flash_args
partition_table/partition-table.bin
bootloader/bootloader.bin

# 组件自动拉取缓存
managed_components/

# VSCode配置（如需保留.vscode删掉下面这一行）
.vscode/
.devcontainer/

# 工具生成文件
compile_commands.json

# Python缓存
__pycache__/
*.pyc
*.pyo

# 系统文件
Thumbs.db
.DS_Store
"@
# 写入.gitignore
$gitignorePath = Join-Path $projPath ".gitignore"
$gitignoreContent | Out-File $gitignorePath -Encoding utf8
Write-Host "✅ 已生成 .gitignore 文件`n" -ForegroundColor Green

# 2. 初始化git（如果还没init）
if(-not (Test-Path ".git")){
    git init
    Write-Host "✅ git仓库初始化完成`n" -ForegroundColor Green
}else{
    Write-Host "ℹ️ git仓库已存在，跳过init`n" -ForegroundColor Yellow
}

# 3. 清空暂存区，防止build目录被提交
git reset HEAD
Write-Host "✅ 清空暂存区`n" -ForegroundColor Green

# 4. 添加所有文件（.gitignore会自动过滤build、managed_components等）
git add .
Write-Host "✅ git add . 完成`n" -ForegroundColor Green

# 5. 提交代码
git commit -m "$commitMsg"
Write-Host "✅ 本地代码提交完成`n" -ForegroundColor Green

# 6. 关联远程仓库，如果已存在origin则先删除
git remote remove origin 2>&1 | Out-Null
git remote add origin "https://github.com/$gitUserName/$repoName.git"
Write-Host "✅ 关联远程仓库 https://github.com/$gitUserName/$repoName.git`n" -ForegroundColor Green

# 7. 推送到远程main分支
git push -u origin main
Write-Host "🚀 推送完成！打开github网页查看仓库" -ForegroundColor Green
