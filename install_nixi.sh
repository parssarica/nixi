#/bin/bash

echo "Fetching GitHub repo"
git clone https://github.com/parssarica/nixi.git
cd nixi
echo "Compiling nixi"
make
sudo make install
cd ~
mkdir .nixi
cd .nixi
echo "Fetching databases"
wget https://raw.githubusercontent.com/parssarica/nixi/refs/heads/main/packages.db
wget https://raw.githubusercontent.com/parssarica/nixi/refs/heads/main/installed.db
