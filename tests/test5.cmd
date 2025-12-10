echo this tests globbing
mkdir ./test5 -p
cd ./test5
touch file1.txt
touch file2.txt
touch file3.tx
touch file4.out
ls *.txt
echo ------------
ls file*
echo ------------
ls *.tx | grep file | wc
echo I love seeing that file* has not left me, so please give me *.out
cd ..
rm -rd ./test5/
