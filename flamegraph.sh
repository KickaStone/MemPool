#!/bin/bash

# 如果提示没有权限
# 方案1 sudo 执行本脚本
# 方案2 永久修改配置
# sudo echo "kernel.perf_event_paranoid = -1" >> /etc/sysctl.conf


# 确保脚本在当前工作目录执行
WORK_DIR=$(pwd)
FLAMEGRAPH_DIR="$WORK_DIR/FlameGraph"
OUTPUT_DIR="$WORK_DIR/output"

# 检查FLAMEGRAPH_DIR是否存在
if [ ! -d "$FLAMEGRAPH_DIR" ]; then
    echo "FlameGraph目录不存在，请先下载FlameGraph: https://github.com/brendangregg/FlameGraph.git"
    exit 1
fi

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 使用perf记录性能数据
# -g: 生成调用图（call graph）
# -F: 设置采样频率，单位是赫兹，默认值通常是 99，增大取样会消耗更多资源
# -o: 指定输出文件
perf record -F 199 -g  -- "./$1" "${@:2}"

# 将性能数据导出为一个可用于生成火焰图的格式
# perf script: 生成perf数据的可读格式
perf script > "$OUTPUT_DIR/perf_output.txt"

# 生成火焰图
# 使用 FlameGraph 提供的 stackcollapse-perf.pl 来生成折叠的堆栈数据
"$FLAMEGRAPH_DIR/stackcollapse-perf.pl" "$OUTPUT_DIR/perf_output.txt" > "$OUTPUT_DIR/collapsed_stack.txt"

# 使用 FlameGraph 提供的 flamegraph.pl 来生成火焰图
"$FLAMEGRAPH_DIR/flamegraph.pl" "$OUTPUT_DIR/collapsed_stack.txt" > "$OUTPUT_DIR/flamegraph.svg"

echo "火焰图已生成：$OUTPUT_DIR/flamegraph.svg"
