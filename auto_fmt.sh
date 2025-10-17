#!/bin/bash

# ====================================================
# 自动遍历当前目录及子目录，统一换行符为 LF 并执行 clang-format
# 兼容 bash 和 zsh
# ====================================================

# 检查 dos2unix 和 clang-format 是否存在
command -v clang-format >/dev/null 2>&1 || { echo "[错误] 未找到 clang-format"; exit 1; }
command -v dos2unix >/dev/null 2>&1 || echo "[警告] 未找到 dos2unix, 将使用 sed 替代"

# 遍历文件类型
find_files() {
    find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \)
}

# 主处理函数
process_file() {
    local file="$1"
    echo "处理文件: $file"

    # 转换 CRLF -> LF
    if command -v dos2unix >/dev/null 2>&1; then
        dos2unix "$file" 2>/dev/null
    else
        sed -i 's/\r$//' "$file"
    fi

    # clang-format 格式化
    clang-format -i "$file"
}

# 遍历所有文件
while IFS= read -r file; do
    process_file "$file"
done < <(find_files)

echo "所有文件处理完成"
