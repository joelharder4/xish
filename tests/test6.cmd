echo this tests background programs
echo testing? &
sleep 1
echo cleared.
echo this is a long string that could take a while to parse | sort | grep e | wc | sort &
sleep 1
echo ------------------
ls -1 /usr/include/*.h | grep std | more &
sleep 1
echo ------------------
ls | grep.c &
sleep 1
echo ------------------
sleep 1 | echo im done sleeping now
sleep 2
echo done