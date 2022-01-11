
###
 # @Author: leeloonki
 # @Bilibili: 李景芳_
 # @Date: 2022-01-10 11:08:32
 # @LastEditTime: 2022-01-10 11:33:12
 # @LastEditors: leeloonki
 # @Description: 
 # @FilePath: \code\readme.sh
### 

# 虚拟机和主机通过VMwaretools共享主机code文件夹
# 使用vscode编辑代码，在虚拟机中编译运行使用bochs调试

# 1.切换共享目录脚本

#! /bin/sh  
cd /mnt/hgfs/code
pwd
ls -l

# 执行./cd_codefolder.sh，sh执行后目录不变
# 执行source ./a. 将切换工作目录
source ./a.sh

# 2.编写流程
cd  # 回到用户主目录
source ./cd_codefolder.sh   # 切换到共享目录


