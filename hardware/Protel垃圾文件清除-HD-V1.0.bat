::PROTEL垃圾文件删除器 
::版本 V1.3  20120625更新
::正点原子制作
::论坛:www.openedv.com 

del *.SchDocPreview /s
del *.PcbDocPreview /s 
del *.PrjPCBStructure /s 
del *.drc /s
del *.LOG /s
del *.htm /s  
del *.OutJob /s 

for /r /d %%b in (History) do rd "%%b" /s /q 
::删除当前目录下的所有History文件夹
for /r /d %%b in (Project?Logs?for*) do rd "%%b" /s /q 
::删除当前目录下的所有带字符串Project Logs for的文件夹
for /r /d %%b in (Project?Outputs?for*) do rd "%%b" /s /q   
::删除当前目录下的所有带字符串Project Outputs for的文件夹
exit


::::::::::::::::::::::::::::::::::::使用方法和解释::::::::::::::::::::::::::::::::::
::将:Protel垃圾文件清除V1.3.bat,拷贝到你的Protel项目文件内,然后运行即可.

::它将删除所有当前文件夹及当前文件夹所包含的文件夹里面的所有如下文件:
::1,".SchDocPreview"文件
::2,".PcbDocPreview"文件
::3,".PrjPCBStructure"文件
::4,".drc"文件
::5,".LOG"文件
::6,".htm"文件
::7,".OutJob"文件
::8,"History"文件夹
::9,所有带字符串"Project Logs for"的文件夹
::10,所有带字符串"Project Outputs for"的文件夹
