cd ../userland/
make -j
cd ../filesys/
bash cpeo.sh
./nachos -x shell -rs 100 -d f