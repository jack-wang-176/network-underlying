#!/bin/bash

echo "开始编译..."
gcc src/*.c -o my_stack

if [ $? -ne 0 ]; then
   echo "编译失败"
   exit 1
fi
echo "编译成功，生成可执行文件my_stack"
echo "启动虚拟网卡"

sudo bash tools/setup_tap.sh
echo "运行可执行文件"
sudo ./my_stack
echo "已经退出程序，正在清理网卡"
sudo bash tools/setup_tap.sh down
echo "网卡清理完毕"